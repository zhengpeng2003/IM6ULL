#include "DataManager.h"
#include "DeviceManager.h"
#include "AlarmManager.h"
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStringList>

static const qint64 kDeletedDeviceIgnoreMs = 30000;

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
                data.statusLevel = QStringLiteral("offline");
                data.statusText = QStringLiteral("设备离线");
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
            if (node.gatewayId == gatewayId && node.port == portId && node.deviceId == deviceId) {
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

void DataManager::clearAllData()
{
    if (m_deviceManager) {
        m_deviceManager->clearAll();
    }
    if (m_alarmManager) {
        m_alarmManager->clearAllAlarms();
    }

    {
        QMutexLocker locker(&m_mutex);
        m_realtimeMap.clear();
        m_deletedDevices.clear();
    }

    emit deviceTreeChanged();
    emit realtimeDataUpdated();
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
    {
        QMutexLocker locker(&m_mutex);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        pruneExpiredDeletedDevicesLocked(now);
        for (const RealtimeDeviceData &data : parsedDevices) {
            releaseDeletedDeviceOnNewDataLocked(data.node.gatewayId,
                                                data.node.port,
                                                data.node.deviceId,
                                                data.node.lastUpdateTime);
        }
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
    QList<DeviceNode> devices = parseDevicesSnapshot(obj);
    QSet<QString> snapshotDeviceKeys;
    for (const DeviceNode &device : devices) {
        snapshotDeviceKeys.insert(deletedDeviceKey(device.gatewayId, device.port, device.deviceId));
    }

    {
        QMutexLocker locker(&m_mutex);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        pruneExpiredDeletedDevicesLocked(now);
        for (const DeviceNode &device : devices) {
            releaseDeletedDeviceOnNewDataLocked(device.gatewayId,
                                                device.port,
                                                device.deviceId,
                                                device.lastUpdateTime);
        }
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
            } else if (node.status == QStringLiteral("online")) {
                data.statusText = QStringLiteral("正常");
                data.statusLevel = QStringLiteral("normal");
            } else if (node.status == QStringLiteral("error")) {
                data.statusText = node.statusReason.isEmpty()
                    ? QStringLiteral("设备异常")
                    : node.statusReason;
                data.statusLevel = QStringLiteral("error");
            } else {
                data.statusText = QStringLiteral("未知");
                data.statusLevel = QStringLiteral("unknown");
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
        node.lastUpdateTime = row.value(QStringLiteral("lastSeenMs")).toVariant().toLongLong();
        const qint64 updateTime = row.value(QStringLiteral("updateTimeMs")).toVariant().toLongLong();
        const qint64 createTime = row.value(QStringLiteral("createTimeMs")).toVariant().toLongLong();
        if (updateTime > node.lastUpdateTime) {
            node.lastUpdateTime = updateTime;
        }
        if (createTime > node.lastUpdateTime) {
            node.lastUpdateTime = createTime;
        }

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
    data.node.online = true;
    data.node.status = QStringLiteral("online");
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
    if (isDeletedDevice(data.node.gatewayId, data.node.port, data.node.deviceId)) {
        return;
    }

    m_deviceManager->upsertDevice(data.node);
    m_deviceManager->updateDeviceOnline(data.node.key(), data.node.online);

    QMutexLocker locker(&m_mutex);
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
    for (auto it = m_deletedDevices.begin(); it != m_deletedDevices.end(); ) {
        const qint64 value = it.value();
        const bool expiredDeadline = value > 0 && value <= nowMs;
        const bool expiredDeleteTime = value < 0 && (-value + kDeletedDeviceIgnoreMs) <= nowMs;
        if (expiredDeadline || expiredDeleteTime) {
            it = m_deletedDevices.erase(it);
        } else {
            ++it;
        }
    }
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

void DataManager::releaseDeletedDeviceOnNewDataLocked(const QString &gatewayId,
                                                      const QString &portId,
                                                      int deviceId,
                                                      qint64 dataTimeMs)
{
    const QString key = deletedDeviceKey(gatewayId, portId, deviceId);
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0 || !m_deletedDevices.contains(key)) {
        return;
    }

    const qint64 value = m_deletedDevices.value(key);
    const qint64 deletedAt = value < 0 ? -value : value - kDeletedDeviceIgnoreMs;
    if (dataTimeMs > 0 && deletedAt > 0 && dataTimeMs >= deletedAt) {
        m_deletedDevices.remove(key);
    }
}
