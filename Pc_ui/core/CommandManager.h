#pragma once
#include <QByteArray>
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include "model/DeviceModel.h"
#include "model/CommandModel.h"

class QTimer;

class CommandManager : public QObject
{
    Q_OBJECT
public:
    explicit CommandManager(QObject *parent = nullptr);

public slots:
    void sendRelayCommand(const DeviceNode &device, const QString &channel, bool value);
    void sendAddDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId,
                              const QString &deviceType, int pollIntervalMs);
    void onCommandAck(const QJsonObject &obj);

signals:
    void commandReadyForIpc(const QByteArray &payload);
    void commandReadyToPublish(const QString &topic, const QByteArray &payload, int qos, bool retain);
    void commandStateChanged(const QString &cmdId, const QString &state);
    void commandTimeout(const QString &cmdId);

private:
    QString createCmdId() const;
    QString commandTopic(const DeviceNode &device) const;

private:
    QHash<QString, CommandRecord> m_pending;
};
