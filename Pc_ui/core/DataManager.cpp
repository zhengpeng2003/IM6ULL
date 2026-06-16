#include "DataManager.h"
#include "DeviceManager.h"
#include "AlarmManager.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>
#include <QDebug>

static const qint64 kRealtimeFreshMs = 30000;
static const qint64 kRealtimeOfflineMs = 90000;

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

void DataManager::loadDemoData(bool demoMode)
{
    if (!demoMode) {
        qWarning("DataManager::loadDemoData skipped: demoMode is false");
        return;
    }
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
        data.mock = true;
        data.dataState = QStringLiteral("mock");
        data.statusLevel = QStringLiteral("mock");
        data.statusText = QStringLiteral("演示数据");
        data.timestamp = QDateTime::currentMSecsSinceEpoch();
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

void DataManager::refreshOfflineStates(qint64 timeoutMs)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const QList<DeviceNode> devices = m_deviceManager->allDevices();
    bool changed = false;

    for (const DeviceNode &device : devices) {
        if (!device.expectTelemetry) {
            continue;
        }

        const bool online = device.lastUpdateTime > 0 && now - device.lastUpdateTime <= timeoutMs;
        if (device.online == online) {
            continue;
        }

        m_deviceManager->updateDeviceOnline(device.key(), online);
        changed = true;

        QMutexLocker locker(&m_mutex);
        if (m_realtimeMap.contains(device.key())) {
            RealtimeDeviceData data = m_realtimeMap.value(device.key());
            data.node.online = online;
            if (!online) {
                const qint64 ageMs = data.timestamp > 0 ? now - data.timestamp : timeoutMs + 1;
                data.dataState = ageMs > kRealtimeOfflineMs ? QStringLiteral("offline") : QStringLiteral("stale");
                data.statusLevel = data.dataState;
                data.statusText = data.dataState == QStringLiteral("offline")
                    ? QStringLiteral("设备离线，保留最后值")
                    : QStringLiteral("旧数据 / 数据过期");
                data.valid = false;
            }
            m_realtimeMap.insert(device.key(), data);
        }
    }

    if (changed) {
        emit deviceTreeChanged();
        emit realtimeDataUpdated();
    }
}

