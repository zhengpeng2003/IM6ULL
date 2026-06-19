#pragma once
#include <QByteArray>
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include "model/DeviceModel.h"
#include "model/CommandModel.h"

class QTimer;

class CommandManager : public QObject
{
    Q_OBJECT
public:
    explicit CommandManager(QObject *parent = nullptr);

public slots:
    void sendRelayCommand(const DeviceNode &device, const QString &channel, bool value, const QMap<QString, bool> &currentStates = QMap<QString, bool>());
    void sendAddDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId,
                              const QString &deviceType, int pollIntervalMs);
    void sendRemoveDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId);
    void onCommandAck(const QJsonObject &obj);
    void onCommandLogUpdate(const QJsonObject &obj);

signals:
    void commandReadyForIpc(const QByteArray &payload);
    void commandReadyToPublish(const QString &topic, const QByteArray &payload, int qos, bool retain);
    void commandStateChanged(const QString &cmdId, const QString &state);
    void commandMessage(const QString &cmdId, const QString &commandType, const QString &level,
                        const QString &title, const QString &message);
    void commandTimeout(const QString &cmdId);

private:
    QString createCmdId() const;
    QString commandTopic(const DeviceNode &device) const;

private:
    void startCommandTimeout(const QString &cmdId);
    void finishCommand(const QString &cmdId, const QString &state);
    QString friendlyCommandReason(const QString &reason, const QString &message) const;
    QString successMessageForCommand(const QString &commandType) const;

    QHash<QString, CommandRecord> m_pending;
    QHash<QString, QTimer *> m_timeoutTimers;
    mutable quint64 m_cmdCounter = 0;
};
