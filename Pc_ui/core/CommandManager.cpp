#include "CommandManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

CommandManager::CommandManager(QObject *parent) : QObject(parent) {}

static QString ipcRelayChannel(const QString &channel)
{
    if (channel == "led") {
        return "relay_1";
    }

    if (channel == "fan") {
        return "relay_2";
    }

    if (channel == "buzzer") {
        return "relay_3";
    }

    return channel;
}

void CommandManager::sendRelayCommand(const DeviceNode &device, const QString &channel, bool value)
{
    CommandRecord rec;
    rec.cmdId = createCmdId();
    rec.timestamp = QDateTime::currentSecsSinceEpoch();
    rec.factoryId = device.factoryId;
    rec.areaId = device.areaId;
    rec.gatewayId = device.gatewayId;
    rec.masterSlot = device.masterSlot;
    rec.slaveAddr = device.slaveAddr;
    rec.deviceType = device.deviceType;
    rec.command = "set_relay";
    rec.state = "pending";

    QJsonObject params{{"channel", ipcRelayChannel(channel)}, {"value", value}};
    rec.paramsJson = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));

    QJsonObject obj;
    obj["type"] = "command";
    obj["msg_type"] = "command";
    obj["version"] = 1;
    obj["cmd_id"] = rec.cmdId;
    obj["timestamp"] = rec.timestamp;
    obj["factory_id"] = rec.factoryId;
    obj["area_id"] = rec.areaId;
    obj["gateway_id"] = rec.gatewayId;
    obj["port_id"] = device.port;
    obj["master_slot"] = rec.masterSlot;
    obj["slave_addr"] = rec.slaveAddr;
    obj["device_id"] = device.deviceId;
    obj["device_type"] = rec.deviceType;
    obj["command"] = rec.command;
    obj["params"] = params;

    m_pending.insert(rec.cmdId, rec);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::sendAddDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId,
                                          const QString &deviceType, int pollIntervalMs)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0 || deviceType.isEmpty() || pollIntervalMs <= 0) {
        return;
    }

    CommandRecord rec;
    rec.cmdId = createCmdId();
    rec.timestamp = QDateTime::currentMSecsSinceEpoch();
    rec.gatewayId = gatewayId;
    rec.slaveAddr = deviceId;
    rec.deviceType = deviceType;
    rec.command = QStringLiteral("add_device");
    rec.state = QStringLiteral("pending");

    QJsonObject target;
    target.insert(QStringLiteral("gatewayId"), gatewayId);
    target.insert(QStringLiteral("portId"), portId);

    QJsonObject device;
    device.insert(QStringLiteral("deviceId"), deviceId);
    device.insert(QStringLiteral("slaveAddress"), deviceId);
    device.insert(QStringLiteral("deviceType"), deviceType);
    device.insert(QStringLiteral("pollIntervalMs"), pollIntervalMs);

    QJsonObject obj;
    obj.insert(QStringLiteral("type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("msg_type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("cmd_id"), rec.cmdId);
    obj.insert(QStringLiteral("seq"), rec.timestamp);
    obj.insert(QStringLiteral("timestamp"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("target"), target);
    obj.insert(QStringLiteral("device"), device);

    m_pending.insert(rec.cmdId, rec);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::sendRemoveDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    CommandRecord rec;
    rec.cmdId = createCmdId();
    rec.timestamp = QDateTime::currentMSecsSinceEpoch();
    rec.gatewayId = gatewayId;
    rec.slaveAddr = deviceId;
    rec.command = QStringLiteral("remove_device");
    rec.state = QStringLiteral("pending");

    QJsonObject target;
    target.insert(QStringLiteral("gatewayId"), gatewayId);
    target.insert(QStringLiteral("portId"), portId);

    QJsonObject obj;
    obj.insert(QStringLiteral("type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("msg_type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("cmd_id"), rec.cmdId);
    obj.insert(QStringLiteral("seq"), rec.timestamp);
    obj.insert(QStringLiteral("timestamp"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("deviceId"), deviceId);
    obj.insert(QStringLiteral("slave_id"), deviceId);
    obj.insert(QStringLiteral("target"), target);

    m_pending.insert(rec.cmdId, rec);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::onCommandAck(const QJsonObject &obj)
{
    const QString cmdId = obj.value("cmd_id").toString();
    if (!m_pending.contains(cmdId)) return;
    auto rec = m_pending.take(cmdId);
    rec.ok = obj.value("ok").toBool();
    rec.reason = obj.value("reason").toString(obj.value("message").toString());
    rec.ackTime = obj.value("timestamp").toVariant().toLongLong();
    rec.state = rec.ok ? "success" : "failed";
    emit commandStateChanged(cmdId, rec.state);
}

QString CommandManager::createCmdId() const
{
    return QString("CMD%1").arg(QDateTime::currentDateTime().toString("yyyyMMddHHmmsszzz"));
}

QString CommandManager::commandTopic(const DeviceNode &device) const
{
    return QString("factory/%1/area/%2/gateway/%3/command")
        .arg(device.factoryId, device.areaId, device.gatewayId);
}
