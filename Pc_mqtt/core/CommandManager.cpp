#include "CommandManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

CommandManager::CommandManager(QObject *parent) : QObject(parent) {}

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

    QJsonObject params{{"channel", channel}, {"value", value}};
    rec.paramsJson = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));

    QJsonObject obj;
    obj["msg_type"] = "command";
    obj["version"] = 1;
    obj["cmd_id"] = rec.cmdId;
    obj["timestamp"] = rec.timestamp;
    obj["factory_id"] = rec.factoryId;
    obj["area_id"] = rec.areaId;
    obj["gateway_id"] = rec.gatewayId;
    obj["master_slot"] = rec.masterSlot;
    obj["slave_addr"] = rec.slaveAddr;
    obj["device_type"] = rec.deviceType;
    obj["command"] = rec.command;
    obj["params"] = params;

    m_pending.insert(rec.cmdId, rec);
    emit commandForDb(rec);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit commandReadyToPublish(commandTopic(device), QJsonDocument(obj).toJson(QJsonDocument::Compact), 1, false);
}

void CommandManager::onCommandAck(const QJsonObject &obj)
{
    const QString cmdId = obj.value("cmd_id").toString();
    if (!m_pending.contains(cmdId)) return;
    auto rec = m_pending.take(cmdId);
    rec.ok = obj.value("ok").toBool();
    rec.reason = obj.value("reason").toString();
    rec.ackTime = obj.value("timestamp").toVariant().toLongLong();
    rec.state = rec.ok ? "success" : "failed";
    emit commandForDb(rec);
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
