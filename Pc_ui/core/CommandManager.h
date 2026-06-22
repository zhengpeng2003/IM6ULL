#pragma once
#include <QByteArray>
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QVariantMap>
#include "model/DeviceModel.h"
#include "model/CommandModel.h"

class QTimer;

class CommandManager : public QObject
{
    Q_OBJECT
public:
    explicit CommandManager(QObject *parent = nullptr);

    QSet<QString> pendingRelayChannels(const DeviceNode &device) const;
    bool isRelayChannelPending(const DeviceNode &device, const QString &channel) const;

public slots:
    void sendRelayCommand(const DeviceNode &device, const QString &channel, bool value, const QMap<QString, bool> &currentStates = QMap<QString, bool>());
    void sendAddDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId,
                              const QString &deviceType, int pollIntervalMs,
                              const QVariantMap &deviceOptions = QVariantMap());
    void sendRemoveDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId);
    void onCommandAck(const QJsonObject &obj);
    void onCommandLogUpdate(const QJsonObject &obj);

signals:
    void commandReadyForIpc(const QByteArray &payload);
    void commandReadyToPublish(const QString &topic, const QByteArray &payload, int qos, bool retain);
    void commandStateChanged(const QString &cmdId, const QString &state);
    void commandTargetStateChanged(const QString &cmdId, const QString &commandType,
                                   const QString &gatewayId, const QString &portId,
                                   int deviceId, const QString &state,
                                   const QString &reason, const QString &message);
    void commandMessage(const QString &cmdId, const QString &commandType, const QString &level,
                        const QString &title, const QString &message);
    void commandTimeout(const QString &cmdId);
    void relayPendingChanged();

private:
    struct RelayPendingInfo
    {
        QString deviceKey;
        QString channel;
        QString commandKey;
    };

    QString createCmdId() const;
    QString commandTopic(const DeviceNode &device) const;

private:
    void startCommandTimeout(const QString &cmdId);
    void finishCommand(const QString &cmdId, const QString &state);
    void clearRelayPending(const QString &cmdId);
    void emitTargetState(const CommandRecord &rec, const QString &state,
                         const QString &reason = QString(), const QString &message = QString());
    QString friendlyCommandReason(const QString &reason, const QString &message) const;
    QString successMessageForCommand(const QString &commandType) const;
    QString relayDeviceKey(const DeviceNode &device) const;
    QString relayCommandKey(const DeviceNode &device, const QString &channel) const;
    QString normalizeRelayChannel(const QString &channel) const;
    int relayChannelNumber(const QString &channel) const;
    int relaySlaveId(const DeviceNode &device) const;

    QHash<QString, CommandRecord> m_pending;
    QHash<QString, QTimer *> m_timeoutTimers;
    QHash<QString, RelayPendingInfo> m_pendingRelayByCmdId;
    QHash<QString, QString> m_pendingRelayCmdIdByKey;
    QHash<QString, QMap<QString, bool>> m_pendingRelayDesiredByDevice;
    mutable quint64 m_cmdCounter = 0;
};
