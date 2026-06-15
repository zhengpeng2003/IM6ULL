#include "CommandManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

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

void CommandManager::sendRelayCommand(const DeviceNode &device, const QString &channel, bool value, const QMap<QString, bool> &currentStates)
{
    CommandRecord rec;
    rec.cmdId = createCmdId();
    rec.timestamp = QDateTime::currentMSecsSinceEpoch();
    rec.seq = rec.timestamp;
    rec.factoryId = device.factoryId;
    rec.areaId = device.areaId;
    rec.gatewayId = device.gatewayId;
    rec.masterSlot = device.masterSlot;
    rec.slaveAddr = device.slaveAddr;
    rec.deviceType = device.deviceType;
    rec.command = "set_relay";
    rec.state = "pending";

    const QString relayChannel = ipcRelayChannel(channel);
    if (currentStates.isEmpty() || !currentStates.contains(relayChannel)) {
        emit commandStateChanged(rec.cmdId, QStringLiteral("failed: 当前继电器状态未知，请刷新后再操作"));
        return;
    }

    QJsonArray states;
    for (int i = 1; i <= 4; ++i) {
        const QString key = QStringLiteral("relay_%1").arg(i);
        states.append(key == relayChannel ? value : currentStates.value(key, false));
    }

    QJsonObject params{{"channel", relayChannel}, {"value", value}, {"states", states}};
    rec.paramsJson = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));

    QJsonObject obj;
    obj["type"] = "command";
    obj["msg_type"] = "command";
    obj["version"] = 1;
    obj["cmd_id"] = rec.cmdId;
    obj["timestamp"] = rec.timestamp;
    obj["seq"] = rec.seq;
    obj["cmd"] = rec.command;
    obj["commandType"] = rec.command;
    obj["factory_id"] = rec.factoryId;
    obj["area_id"] = rec.areaId;
    QJsonObject target;
    target["gatewayId"] = rec.gatewayId;
    target["portId"] = device.port;
    obj["target"] = target;
    obj["gatewayId"] = rec.gatewayId;
    obj["portId"] = device.port;
    obj["gateway_id"] = rec.gatewayId;
    obj["port_id"] = device.port;
    obj["slot"] = rec.masterSlot;
    obj["slave_id"] = rec.slaveAddr > 0 ? rec.slaveAddr : device.deviceId;
    obj["master_slot"] = rec.masterSlot;
    obj["slave_addr"] = rec.slaveAddr;
    obj["device_id"] = device.deviceId;
    obj["device_type"] = rec.deviceType;
    obj["states"] = states;
    obj["command"] = rec.command;
    obj["params"] = params;

    m_pending.insert(rec.cmdId, rec);
    m_seqToCmdId.insert(rec.seq, rec.cmdId);
    startCommandTimeout(rec.cmdId);
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
    rec.seq = rec.timestamp;
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
    obj.insert(QStringLiteral("seq"), rec.seq);
    obj.insert(QStringLiteral("timestamp"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("target"), target);
    obj.insert(QStringLiteral("device"), device);

    m_pending.insert(rec.cmdId, rec);
    m_seqToCmdId.insert(rec.seq, rec.cmdId);
    startCommandTimeout(rec.cmdId);
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
    rec.seq = rec.timestamp;
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
    obj.insert(QStringLiteral("seq"), rec.seq);
    obj.insert(QStringLiteral("timestamp"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("deviceId"), deviceId);
    obj.insert(QStringLiteral("slave_id"), deviceId);
    obj.insert(QStringLiteral("target"), target);

    m_pending.insert(rec.cmdId, rec);
    m_seqToCmdId.insert(rec.seq, rec.cmdId);
    startCommandTimeout(rec.cmdId);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::onCommandAck(const QJsonObject &obj)
{
    const QString cmdId = obj.value("cmd_id").toString();
    if (!m_pending.contains(cmdId)) return;

    CommandRecord rec = m_pending.value(cmdId);
    rec.ok = obj.value("ok").toBool();
    rec.reason = obj.value("message").toString(obj.value("reason").toString());
    rec.ackTime = obj.value("timestamp").toVariant().toLongLong();

    const QString stage = obj.value("stage").toString();
    if (stage == QStringLiteral("sent") && rec.ok) {
        rec.state = QStringLiteral("running");
        m_pending.insert(cmdId, rec);
        emit commandStateChanged(cmdId, QStringLiteral("已发送，等待设备确认"));
        return;
    }

    if (stage == QStringLiteral("done") && !rec.ok) {
        rec.state = QStringLiteral("failed");
        m_pending.insert(cmdId, rec);
        const QString message = rec.reason.isEmpty() ? QStringLiteral("failed") : QStringLiteral("failed: ") + rec.reason;
        finishCommand(cmdId, message);
        return;
    }

    rec.state = rec.ok ? QStringLiteral("running") : QStringLiteral("failed");
    m_pending.insert(cmdId, rec);
    if (rec.ok) {
        emit commandStateChanged(cmdId, rec.state);
    } else {
        finishCommand(cmdId, rec.state);
    }
}

void CommandManager::onCommandLogUpdate(const QJsonObject &obj)
{
    const qint64 seq = obj.value(QStringLiteral("seq")).toVariant().toLongLong();
    const QString cmdId = m_seqToCmdId.value(seq);
    if (cmdId.isEmpty() || !m_pending.contains(cmdId)) return;

    const QString stage = obj.value(QStringLiteral("stage")).toString();
    const QString status = obj.value(QStringLiteral("status")).toString();
    if (stage == QStringLiteral("done") && status == QStringLiteral("success")) {
        const CommandRecord rec = m_pending.value(cmdId);
        if (rec.command == QStringLiteral("set_relay")) {
            finishCommand(cmdId, QStringLiteral("写入成功，等待状态回读"));
        } else {
            finishCommand(cmdId, QStringLiteral("success"));
        }
    } else if (stage == QStringLiteral("done")) {
        const QString message = obj.value(QStringLiteral("message")).toString(obj.value(QStringLiteral("reason")).toString());
        finishCommand(cmdId, status == QStringLiteral("timeout") ? QStringLiteral("执行超时，请刷新状态") : QStringLiteral("failed: ") + message);
    }
}

void CommandManager::startCommandTimeout(const QString &cmdId)
{
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, cmdId]() {
        if (!m_pending.contains(cmdId)) return;
        finishCommand(cmdId, QStringLiteral("执行超时，请刷新状态"));
        emit commandTimeout(cmdId);
    });
    m_timeoutTimers.insert(cmdId, timer);
    timer->start(5000);
}

void CommandManager::finishCommand(const QString &cmdId, const QString &state)
{
    const CommandRecord rec = m_pending.take(cmdId);
    if (rec.seq > 0) {
        m_seqToCmdId.remove(rec.seq);
    }
    if (QTimer *timer = m_timeoutTimers.take(cmdId)) {
        timer->stop();
        timer->deleteLater();
    }
    emit commandStateChanged(cmdId, state);
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
