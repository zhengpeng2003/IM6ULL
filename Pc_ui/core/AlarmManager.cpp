#include "AlarmManager.h"
#include <QDateTime>
#include <QRegularExpression>

namespace {

QString stringValue(const QJsonObject &obj, const char *snakeKey, const char *camelKey = nullptr)
{
    QString value = obj.value(QLatin1String(snakeKey)).toString();
    if (value.isEmpty() && camelKey) {
        value = obj.value(QLatin1String(camelKey)).toString();
    }
    return value;
}

qint64 int64Value(const QJsonObject &obj, const char *key, qint64 defaultValue = 0)
{
    const QJsonValue value = obj.value(QLatin1String(key));
    return value.isDouble() ? static_cast<qint64>(value.toDouble()) : defaultValue;
}

int masterSlotFromPortId(const QString &portId)
{
    static const QRegularExpression re("^(?:port_|rs485-|RS485-)?(\\d+)$");
    const QRegularExpressionMatch match = re.match(portId);
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;
    const int portNumber = match.captured(1).toInt(&ok);
    if (!ok || portNumber <= 0) {
        return -1;
    }

    return portNumber - 1;
}

} // namespace

AlarmManager::AlarmManager(QObject *parent) : QObject(parent) {}

QList<AlarmRecord> AlarmManager::alarms() const { return m_alarms.values(); }
QList<AlarmRecord> AlarmManager::latestAlarms(int limit) const { return m_alarms.values().mid(0, limit); }

int AlarmManager::activeAlarmCount() const
{
    int count = 0;
    for (const auto &a : m_alarms) if (a.state == "active") ++count;
    return count;
}

void AlarmManager::onAlarmMessage(const QJsonObject &obj)
{
    AlarmRecord a;
    a.alarmId = stringValue(obj, "alarm_id", "alarmId");
    a.factoryId = stringValue(obj, "factory_id", "factoryId");
    a.areaId = stringValue(obj, "area_id", "areaId");
    a.areaName = stringValue(obj, "area_name", "areaName");
    a.gatewayId = stringValue(obj, "gateway_id", "gatewayId");
    a.portId = stringValue(obj, "port_id", "portId");
    a.masterSlot = obj.value("master_slot").toInt(masterSlotFromPortId(a.portId));
    a.slaveAddr = obj.value("slave_addr").toInt(obj.value("deviceId").toInt(obj.value("device_id").toInt()));
    a.deviceName = stringValue(obj, "device_name", "deviceName");
    a.deviceType = stringValue(obj, "device_type", "deviceType");
    a.pointKey = stringValue(obj, "point_key", "pointKey");
    a.alarmType = stringValue(obj, "alarm_type", "alarmType");
    a.level = obj.value("level").toString(obj.value("alarm_level").toString("warning"));
    a.message = obj.value("message").toString(obj.value("alarm_message").toString());
    a.value = obj.value("value").toDouble(obj.value("trigger_value").toDouble());
    a.threshold = obj.value("threshold").toDouble(obj.value("threshold_value").toDouble());
    a.state = obj.value("state").toString(obj.value("status").toString("active"));
    if (a.state == "acked") {
        a.state = "acknowledged";
    }
    a.startTime = int64Value(obj, "timestampMs", int64Value(obj, "timestamp", QDateTime::currentMSecsSinceEpoch()));
    if (a.alarmId.isEmpty()) {
        a.alarmId = QStringLiteral("%1.%2.%3.%4.%5")
            .arg(a.gatewayId.isEmpty() ? QStringLiteral("unknown_gateway") : a.gatewayId,
                 a.portId.isEmpty() ? QStringLiteral("unknown_port") : a.portId)
            .arg(a.slaveAddr)
            .arg(a.pointKey.isEmpty() ? QStringLiteral("unknown") : a.pointKey,
                 a.alarmType.isEmpty() ? QStringLiteral("emergency") : a.alarmType);
    }

    const bool existed = m_alarms.contains(a.alarmId);
    m_alarms.insert(a.alarmId, a);
    existed ? emit alarmUpdated(a) : emit alarmAdded(a);
    emit alarmsChanged();
    emit activeAlarmCountChanged(activeAlarmCount());
}

void AlarmManager::acknowledgeAlarm(const QString &alarmId)
{
    if (!m_alarms.contains(alarmId)) return;
    auto a = m_alarms.value(alarmId);
    a.state = "acknowledged";
    a.ackTime = QDateTime::currentSecsSinceEpoch();
    m_alarms.insert(alarmId, a);
    emit alarmUpdated(a);
    emit alarmsChanged();
    emit activeAlarmCountChanged(activeAlarmCount());
}

void AlarmManager::recoverAlarm(const QString &alarmId)
{
    if (!m_alarms.contains(alarmId)) return;
    auto a = m_alarms.value(alarmId);
    a.state = "recovered";
    a.recoverTime = QDateTime::currentSecsSinceEpoch();
    m_alarms.insert(alarmId, a);
    emit alarmUpdated(a);
    emit alarmsChanged();
    emit activeAlarmCountChanged(activeAlarmCount());
}

void AlarmManager::removeDeviceAlarms(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (gatewayId.isEmpty() || deviceId <= 0) {
        return;
    }

    const int targetMasterSlot = masterSlotFromPortId(portId);
    bool changed = false;
    for (auto it = m_alarms.begin(); it != m_alarms.end(); ) {
        const AlarmRecord &alarm = it.value();
        const bool sameDevice = alarm.gatewayId == gatewayId &&
                                alarm.slaveAddr == deviceId &&
                                (targetMasterSlot < 0 || alarm.masterSlot == targetMasterSlot);
        if (sameDevice) {
            it = m_alarms.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        emit alarmsChanged();
        emit activeAlarmCountChanged(activeAlarmCount());
    }
}

void AlarmManager::clearRecoveredAlarms()
{
    bool changed = false;
    for (auto it = m_alarms.begin(); it != m_alarms.end(); ) {
        if (it.value().state == "recovered") {
            it = m_alarms.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    if (changed) {
        emit alarmsChanged();
        emit activeAlarmCountChanged(activeAlarmCount());
    }
}

void AlarmManager::clearAllAlarms()
{
    if (m_alarms.isEmpty()) {
        return;
    }

    m_alarms.clear();
    emit alarmsChanged();
    emit activeAlarmCountChanged(0);
}
