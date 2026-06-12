#include "AlarmManager.h"
#include <QDateTime>
#include <QRegularExpression>

namespace {

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
    a.alarmId = obj.value("alarm_id").toString();
    a.factoryId = obj.value("factory_id").toString();
    a.areaId = obj.value("area_id").toString();
    a.areaName = obj.value("area_name").toString();
    a.gatewayId = obj.value("gateway_id").toString();
    a.masterSlot = obj.value("master_slot").toInt();
    a.slaveAddr = obj.value("slave_addr").toInt();
    a.deviceName = obj.value("device_name").toString();
    a.deviceType = obj.value("device_type").toString();
    a.alarmType = obj.value("alarm_type").toString();
    a.level = obj.value("level").toString();
    a.message = obj.value("message").toString();
    a.value = obj.value("value").toDouble();
    a.threshold = obj.value("threshold").toDouble();
    a.state = obj.value("state").toString("active");
    a.startTime = obj.value("timestamp").toVariant().toLongLong();

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
