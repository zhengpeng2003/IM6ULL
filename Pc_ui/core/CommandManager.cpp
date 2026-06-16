#include "CommandManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <initializer_list>

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

static QString stringAny(const QJsonObject &obj, std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = obj.value(QString::fromLatin1(key)).toString();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return QString();
}

static qint64 int64Any(const QJsonObject &obj, std::initializer_list<const char *> keys, qint64 defaultValue = 0)
{
    for (const char *key : keys) {
        const QJsonValue value = obj.value(QString::fromLatin1(key));
        if (!value.isUndefined() && !value.isNull()) {
            return value.toVariant().toLongLong();
        }
    }
    return defaultValue;
}

static bool ackSuccess(const QJsonObject &obj)
{
    const QString status = obj.value(QStringLiteral("status")).toString();
    if (!status.isEmpty()) {
        return status == QStringLiteral("ok") || status == QStringLiteral("success");
    }
    return obj.value(QStringLiteral("ok")).toBool(false);
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
    obj["timestampMs"] = rec.timestamp;
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
    obj["deviceId"] = device.deviceId;
    obj["deviceType"] = rec.deviceType;
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
    obj.insert(QStringLiteral("timestampMs"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("target"), target);
    obj.insert(QStringLiteral("device"), device);
    obj.insert(QStringLiteral("deviceId"), deviceId);
    obj.insert(QStringLiteral("deviceType"), deviceType);
    obj.insert(QStringLiteral("slave_id"), deviceId);

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
    obj.insert(QStringLiteral("timestampMs"), rec.timestamp);
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
    QString cmdId = obj.value("cmd_id").toString();
    if (cmdId.isEmpty()) {
        const qint64 seq = int64Any(obj, {"seq", "sequence"});
        cmdId = m_seqToCmdId.value(seq);
    }
    if (!m_pending.contains(cmdId)) return;

    CommandRecord rec = m_pending.value(cmdId);
    rec.ok = ackSuccess(obj);
    const QString cmd = stringAny(obj, {"cmd", "commandType", "command"});
    if (!cmd.isEmpty()) {
        rec.command = cmd;
    }
    const QString reason = obj.value(QStringLiteral("reason")).toString();
    const QString rawMessage = obj.value(QStringLiteral("message")).toString();
    rec.reason = reason.isEmpty() ? rawMessage : reason;
    rec.ackTime = int64Any(obj, {"timestampMs", "timestamp"});

    const QString stage = obj.value("stage").toString();
    if (stage == QStringLiteral("sent") && rec.ok) {
        rec.state = QStringLiteral("running");
        m_pending.insert(cmdId, rec);
        emit commandStateChanged(cmdId, QStringLiteral("已发送，等待设备确认"));
        emit commandMessage(cmdId, rec.command, QStringLiteral("info"),
                            QStringLiteral("命令已发送"), QStringLiteral("等待设备确认"));
        return;
    }

    if (stage == QStringLiteral("done") && !rec.ok) {
        rec.state = QStringLiteral("failed");
        m_pending.insert(cmdId, rec);
        const QString message = rec.reason.isEmpty() ? QStringLiteral("failed") : QStringLiteral("failed: ") + rec.reason;
        emit commandMessage(cmdId, rec.command, QStringLiteral("error"),
                            QStringLiteral("命令执行失败"), friendlyCommandReason(rec.reason, rawMessage));
        finishCommand(cmdId, message);
        return;
    }

    if (stage == QStringLiteral("done") && rec.ok) {
        const QString msg = successMessageForCommand(rec.command);
        emit commandMessage(cmdId, rec.command, QStringLiteral("success"), msg, QString());
        finishCommand(cmdId, msg);
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
    const qint64 seq = int64Any(obj, {"seq", "sequence"});
    QString cmdId = obj.value(QStringLiteral("cmd_id")).toString();
    if (cmdId.isEmpty()) {
        cmdId = m_seqToCmdId.value(seq);
    }
    if (cmdId.isEmpty() || !m_pending.contains(cmdId)) return;

    const QString stage = obj.value(QStringLiteral("stage")).toString();
    const bool ok = ackSuccess(obj);
    const QString status = obj.value(QStringLiteral("status")).toString(ok ? QStringLiteral("success") : QStringLiteral("failed"));
    if (stage == QStringLiteral("sent")) {
        const CommandRecord rec = m_pending.value(cmdId);
        emit commandStateChanged(cmdId, QStringLiteral("已发送，等待设备确认"));
        emit commandMessage(cmdId, rec.command, QStringLiteral("info"),
                            QStringLiteral("命令已发送"), QStringLiteral("等待设备确认"));
        return;
    }
    if (stage == QStringLiteral("done") && ok) {
        const CommandRecord rec = m_pending.value(cmdId);
        if (rec.command == QStringLiteral("set_relay")) {
            emit commandMessage(cmdId, rec.command, QStringLiteral("success"),
                                QStringLiteral("写入成功，等待状态回读"), QString());
            finishCommand(cmdId, QStringLiteral("写入成功，等待状态回读"));
        } else {
            const QString msg = successMessageForCommand(rec.command);
            emit commandMessage(cmdId, rec.command, QStringLiteral("success"), msg, QString());
            finishCommand(cmdId, msg);
        }
    } else if (stage == QStringLiteral("done")) {
        const CommandRecord rec = m_pending.value(cmdId);
        const QString reason = obj.value(QStringLiteral("reason")).toString();
        const QString rawMessage = obj.value(QStringLiteral("message")).toString();
        const QString message = status == QStringLiteral("timeout")
            ? QStringLiteral("执行超时，请刷新状态")
            : friendlyCommandReason(reason, rawMessage);
        emit commandMessage(cmdId, rec.command, QStringLiteral("error"),
                            status == QStringLiteral("timeout") ? QStringLiteral("执行超时") : QStringLiteral("命令执行失败"),
                            message);
        finishCommand(cmdId, message);
    }
}

void CommandManager::startCommandTimeout(const QString &cmdId)
{
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [this, cmdId]() {
        if (!m_pending.contains(cmdId)) return;
        const CommandRecord rec = m_pending.value(cmdId);
        emit commandMessage(cmdId, rec.command, QStringLiteral("error"),
                            QStringLiteral("执行超时"),
                            QStringLiteral("未收到 Linux_data ACK，请检查板端服务、MQTT 或设备连接"));
        finishCommand(cmdId, QStringLiteral("执行超时：未收到 Linux_data ACK，请检查板端服务、MQTT 或设备连接"));
        emit commandTimeout(cmdId);
    });
    m_timeoutTimers.insert(cmdId, timer);
    timer->start(10000);
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


QString CommandManager::friendlyCommandReason(const QString &reason, const QString &message) const
{
    if (reason == QStringLiteral("invalid_request")) return QStringLiteral("请求参数错误");
    if (reason == QStringLiteral("port_not_connected")) return QStringLiteral("端口未连接");
    if (reason == QStringLiteral("port_not_found")) return QStringLiteral("端口不存在或未连接");
    if (reason == QStringLiteral("mqtt_publish_failed")) return QStringLiteral("MQTT 发送失败，请检查 Pc_data 与 Broker 连接");
    if (reason == QStringLiteral("linux_data_ack_timeout")) return QStringLiteral("板端执行超时，可能 Linux_data 未运行或 MQTT 不通");
    if (reason == QStringLiteral("delete_device_data_failed")) return QStringLiteral("PC 侧数据删除失败");
    if (reason == QStringLiteral("unsupported_command")) return QStringLiteral("不支持的命令");
    if (reason == QStringLiteral("unsupported_device_type")) return QStringLiteral("不支持的设备类型");
    if (reason == QStringLiteral("device_not_found")) return QStringLiteral("设备不存在或已被删除");
    if (reason == QStringLiteral("device_no_response")) return QStringLiteral("设备无响应，请检查从站地址、接线、波特率和设备供电");
    if (reason == QStringLiteral("slave_address_conflict")) return QStringLiteral("从站地址已存在");
    if (reason == QStringLiteral("config_write_failed")) return QStringLiteral("板端配置保存失败");
    if (reason == QStringLiteral("device_db_save_failed")) return QStringLiteral("板端已添加设备，但 PC 本地数据库保存失败");
    if (reason == QStringLiteral("db_not_open")) return QStringLiteral("数据库未打开");
    if (reason == QStringLiteral("timeout")) return QStringLiteral("执行超时");
    if (!message.isEmpty()) {
        return message;
    }
    return reason.isEmpty() ? QStringLiteral("未知错误") : reason;
}

QString CommandManager::successMessageForCommand(const QString &commandType) const
{
    if (commandType == QStringLiteral("remove_device")) return QStringLiteral("删除成功");
    if (commandType == QStringLiteral("add_device")) return QStringLiteral("添加成功");
    if (commandType == QStringLiteral("set_device_threshold") || commandType == QStringLiteral("set_threshold")) return QStringLiteral("阈值设置成功");
    if (commandType == QStringLiteral("set_relay")) return QStringLiteral("写入成功，等待状态回读");
    return QStringLiteral("命令执行成功");
}