void DataManager::removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId)
{
    m_deviceManager->removeDeviceData(gatewayId, portId, deviceId);

    {
        QMutexLocker locker(&m_mutex);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        pruneExpiredDeletedDevicesLocked(now);
        rememberDeletedDeviceLocked(gatewayId, portId, deviceId, now);
        for (auto it = m_realtimeMap.begin(); it != m_realtimeMap.end(); ) {
            const DeviceNode &node = it.value().node;
            const int slaveId = node.slaveAddr > 0 ? node.slaveAddr : node.deviceId;
            if (node.gatewayId == gatewayId && node.port == portId && slaveId == deviceId) {
                it = m_realtimeMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::forgetRemovedDevice(const QString &gatewayId, const QString &portId, int deviceId)
{
    QMutexLocker locker(&m_mutex);
    m_deletedDevices.remove(deletedDeviceKey(gatewayId, portId, deviceId));
}

void DataManager::removeMasterData(const QString &gatewayId, const QString &portId)
{
    m_deviceManager->removeMasterData(gatewayId, portId);

    {
        QMutexLocker locker(&m_mutex);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        pruneExpiredDeletedDevicesLocked(now);
        for (auto it = m_realtimeMap.begin(); it != m_realtimeMap.end(); ) {
            const DeviceNode &node = it.value().node;
            if (node.gatewayId == gatewayId && node.port == portId) {
                rememberDeletedDeviceLocked(node.gatewayId, node.port, node.deviceId, now);
                it = m_realtimeMap.erase(it);
            } else {
                ++it;
            }
        }
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::clearRuntimeData()
{
    if (m_alarmManager) {
        m_alarmManager->clearAllAlarms();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_realtimeMap.clear();
    }

    emit realtimeDataUpdated();
}

void DataManager::clearAllData()
{
    clearRuntimeData();
}

bool DataManager::isServiceOnline() const
{
    QMutexLocker locker(&m_mutex);
    return m_serviceOnline;
}

void DataManager::markAllDevicesOffline(const QString &reason)
{
    {
        QMutexLocker locker(&m_mutex);
        m_serviceOnline = false;
    }
    QList<DeviceNode> devices = m_deviceManager ? m_deviceManager->allDevices() : QList<DeviceNode>();
    for (DeviceNode &device : devices) {
        device.online = false;
        device.status = reason;
        device.statusReason = reason;
    }
    if (m_deviceManager) {
        m_deviceManager->setDevices(devices);
    }

    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_realtimeMap.begin(); it != m_realtimeMap.end(); ++it) {
            it->node.online = false;
            it->node.status = reason;
            it->serviceOffline = true;
            it->dataState = QStringLiteral("offline");
            it->statusLevel = QStringLiteral("offline");
            it->statusText = reason == QStringLiteral("service_offline")
                ? QStringLiteral("Pc_data 服务离线，保留最后值")
                : QStringLiteral("设备离线，保留最后值");
            it->valid = false;
            if (it->errorMessage.isEmpty()) {
                it->errorMessage = reason;
            }
        }
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::onLatestPointsMessage(const QJsonObject &obj)
{
    {
        QMutexLocker locker(&m_mutex);
        m_serviceOnline = true;
    }
    const QList<RealtimeDeviceData> parsedDevices = parseLatestPoints(obj);
    {
        QMutexLocker locker(&m_mutex);
        pruneExpiredDeletedDevicesLocked(QDateTime::currentMSecsSinceEpoch());
    }

    for (const RealtimeDeviceData &data : parsedDevices) {
        if (isDeletedDevice(data.node.gatewayId, data.node.port, data.node.deviceId)) {
            continue;
        }
        upsertRealtimeData(data);
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

void DataManager::onDevicesSnapshotMessage(const QJsonObject &obj)
{
    {
        QMutexLocker locker(&m_mutex);
        m_serviceOnline = true;
    }
    QList<DeviceNode> devices = parseDevicesSnapshot(obj);
    QSet<QString> snapshotDeviceKeys;
    for (const DeviceNode &device : devices) {
        snapshotDeviceKeys.insert(deletedDeviceKey(device.gatewayId, device.port, device.deviceId));
    }

    {
        QMutexLocker locker(&m_mutex);
        pruneExpiredDeletedDevicesLocked(QDateTime::currentMSecsSinceEpoch());
    }

    for (auto it = devices.begin(); it != devices.end(); ) {
        if (isDeletedDevice(it->gatewayId, it->port, it->deviceId)) {
            it = devices.erase(it);
        } else {
            ++it;
        }
    }

    if (m_deviceManager) {
        m_deviceManager->setDevices(devices);
    }

    {
        QMutexLocker locker(&m_mutex);
        QSet<QString> visibleKeys;
        for (const DeviceNode &node : devices) {
            visibleKeys.insert(node.key());
        }

        for (auto it = m_realtimeMap.begin(); it != m_realtimeMap.end(); ) {
            const DeviceNode &node = it.value().node;
            const QString tombstoneKey = deletedDeviceKey(node.gatewayId, node.port, node.deviceId);
            const qint64 expiresAt = m_deletedDevices.value(tombstoneKey, 0);
            const bool deleted = m_deletedDevices.contains(tombstoneKey) &&
                                 (expiresAt < 0 || expiresAt > QDateTime::currentMSecsSinceEpoch());
            if (!visibleKeys.contains(it.key()) ||
                deleted) {
                it = m_realtimeMap.erase(it);
            } else {
                ++it;
            }
        }

        for (const DeviceNode &node : devices) {
            RealtimeDeviceData data = m_realtimeMap.value(node.key());
            data.node = node;
            if (node.status == QStringLiteral("offline")) {
                data.statusText = QStringLiteral("设备离线");
                data.statusLevel = QStringLiteral("offline");
                data.dataState = QStringLiteral("offline");
            } else if (node.status == QStringLiteral("online")) {
                data.statusText = QStringLiteral("正常");
                data.statusLevel = QStringLiteral("normal");
                data.dataState = QStringLiteral("normal");
            } else if (node.status == QStringLiteral("error")) {
                data.statusText = node.statusReason.isEmpty()
                    ? QStringLiteral("设备异常")
                    : node.statusReason;
                data.statusLevel = QStringLiteral("error");
                data.dataState = QStringLiteral("invalid");
            } else {
                data.statusText = QStringLiteral("未知");
                data.statusLevel = QStringLiteral("unknown");
                data.dataState = QStringLiteral("unknown");
            }
            data.timestamp = node.lastUpdateTime;
            m_realtimeMap.insert(node.key(), data);
        }
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
}

QList<RealtimeDeviceData> DataManager::parseLatestPoints(const QJsonObject &obj) const
{
    QHash<QString, RealtimeDeviceData> deviceMap;
    const QHash<QString, QList<TelemetryPointData>> grouped = groupPointsByDevice(obj);

    for (const QList<TelemetryPointData> &points : grouped) {
        RealtimeDeviceData data = buildRealtimeDeviceData(points);
        if (!data.node.factoryId.isEmpty() && !data.node.key().isEmpty()) {
            const QString key = data.node.key();
            if (!deviceMap.contains(key) ||
                data.node.lastUpdateTime >= deviceMap.value(key).node.lastUpdateTime) {
                deviceMap.insert(key, data);
            }
        }
    }

    return deviceMap.values();
}

QList<DeviceNode> DataManager::parseDevicesSnapshot(const QJsonObject &obj) const
{
    QHash<QString, DeviceNode> deviceMap;
    const QJsonArray rows = obj.value(QStringLiteral("devices")).toArray();

    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject row = value.toObject();
        DeviceNode node;
        node.factoryId = row.value(QStringLiteral("factoryId")).toString();
        node.factoryName = row.value(QStringLiteral("factoryName")).toString();
        node.areaId = row.value(QStringLiteral("areaId")).toString();
        node.areaName = row.value(QStringLiteral("areaName")).toString();
        node.gatewayId = row.value(QStringLiteral("gatewayId")).toString();
        node.gatewayName = row.value(QStringLiteral("gatewayName")).toString();
        node.port = row.value(QStringLiteral("portId")).toString();
        node.masterSlot = masterSlotFromPortId(node.port);
        node.masterName = row.value(QStringLiteral("portName")).toString();
        node.deviceId = row.value(QStringLiteral("deviceId")).toInt();
        node.slaveAddr = node.deviceId;
        node.deviceName = row.value(QStringLiteral("deviceName")).toString();
        node.deviceType = normalizeDeviceType(row.value(QStringLiteral("deviceType")).toString());
        node.expectTelemetry = row.value(QStringLiteral("expectTelemetry")).toBool(true);
        node.status = row.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        node.statusReason = row.value(QStringLiteral("statusReason")).toString();
        node.online = node.status == QStringLiteral("online");
        // lastUpdateTime means the device's real last telemetry time only.
        // Do not use create/update config timestamps as fake telemetry freshness.
        node.lastUpdateTime = row.value(QStringLiteral("lastSeenMs")).toVariant().toLongLong();

        if (node.factoryId.isEmpty() || node.areaId.isEmpty() || node.gatewayId.isEmpty() ||
            node.port.isEmpty() || node.deviceId <= 0) {
            continue;
        }

        const QString key = node.key();
        if (!deviceMap.contains(key) ||
            node.lastUpdateTime >= deviceMap.value(key).lastUpdateTime) {
            deviceMap.insert(key, node);
        }
    }

    return deviceMap.values();
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
    if (!point.valid && point.errorMessage.isEmpty()) {
        point.errorMessage = QStringLiteral("数据无效");
    }
    point.receiveTimeMs = QDateTime::currentMSecsSinceEpoch();
    point.lastUpdateTime = point.receiveTimeMs;
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
    data.node.expectTelemetry = data.node.deviceType != QStringLiteral("relay") &&
                                data.node.deviceType != QStringLiteral("led");
    data.node.online = false;
    data.node.status = QStringLiteral("unknown");
    data.serviceOffline = false;
    data.node.lastUpdateTime = first.receiveTimeMs;
    data.timestamp = first.timestampMs;
    data.valid = true;
    data.dataState = QStringLiteral("normal");
    data.points = points;

    for (const TelemetryPointData &point : points) {
        data.valid = data.valid && point.valid;
        if (data.errorMessage.isEmpty() && !point.errorMessage.isEmpty()) {
            data.errorMessage = point.errorMessage;
        }
        if (point.timestampMs > data.timestamp) {
            data.timestamp = point.timestampMs;
        }
        if (point.receiveTimeMs > data.node.lastUpdateTime) {
            data.node.lastUpdateTime = point.receiveTimeMs;
        }
        applyPointToTypedFields(data, point);
    }

    evaluateDeviceStatus(data);
    return data;
}

void DataManager::evaluateDeviceStatus(RealtimeDeviceData &data) const
{
    data.statusLevel = QStringLiteral("unknown");
    data.statusText = QStringLiteral("未知");
    data.dataState = QStringLiteral("unknown");
    data.node.online = false;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 ageMs = data.node.lastUpdateTime > 0 ? now - data.node.lastUpdateTime : kRealtimeOfflineMs + 1;
    if (data.mock) {
        data.statusLevel = QStringLiteral("mock");
        data.statusText = QStringLiteral("演示数据");
        data.dataState = QStringLiteral("mock");
        return;
    }
    if (!data.valid && (data.errorMessage == QStringLiteral("device_offline") ||
                        data.errorMessage == QStringLiteral("modbus_timeout") ||
                        data.errorMessage == QStringLiteral("timeout"))) {
        data.statusLevel = QStringLiteral("offline");
        data.statusText = QStringLiteral("设备离线，保留最后值");
        data.dataState = QStringLiteral("offline");
        data.node.status = QStringLiteral("offline");
        data.node.online = false;
        return;
    }

    if (ageMs > kRealtimeOfflineMs) {
        data.statusLevel = QStringLiteral("offline");
        data.statusText = QStringLiteral("设备离线，保留最后值");
        data.dataState = QStringLiteral("offline");
        data.node.status = QStringLiteral("offline");
        data.node.online = false;
        return;
    }
    if (ageMs > kRealtimeFreshMs) {
        data.statusLevel = QStringLiteral("stale");
        data.statusText = QStringLiteral("旧数据 / 数据过期");
        data.dataState = QStringLiteral("stale");
        data.node.status = QStringLiteral("stale");
        data.node.online = false;
        return;
    }

    data.node.online = true;
    data.node.status = QStringLiteral("online");
    data.statusLevel = QStringLiteral("normal");
    data.statusText = QStringLiteral("正常");
    data.dataState = QStringLiteral("normal");

    if (data.node.deviceType == "unknown") {
        data.statusLevel = QStringLiteral("warning");
        data.statusText = QStringLiteral("未知设备类型");
        if (data.errorMessage.isEmpty()) {
            data.errorMessage = QStringLiteral("unknown_device_type");
        }
    }

    if (!data.valid) {
        data.statusLevel = QStringLiteral("error");
        data.dataState = QStringLiteral("invalid");
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
            data.dataState = QStringLiteral("offline");
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
    } else if (data.node.deviceType == "relay" && (point.pointKey.startsWith("relay_") || point.pointKey.startsWith("relay.ch"))) {
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
    if (isDeletedDevice(data.node.gatewayId, data.node.port, data.node.deviceId)) {
        return;
    }

    m_deviceManager->upsertDevice(data.node);
    m_deviceManager->updateDeviceOnline(data.node.key(), data.node.online);

    QMutexLocker locker(&m_mutex);
    const RealtimeDeviceData old = m_realtimeMap.value(data.node.key());
    if (old.node.lastUpdateTime > 0 && data.node.lastUpdateTime > 0 &&
        data.node.lastUpdateTime < old.node.lastUpdateTime) {
        qDebug() << "latest_points stale dropped in UI" << data.node.key()
                 << "oldRecvTs" << old.node.lastUpdateTime
                 << "newRecvTs" << data.node.lastUpdateTime
                 << "oldDataTs" << old.timestamp
                 << "newDataTs" << data.timestamp;
        return;
    }
    m_realtimeMap.insert(data.node.key(), data);
}

QString DataManager::deletedDeviceKey(const QString &gatewayId, const QString &portId, int deviceId) const
{
    return QStringLiteral("%1/%2/%3").arg(gatewayId, portId).arg(deviceId);
}

bool DataManager::isDeletedDevice(const QString &gatewayId, const QString &portId, int deviceId) const
{
    QMutexLocker locker(&m_mutex);
    const QString key = deletedDeviceKey(gatewayId, portId, deviceId);
    const qint64 expiresAt = m_deletedDevices.value(key, 0);
    return m_deletedDevices.contains(key) &&
           (expiresAt < 0 || expiresAt > QDateTime::currentMSecsSinceEpoch());
}

void DataManager::pruneExpiredDeletedDevicesLocked(qint64 nowMs)
{
    // Deleted-device tombstones are intentionally sticky. They are released only
    // by explicit add/register/sync success paths, not by telemetry timestamps.
    Q_UNUSED(nowMs);
}

void DataManager::rememberDeletedDeviceLocked(const QString &gatewayId,
                                              const QString &portId,
                                              int deviceId,
                                              qint64 nowMs)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    m_deletedDevices.insert(deletedDeviceKey(gatewayId, portId, deviceId),
                            -nowMs);
}
