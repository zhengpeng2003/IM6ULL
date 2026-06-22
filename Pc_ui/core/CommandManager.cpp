#include "CommandManager.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <QVariantMap>
#include <QtGlobal>
#include <initializer_list>

CommandManager::CommandManager(QObject *parent) : QObject(parent) {}

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
    const QString stage = obj.value(QStringLiteral("stage")).toString();
    if (stage != QStringLiteral("done")) {
        return false;
    }
    const QString status = obj.value(QStringLiteral("status")).toString();
    if (!status.isEmpty()) {
        return status == QStringLiteral("success") && obj.value(QStringLiteral("ok")).toBool(false);
    }
    return obj.value(QStringLiteral("ok")).toBool(false);
}

QSet<QString> CommandManager::pendingRelayChannels(const DeviceNode &device) const
{
    const QString deviceKey = relayDeviceKey(device);
    QSet<QString> result;
    for (const RelayPendingInfo &info : m_pendingRelayByCmdId) {
        if (info.deviceKey == deviceKey) {
            result.insert(info.channel);
        }
    }
    return result;
}

bool CommandManager::isRelayChannelPending(const DeviceNode &device, const QString &channel) const
{
    return m_pendingRelayCmdIdByKey.contains(relayCommandKey(device, normalizeRelayChannel(channel)));
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
    rec.portId = device.port;
    rec.masterSlot = device.masterSlot;
    rec.slaveAddr = device.slaveAddr;
    rec.deviceType = device.deviceType;
    rec.command = "set_relay";
    rec.state = "pending";

    const QString relayChannel = normalizeRelayChannel(channel);
    const QString relayKey = relayCommandKey(device, relayChannel);
    if (relayChannel.isEmpty() || relayChannelNumber(relayChannel) <= 0) {
        emit commandMessage(rec.cmdId, rec.command, QStringLiteral("error"),
                            QStringLiteral("继电器命令未发送"),
                            QStringLiteral("未知继电器通道"));
        emit commandStateChanged(rec.cmdId, QStringLiteral("failed: 未知继电器通道"));
        return;
    }
    if (m_pendingRelayCmdIdByKey.contains(relayKey)) {
        emit commandMessage(m_pendingRelayCmdIdByKey.value(relayKey),
                            rec.command,
                            QStringLiteral("warning"),
                            QStringLiteral("继电器通道命令正在执行"),
                            QStringLiteral("该继电器通道命令正在执行，请等待回包"));
        return;
    }
    QMap<QString, bool> normalizedCurrentStates;
    for (auto it = currentStates.cbegin(); it != currentStates.cend(); ++it) {
        const QString key = normalizeRelayChannel(it.key());
        if (!key.isEmpty()) {
            normalizedCurrentStates.insert(key, it.value());
        }
    }

    if (normalizedCurrentStates.isEmpty() || !normalizedCurrentStates.contains(relayChannel)) {
        emit commandMessage(rec.cmdId, rec.command, QStringLiteral("error"),
                            QStringLiteral("继电器命令未发送"),
                            QStringLiteral("当前继电器状态未知，请刷新后再操作"));
        emit commandStateChanged(rec.cmdId, QStringLiteral("failed: 当前继电器状态未知，请刷新后再操作"));
        return;
    }

    const QString deviceKey = relayDeviceKey(device);
    QMap<QString, bool> desiredStates = normalizedCurrentStates;
    const QMap<QString, bool> pendingDesired = m_pendingRelayDesiredByDevice.value(deviceKey);
    for (auto it = pendingDesired.cbegin(); it != pendingDesired.cend(); ++it) {
        desiredStates.insert(it.key(), it.value());
    }
    desiredStates.insert(relayChannel, value);

    int channelCount = relayChannelNumber(relayChannel);
    for (auto it = desiredStates.cbegin(); it != desiredStates.cend(); ++it) {
        channelCount = qMax(channelCount, relayChannelNumber(it.key()));
    }

    QJsonArray states;
    for (int i = 1; i <= channelCount; ++i) {
        const QString key = QStringLiteral("relay_%1").arg(i);
        states.append(desiredStates.value(key, false));
    }

    const int relaySlave = relaySlaveId(device);
    QJsonObject params{{"channel", relayChannel}, {"value", value}, {"states", states}};
    rec.paramsJson = QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));

    QJsonObject payload = params;
    payload["slot"] = rec.masterSlot;
    payload["master_slot"] = rec.masterSlot;
    payload["slave_id"] = relaySlave;
    payload["deviceId"] = relaySlave;
    payload["device_id"] = relaySlave;
    payload["slave_addr"] = relaySlave;

    QJsonObject obj;
    obj["type"] = "command";
    obj["msg_type"] = "command";
    obj["version"] = 1;
    obj["cmd_id"] = rec.cmdId;
    obj["timestamp"] = rec.timestamp;
    obj["timestampMs"] = rec.timestamp;
    obj["seq"] = rec.seq;
    obj["cmd"] = rec.command;
    obj["cmdType"] = rec.command;
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
    obj["slave_id"] = relaySlave;
    obj["master_slot"] = rec.masterSlot;
    obj["slave_addr"] = rec.slaveAddr;
    obj["device_id"] = device.deviceId;
    obj["device_type"] = rec.deviceType;
    obj["states"] = states;
    obj["command"] = rec.command;
    obj["params"] = params;
    obj["payload"] = payload;

    m_pending.insert(rec.cmdId, rec);
    RelayPendingInfo relayPending;
    relayPending.deviceKey = deviceKey;
    relayPending.channel = relayChannel;
    relayPending.commandKey = relayKey;
    m_pendingRelayByCmdId.insert(rec.cmdId, relayPending);
    m_pendingRelayCmdIdByKey.insert(relayKey, rec.cmdId);
    m_pendingRelayDesiredByDevice[deviceKey].insert(relayChannel, value);
    startCommandTimeout(rec.cmdId);
    emit commandStateChanged(rec.cmdId, rec.state);
    emit relayPendingChanged();
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::sendAddDeviceCommand(const QString &gatewayId, const QString &portId, int deviceId,
                                          const QString &deviceType, int pollIntervalMs,
                                          const QVariantMap &deviceOptions)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0 || deviceType.isEmpty() || pollIntervalMs <= 0) {
        return;
    }

    CommandRecord rec;
    rec.cmdId = createCmdId();
    rec.timestamp = QDateTime::currentMSecsSinceEpoch();
    rec.seq = rec.timestamp;
    rec.gatewayId = gatewayId;
    rec.portId = portId;
    rec.slaveAddr = deviceId;
    rec.deviceType = deviceType;
    rec.command = QStringLiteral("add_device");
    rec.state = QStringLiteral("pending");

    QJsonObject target;
    target.insert(QStringLiteral("gatewayId"), gatewayId);
    target.insert(QStringLiteral("portId"), portId);

    QJsonObject device;
    device.insert(QStringLiteral("slave_id"), deviceId);
    device.insert(QStringLiteral("deviceId"), deviceId);
    device.insert(QStringLiteral("slaveAddress"), deviceId);
    device.insert(QStringLiteral("device_type"), deviceType);
    device.insert(QStringLiteral("deviceType"), deviceType);
    device.insert(QStringLiteral("poll_interval_ms"), pollIntervalMs);
    device.insert(QStringLiteral("pollIntervalMs"), pollIntervalMs);
    if (!deviceOptions.isEmpty()) {
        device.insert(QStringLiteral("device_options"), QJsonObject::fromVariantMap(deviceOptions));
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("slave_id"), deviceId);
    payload.insert(QStringLiteral("deviceId"), deviceId);
    payload.insert(QStringLiteral("slaveAddress"), deviceId);
    payload.insert(QStringLiteral("device_type"), deviceType);
    payload.insert(QStringLiteral("deviceType"), deviceType);
    payload.insert(QStringLiteral("poll_interval_ms"), pollIntervalMs);
    payload.insert(QStringLiteral("pollIntervalMs"), pollIntervalMs);

    QJsonObject obj;
    obj.insert(QStringLiteral("type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("msg_type"), QStringLiteral("command"));
    obj.insert(QStringLiteral("version"), 1);
    obj.insert(QStringLiteral("cmd_id"), rec.cmdId);
    obj.insert(QStringLiteral("seq"), rec.seq);
    obj.insert(QStringLiteral("timestamp"), rec.timestamp);
    obj.insert(QStringLiteral("timestampMs"), rec.timestamp);
    obj.insert(QStringLiteral("commandType"), rec.command);
    obj.insert(QStringLiteral("cmdType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("target"), target);
    obj.insert(QStringLiteral("device"), device);
    obj.insert(QStringLiteral("deviceId"), deviceId);
    obj.insert(QStringLiteral("deviceType"), deviceType);
    obj.insert(QStringLiteral("slave_id"), deviceId);
    obj.insert(QStringLiteral("payload"), payload);

    m_pending.insert(rec.cmdId, rec);
    startCommandTimeout(rec.cmdId);
    emit commandStateChanged(rec.cmdId, rec.state);
    emitTargetState(rec,
                    QStringLiteral("消息正在发送"),
                    QString(),
                    QStringLiteral("正在添加从站 %1").arg(deviceId));
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
    rec.portId = portId;
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
    obj.insert(QStringLiteral("cmdType"), rec.command);
    obj.insert(QStringLiteral("cmd"), rec.command);
    obj.insert(QStringLiteral("gatewayId"), gatewayId);
    obj.insert(QStringLiteral("portId"), portId);
    obj.insert(QStringLiteral("deviceId"), deviceId);
    obj.insert(QStringLiteral("slave_id"), deviceId);
    obj.insert(QStringLiteral("target"), target);
    QJsonObject payload;
    payload.insert(QStringLiteral("deviceId"), deviceId);
    payload.insert(QStringLiteral("slave_id"), deviceId);
    obj.insert(QStringLiteral("payload"), payload);

    m_pending.insert(rec.cmdId, rec);
    startCommandTimeout(rec.cmdId);
    emit commandStateChanged(rec.cmdId, rec.state);
    emitTargetState(rec,
                    QStringLiteral("消息正在发送"),
                    QString(),
                    QStringLiteral("正在删除从站 %1").arg(deviceId));
    emit commandReadyForIpc(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void CommandManager::onCommandAck(const QJsonObject &obj)
{
    QString cmdId = obj.value("cmd_id").toString();
    const QString cmd = stringAny(obj, {"cmd", "commandType", "command"});
    const QString stage = obj.value("stage").toString();
    const QString status = obj.value(QStringLiteral("status")).toString();
    const bool ok = ackSuccess(obj);
    const bool pendingHit = !cmdId.isEmpty() && m_pending.contains(cmdId);
    qDebug() << "Pc_ui command_ack"
             << "cmd_id=" << cmdId
             << "cmd=" << cmd
             << "stage=" << stage
             << "status=" << status
             << "ok=" << ok
             << "pendingHit=" << pendingHit;

    if (cmdId.isEmpty()) {
        return;
    }
    if (!pendingHit) return;

    CommandRecord rec = m_pending.value(cmdId);
    rec.ok = ok;
    if (!cmd.isEmpty()) {
        rec.command = cmd;
    }
    const QString reason = obj.value(QStringLiteral("reason")).toString();
    const QString rawMessage = obj.value(QStringLiteral("message")).toString();
    rec.reason = reason.isEmpty() ? rawMessage : reason;
    rec.ackTime = int64Any(obj, {"timestampMs", "timestamp"});

    if (stage == QStringLiteral("done") && status == QStringLiteral("success") && ok) {
        const QString msg = successMessageForCommand(rec.command);
        emitTargetState(rec,
                        rec.command == QStringLiteral("add_device")
                            ? QStringLiteral("添加成功，正在刷新设备列表")
                            : (rec.command == QStringLiteral("remove_device")
                                ? QStringLiteral("删除成功，正在刷新设备列表")
                                : msg),
                        QString(),
                        msg);
        emit commandMessage(cmdId, rec.command, QStringLiteral("success"), msg, QString());
        finishCommand(cmdId, msg);
        return;
    }

    if (stage == QStringLiteral("sent") && rec.ok) {
        rec.ok = false;
        rec.state = QStringLiteral("running");
        m_pending.insert(cmdId, rec);
        emit commandStateChanged(cmdId, QStringLiteral("已发送，等待设备确认"));
        emitTargetState(rec,
                        QStringLiteral("消息发送成功，等待板端回应"),
                        QString(),
                        QStringLiteral("等待板端回应"));
        emit commandMessage(cmdId, rec.command, QStringLiteral("info"),
                            QStringLiteral("命令已发送"), QStringLiteral("等待设备确认"));
        return;
    }

    if (stage == QStringLiteral("done") && !rec.ok) {
        rec.state = QStringLiteral("failed");
        m_pending.insert(cmdId, rec);
        const QString message = rec.reason.isEmpty() ? QStringLiteral("failed") : QStringLiteral("failed: ") + rec.reason;
        emitTargetState(rec,
                        rec.command == QStringLiteral("add_device")
                            ? QStringLiteral("添加从站失败")
                            : (rec.command == QStringLiteral("remove_device")
                                ? QStringLiteral("删除从站失败")
                                : QStringLiteral("命令执行失败")),
                        friendlyCommandReason(rec.reason, rawMessage),
                        rawMessage);
        emit commandMessage(cmdId, rec.command, QStringLiteral("error"),
                            QStringLiteral("命令执行失败"), friendlyCommandReason(rec.reason, rawMessage));
        finishCommand(cmdId, message);
        return;
    }

    if (stage == QStringLiteral("done") && rec.ok) {
        const QString msg = successMessageForCommand(rec.command);
        emitTargetState(rec,
                        rec.command == QStringLiteral("add_device")
                            ? QStringLiteral("添加成功，正在刷新设备列表")
                            : (rec.command == QStringLiteral("remove_device")
                                ? QStringLiteral("删除成功，正在刷新设备列表")
                                : msg),
                        QString(),
                        msg);
        emit commandMessage(cmdId, rec.command, QStringLiteral("success"), msg, QString());
        finishCommand(cmdId, msg);
        return;
    }

    rec.state = stage == QStringLiteral("sent") || stage == QStringLiteral("accepted")
        ? QStringLiteral("running")
        : (rec.ok ? QStringLiteral("running") : QStringLiteral("failed"));
    m_pending.insert(cmdId, rec);
    if (rec.ok || stage == QStringLiteral("sent") || stage == QStringLiteral("accepted")) {
        emit commandStateChanged(cmdId, rec.state);
        emitTargetState(rec,
                        QStringLiteral("消息发送成功，等待板端回应"),
                        QString(),
                        QStringLiteral("等待板端回应"));
    } else {
        emitTargetState(rec,
                        rec.command == QStringLiteral("add_device")
                            ? QStringLiteral("添加从站失败")
                            : (rec.command == QStringLiteral("remove_device")
                                ? QStringLiteral("删除从站失败")
                                : QStringLiteral("命令执行失败")),
                        friendlyCommandReason(rec.reason, rawMessage),
                        rawMessage);
        finishCommand(cmdId, rec.state);
    }
}

void CommandManager::onCommandLogUpdate(const QJsonObject &obj)
{
    QString cmdId = obj.value(QStringLiteral("cmd_id")).toString();
    if (cmdId.isEmpty()) {
        return;
    }
    if (cmdId.isEmpty() || !m_pending.contains(cmdId)) return;

    const QString stage = obj.value(QStringLiteral("stage")).toString();
    const bool ok = ackSuccess(obj);
    const QString status = obj.value(QStringLiteral("status")).toString(ok ? QStringLiteral("success") : QStringLiteral("failed"));
    if (stage == QStringLiteral("sent")) {
        const CommandRecord rec = m_pending.value(cmdId);
        emit commandStateChanged(cmdId, QStringLiteral("已发送，等待设备确认"));
        emitTargetState(rec,
                        QStringLiteral("消息发送成功，等待板端回应"),
                        QString(),
                        QStringLiteral("等待板端回应"));
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
            emitTargetState(rec,
                            rec.command == QStringLiteral("add_device")
                                ? QStringLiteral("添加成功，正在刷新设备列表")
                                : (rec.command == QStringLiteral("remove_device")
                                    ? QStringLiteral("删除成功，正在刷新设备列表")
                                    : msg),
                            QString(),
                            msg);
            emit commandMessage(cmdId, rec.command, QStringLiteral("success"), msg, QString());
            finishCommand(cmdId, msg);
        }
    } else if (stage == QStringLiteral("done")) {
        CommandRecord rec = m_pending.value(cmdId);
        const QString reason = obj.value(QStringLiteral("reason")).toString();
        const QString rawMessage = obj.value(QStringLiteral("message")).toString();
        if (status == QStringLiteral("timeout")) {
            rec.state = QStringLiteral("timeout_waiting_ack");
            rec.reason = reason.isEmpty() ? rawMessage : reason;
            m_pending.insert(cmdId, rec);
            emit commandStateChanged(cmdId, QStringLiteral("Pc_data 等待 Linux_data ACK 超时，继续等待最终确认"));
            emitTargetState(rec,
                            QStringLiteral("等待 Linux_data 最终 ACK"),
                            QString(),
                            QStringLiteral("Pc_data 暂未收到 Linux_data ACK，仍在等待最终结果"));
            emit commandMessage(cmdId, rec.command, QStringLiteral("warning"),
                                QStringLiteral("等待板端确认"),
                                QStringLiteral("Pc_data 暂未收到 Linux_data ACK，仍在等待最终结果"));
            return;
        }
        const QString message = status == QStringLiteral("timeout")
            ? QStringLiteral("执行超时，请刷新状态")
            : friendlyCommandReason(reason, rawMessage);
        emit commandMessage(cmdId, rec.command, QStringLiteral("error"),
                            status == QStringLiteral("timeout") ? QStringLiteral("执行超时") : QStringLiteral("命令执行失败"),
                            message);
        emitTargetState(rec,
                        rec.command == QStringLiteral("add_device")
                            ? QStringLiteral("添加从站失败")
                            : (rec.command == QStringLiteral("remove_device")
                                ? QStringLiteral("删除从站失败")
                                : QStringLiteral("命令执行失败")),
                        message,
                        rawMessage);
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
        emit commandMessage(cmdId, rec.command, QStringLiteral("warning"),
                            QStringLiteral("等待板端确认"),
                            QStringLiteral("暂未收到 Linux_data 最终 ACK，命令仍在等待确认"));
        emit commandStateChanged(cmdId, QStringLiteral("等待 Linux_data 最终 ACK"));
        emitTargetState(rec,
                        QStringLiteral("等待 Linux_data 最终 ACK"),
                        QString(),
                        QStringLiteral("暂未收到 Linux_data 最终 ACK，命令仍在等待确认"));
        emit commandTimeout(cmdId);
    });
    m_timeoutTimers.insert(cmdId, timer);
    timer->start(20000);
}

void CommandManager::finishCommand(const QString &cmdId, const QString &state)
{
    const CommandRecord rec = m_pending.take(cmdId);
    if (QTimer *timer = m_timeoutTimers.take(cmdId)) {
        timer->stop();
        timer->deleteLater();
    }
    clearRelayPending(cmdId);
    emit commandStateChanged(cmdId, state);
}

void CommandManager::clearRelayPending(const QString &cmdId)
{
    if (!m_pendingRelayByCmdId.contains(cmdId)) {
        return;
    }

    const RelayPendingInfo info = m_pendingRelayByCmdId.take(cmdId);
    m_pendingRelayCmdIdByKey.remove(info.commandKey);
    if (m_pendingRelayDesiredByDevice.contains(info.deviceKey)) {
        QMap<QString, bool> desired = m_pendingRelayDesiredByDevice.value(info.deviceKey);
        desired.remove(info.channel);
        if (desired.isEmpty()) {
            m_pendingRelayDesiredByDevice.remove(info.deviceKey);
        } else {
            m_pendingRelayDesiredByDevice.insert(info.deviceKey, desired);
        }
    }
    emit relayPendingChanged();
}

void CommandManager::emitTargetState(const CommandRecord &rec, const QString &state,
                                     const QString &reason, const QString &message)
{
    if (rec.command != QStringLiteral("add_device") &&
        rec.command != QStringLiteral("remove_device")) {
        return;
    }

    emit commandTargetStateChanged(rec.cmdId,
                                   rec.command,
                                   rec.gatewayId,
                                   rec.portId,
                                   rec.slaveAddr,
                                   state,
                                   reason,
                                   message);
}

QString CommandManager::createCmdId() const
{
    const quint64 counter = ++m_cmdCounter;
    return QStringLiteral("CMD%1_%2")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmsszzz")))
        .arg(counter);
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
    if (reason == QStringLiteral("device_exists")) return QStringLiteral("从站地址已存在");
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

QString CommandManager::relayDeviceKey(const DeviceNode &device) const
{
    return QStringLiteral("%1|%2|%3")
        .arg(device.gatewayId, device.port)
        .arg(relaySlaveId(device));
}

QString CommandManager::relayCommandKey(const DeviceNode &device, const QString &channel) const
{
    return relayDeviceKey(device) + QStringLiteral("|") + normalizeRelayChannel(channel);
}

QString CommandManager::normalizeRelayChannel(const QString &channel) const
{
    if (channel == QStringLiteral("led")) {
        return QStringLiteral("relay_1");
    }
    if (channel == QStringLiteral("fan")) {
        return QStringLiteral("relay_2");
    }
    if (channel == QStringLiteral("buzzer")) {
        return QStringLiteral("relay_3");
    }

    static const QRegularExpression relayDotChannel(QStringLiteral("^relay\\.ch(\\d+)$"));
    const QRegularExpressionMatch dotMatch = relayDotChannel.match(channel);
    if (dotMatch.hasMatch()) {
        return QStringLiteral("relay_%1").arg(dotMatch.captured(1));
    }

    static const QRegularExpression relayUnderscoreChannel(QStringLiteral("^relay_(\\d+)$"));
    const QRegularExpressionMatch underscoreMatch = relayUnderscoreChannel.match(channel);
    if (underscoreMatch.hasMatch()) {
        return channel;
    }

    return channel;
}

int CommandManager::relayChannelNumber(const QString &channel) const
{
    static const QRegularExpression relayNumber(QStringLiteral("^(?:relay_|relay\\.ch)(\\d+)$"));
    const QRegularExpressionMatch match = relayNumber.match(channel);
    if (!match.hasMatch()) {
        return 0;
    }
    return match.captured(1).toInt();
}

int CommandManager::relaySlaveId(const DeviceNode &device) const
{
    return device.slaveAddr > 0 ? device.slaveAddr : device.deviceId;
}
