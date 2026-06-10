#include "DataManager.h"
#include "DeviceManager.h"
#include "AlarmManager.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>

static int masterSlotFromPortId(const QString &portId)
{
    static const QRegularExpression re("^(?:port_|rs485-|RS485-)?(\\d+)$");
    const QRegularExpressionMatch match = re.match(portId);
    if (!match.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const int portNumber = match.captured(1).toInt(&ok);
    if (!ok || portNumber <= 0) {
        return 0;
    }

    return portNumber - 1;
}

static QString normalizeDeviceType(const QString &deviceType)
{
    if (deviceType == "electric_meter") {
        return "meter";
    }

    return deviceType;
}

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

void DataManager::markAllDevicesOffline()
{
    const QList<DeviceNode> devices = m_deviceManager->allDevices();
    for (const auto &device : devices) {
        m_deviceManager->updateDeviceOnline(device.key(), false);
    }

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_realtimeMap.begin(); it != m_realtimeMap.end(); ++it) {
            it->node.online = false;
        }
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::onLatestPointsMessage(const QJsonObject &obj)
{
    const QList<RealtimeDeviceData> parsedDevices = parseLatestPoints(obj);
    for (const RealtimeDeviceData &data : parsedDevices) {
        upsertRealtimeData(data);
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

QList<RealtimeDeviceData> DataManager::parseLatestPoints(const QJsonObject &obj) const
{
    QList<RealtimeDeviceData> devices;
    const QHash<QString, QList<TelemetryPointData>> grouped = groupPointsByDevice(obj);

    for (const QList<TelemetryPointData> &points : grouped) {
        RealtimeDeviceData data = buildRealtimeDeviceData(points);
        if (!data.node.factoryId.isEmpty()) {
            devices.append(data);
        }
    }

    return devices;
}

TelemetryPointData DataManager::parseTelemetryPoint(const QJsonObject &obj) const
{
    TelemetryPointData point;
    point.pointId = obj.value("pointId").toString();
    point.timestampMs = obj.value("timestampMs").toVariant().toLongLong();
    point.factoryId = obj.value("factoryId").toString();
    point.factoryName = obj.value("factoryName").toString();
    point.areaId = obj.value("areaId").toString();
    point.areaName = obj.value("areaName").toString();
    point.gatewayId = obj.value("gatewayId").toString();
    point.gatewayName = obj.value("gatewayName").toString();
    point.portId = obj.value("portId").toString();
    point.portName = obj.value("portName").toString();
    point.deviceId = obj.value("deviceId").toInt();
    point.deviceName = obj.value("deviceName").toString();
    point.deviceType = normalizeDeviceType(obj.value("deviceType").toString());
    point.pointKey = obj.value("pointKey").toString();
    point.pointName = obj.value("pointName").toString();
    point.unit = obj.value("unit").toString();
    point.valueType = obj.value("valueType").toString();
    point.numberValue = obj.value("numberValue").toDouble();
    point.textValue = obj.value("textValue").toString();
    point.valid = obj.value("valid").toBool(true);
    point.errorMessage = obj.value("errorMessage").toString();
    return point;
}

QHash<QString, QList<TelemetryPointData>> DataManager::groupPointsByDevice(const QJsonObject &obj) const
{
    QHash<QString, QList<TelemetryPointData>> grouped;
    const QJsonArray points = obj.value("points").toArray();

    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }

        const TelemetryPointData point = parseTelemetryPoint(value.toObject());
        if (point.factoryId.isEmpty() || point.areaId.isEmpty() || point.gatewayId.isEmpty() ||
            point.portId.isEmpty() || point.deviceId <= 0) {
            continue;
        }

        const QString key = QString("%1/%2/%3/%4/%5")
            .arg(point.factoryId, point.areaId, point.gatewayId, point.portId)
            .arg(point.deviceId);
        grouped[key].append(point);
    }

    return grouped;
}

RealtimeDeviceData DataManager::buildRealtimeDeviceData(const QList<TelemetryPointData> &points) const
{
    RealtimeDeviceData data;
    if (points.isEmpty()) {
        return data;
    }

    const TelemetryPointData &first = points.first();
    data.node.factoryId = first.factoryId;
    data.node.factoryName = first.factoryName;
    data.node.areaId = first.areaId;
    data.node.areaName = first.areaName;
    data.node.gatewayId = first.gatewayId;
    data.node.gatewayName = first.gatewayName;
    data.node.port = first.portId;
    data.node.masterSlot = masterSlotFromPortId(first.portId);
    data.node.masterName = first.portName;
    data.node.slaveAddr = first.deviceId;
    data.node.deviceId = first.deviceId;
    data.node.deviceName = first.deviceName;
    data.node.deviceType = first.deviceType;
    data.node.online = true;
    data.node.lastUpdateTime = first.timestampMs;
    data.timestamp = first.timestampMs;
    data.valid = true;
    data.points = points;

    for (const TelemetryPointData &point : points) {
        data.valid = data.valid && point.valid;
        if (data.errorMessage.isEmpty() && !point.errorMessage.isEmpty()) {
            data.errorMessage = point.errorMessage;
        }
        if (point.timestampMs > data.timestamp) {
            data.timestamp = point.timestampMs;
            data.node.lastUpdateTime = point.timestampMs;
        }
        applyPointToTypedFields(data, point);
    }

    evaluateDeviceStatus(data);
    return data;
}

void DataManager::evaluateDeviceStatus(RealtimeDeviceData &data) const
{
    data.statusLevel = QStringLiteral("normal");
    data.statusText = QStringLiteral("正常");
    data.node.online = true;

    if (data.node.deviceType == "unknown") {
        data.statusLevel = QStringLiteral("warning");
        data.statusText = QStringLiteral("未知设备类型");
        if (data.errorMessage.isEmpty()) {
            data.errorMessage = QStringLiteral("unknown_device_type");
        }
    }

    if (!data.valid) {
        data.statusLevel = QStringLiteral("error");
        const QString reason = data.errorMessage;
        if (reason == "modbus_timeout") {
            data.statusText = QStringLiteral("通信超时");
            data.node.online = false;
        } else if (reason == "modbus_crc_error") {
            data.statusText = QStringLiteral("CRC 校验错误");
            data.node.online = false;
        } else if (reason == "invalid_data") {
            data.statusText = QStringLiteral("非法数据");
        } else if (reason == "device_offline") {
            data.statusText = QStringLiteral("设备离线");
            data.node.online = false;
            data.statusLevel = QStringLiteral("offline");
        } else if (reason == "unknown_device_type") {
            data.statusText = QStringLiteral("未知设备类型");
        } else {
            data.statusText = reason.isEmpty() ? QStringLiteral("数据无效") : reason;
        }
        return;
    }

    QStringList warnings;
    for (const TelemetryPointData &point : data.points) {
        if (!point.valid || point.valueType == "text") {
            continue;
        }

        if (point.pointKey == "temperature" && point.numberValue > 60.0) {
            warnings << QStringLiteral("温度异常值");
        } else if (point.pointKey == "humidity" && point.numberValue > 85.0) {
            warnings << QStringLiteral("湿度异常值");
        } else if (point.pointKey == "voltage" && point.numberValue < 180.0) {
            warnings << QStringLiteral("低电压异常值");
        } else if (point.pointKey == "current" && point.numberValue > 20.0) {
            warnings << QStringLiteral("电流异常值");
        } else if (point.pointKey == "power" && point.numberValue > 5000.0) {
            warnings << QStringLiteral("功率异常值");
        } else if (point.pointKey == "cpu_usage" && point.numberValue > 90.0) {
            warnings << QStringLiteral("CPU 使用率异常值");
        } else if (point.pointKey == "memory_usage" && point.numberValue > 85.0) {
            warnings << QStringLiteral("内存使用率异常值");
        }
    }

    if (!warnings.isEmpty()) {
        data.statusLevel = QStringLiteral("warning");
        data.statusText = warnings.join(QStringLiteral("、"));
    }
}

void DataManager::applyPointToTypedFields(RealtimeDeviceData &data, const TelemetryPointData &point) const
{
    if (!point.valid) {
        return;
    }

    if (data.node.deviceType == "sensor_th") {
        if (point.pointKey == "temperature") {
            data.sensorTh.temperature = point.numberValue;
        } else if (point.pointKey == "humidity") {
            data.sensorTh.humidity = point.numberValue;
        }
    } else if (data.node.deviceType == "meter") {
        if (point.pointKey == "voltage") {
            data.meter.voltage = point.numberValue;
        } else if (point.pointKey == "current") {
            data.meter.current = point.numberValue;
        } else if (point.pointKey == "power") {
            data.meter.power = point.numberValue;
        } else if (point.pointKey == "energy") {
            data.meter.energy = point.numberValue;
        }
    } else if (data.node.deviceType == "relay" && point.pointKey.startsWith("relay_")) {
        const bool on = point.numberValue != 0.0;
        data.relay.channels.insert(point.pointKey, on);
        if (point.pointKey == "relay_1") {
            data.relay.led = on;
        } else if (point.pointKey == "relay_2") {
            data.relay.fan = on;
        } else if (point.pointKey == "relay_3") {
            data.relay.buzzer = on;
        }
    }
}

void DataManager::upsertRealtimeData(const RealtimeDeviceData &data)
{
    m_deviceManager->upsertDevice(data.node);
    m_deviceManager->updateDeviceOnline(data.node.key(), data.node.online);

    QMutexLocker locker(&m_mutex);
    m_realtimeMap.insert(data.node.key(), data);
}
