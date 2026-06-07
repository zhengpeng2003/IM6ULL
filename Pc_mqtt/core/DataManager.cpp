#include "DataManager.h"
#include "DeviceManager.h"
#include "AlarmManager.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QMutexLocker>

DataManager::DataManager(DeviceManager *deviceManager, AlarmManager *alarmManager, QObject *parent)
    : QObject(parent), m_deviceManager(deviceManager), m_alarmManager(alarmManager)
{
}

void DataManager::loadDemoData()
{
    QList<DeviceNode> list;

    DeviceNode th;
    th.factoryId = "factory_001";
    th.areaId = "area_001";
    th.areaName = QStringLiteral("1号厂房");
    th.gatewayId = "imx6ull_001";
    th.masterSlot = 0;
    th.masterName = QStringLiteral("环境采集总线");
    th.slaveAddr = 1;
    th.deviceId = 1;
    th.deviceName = QStringLiteral("温湿度传感器1");
    th.deviceType = "sensor_th";
    th.online = true;
    list << th;

    DeviceNode meter = th;
    meter.slaveAddr = 3;
    meter.deviceId = 3;
    meter.deviceName = QStringLiteral("电表1");
    meter.deviceType = "meter";
    list << meter;

    DeviceNode relay = th;
    relay.masterSlot = 1;
    relay.masterName = QStringLiteral("设备控制总线");
    relay.slaveAddr = 1;
    relay.deviceId = 2;
    relay.deviceName = QStringLiteral("继电器模块1");
    relay.deviceType = "relay";
    list << relay;

    m_deviceManager->setDevices(list);

    QMutexLocker locker(&m_mutex);
    for (const auto &node : list) {
        RealtimeDeviceData data;
        data.node = node;
        data.valid = true;
        data.timestamp = QDateTime::currentSecsSinceEpoch();
        data.sensorTh.temperature = 26.5;
        data.sensorTh.humidity = 60.2;
        data.relay.led = true;
        data.meter.voltage = 220.0;
        data.meter.current = 1.2;
        data.meter.power = 260.0;
        data.meter.energy = 88.8;
        m_realtimeMap.insert(node.key(), data);
    }
    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

QList<DeviceNode> DataManager::deviceTreeSnapshot() const
{
    return m_deviceManager->allDevices();
}

RealtimeDeviceData DataManager::deviceData(const QString &deviceKey) const
{
    QMutexLocker locker(&m_mutex);
    return m_realtimeMap.value(deviceKey);
}

QList<RealtimeDeviceData> DataManager::allRealtimeData() const
{
    QMutexLocker locker(&m_mutex);
    return m_realtimeMap.values();
}

void DataManager::onMqttMessageArrived(const QString &topic, const QByteArray &payload)
{
    const auto doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) return;
    const QJsonObject obj = doc.object();
    const QString msgType = obj.value("msg_type").toString();

    if (msgType == "telemetry" || topic.endsWith("/telemetry")) handleTelemetry(obj);
    else if (msgType == "status" || topic.endsWith("/status")) handleStatus(obj);
    else if (msgType == "heartbeat" || topic.endsWith("/heartbeat")) handleHeartbeat(obj);
    else if (msgType == "alarm" || topic.endsWith("/alarm")) m_alarmManager->onAlarmMessage(obj);
}

void DataManager::handleTelemetry(const QJsonObject &obj)
{
    const auto devices = obj.value("devices").toArray();
    for (const auto &v : devices) {
        const auto d = v.toObject();
        DeviceNode node;
        node.factoryId = obj.value("factory_id").toString();
        node.areaId = obj.value("area_id").toString();
        node.areaName = obj.value("area_name").toString();
        node.gatewayId = obj.value("gateway_id").toString();
        node.masterSlot = d.value("master_slot").toInt();
        node.masterName = d.value("master_name").toString();
        node.slaveAddr = d.value("slave_addr").toInt();
        node.deviceId = d.value("device_id").toInt();
        node.deviceName = d.value("device_name").toString();
        node.deviceType = d.value("device_type").toString();
        node.online = d.value("online").toBool(true);
        node.lastUpdateTime = obj.value("timestamp").toVariant().toLongLong();

        RealtimeDeviceData rt;
        rt.node = node;
        rt.valid = d.value("valid").toBool(true);
        rt.timestamp = node.lastUpdateTime;

        if (node.deviceType == "sensor_th") {
            rt.sensorTh.temperature = d.value("temperature").toDouble();
            rt.sensorTh.humidity = d.value("humidity").toDouble();
        } else if (node.deviceType == "relay") {
            const auto s = d.value("relay_states").toObject();
            rt.relay.led = s.value("led").toBool();
            rt.relay.fan = s.value("fan").toBool();
            rt.relay.buzzer = s.value("buzzer").toBool();
        } else if (node.deviceType == "meter") {
            rt.meter.voltage = d.value("voltage").toDouble();
            rt.meter.current = d.value("current").toDouble();
            rt.meter.power = d.value("power").toDouble();
            rt.meter.energy = d.value("energy").toDouble();
        }

        m_deviceManager->upsertDevice(node);
        m_deviceManager->updateDeviceOnline(node.key(), node.online);
        {
            QMutexLocker locker(&m_mutex);
            m_realtimeMap.insert(node.key(), rt);
        }
        emit telemetryForDb(TelemetryRecord{rt});
    }
    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::handleStatus(const QJsonObject &obj)
{
    Q_UNUSED(obj);
    emit deviceTreeChanged();
}

void DataManager::handleHeartbeat(const QJsonObject &obj)
{
    Q_UNUSED(obj);
}
