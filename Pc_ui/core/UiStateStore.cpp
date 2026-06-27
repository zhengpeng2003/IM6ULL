#include "UiStateStore.h"

#include "DataManager.h"
#include "DeviceManager.h"
#include "AlarmManager.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonValue>
#include <QSet>
#include <QTimer>

UiStateStore::UiStateStore(DataManager *dataManager,
                           DeviceManager *deviceManager,
                           AlarmManager *alarmManager,
                           QObject *parent)
    : QObject(parent)
    , m_dataManager(dataManager)
    , m_deviceManager(deviceManager)
    , m_alarmManager(alarmManager)
{
    if (m_alarmManager) {
        connect(m_alarmManager, &AlarmManager::alarmsChanged,
                this, &UiStateStore::scheduleStateChanged);
        connect(m_alarmManager, &AlarmManager::activeAlarmCountChanged,
                this, [this](int) { scheduleStateChanged(); });
    }
}

bool UiStateStore::applyIpcMessage(const QJsonObject &root)
{
    const QString type = root.value(QStringLiteral("type")).toString();
    if (type == QStringLiteral("latest_points")) {
        qDebug() << "[DBG_UI_STATE] UiStateStore latest_points callDataManager:"
                 << (m_dataManager != nullptr)
                 << "pointCount:" << root.value(QStringLiteral("points")).toArray().size();
        if (m_dataManager) {
            m_dataManager->onLatestPointsMessage(root);
        }
        rebuildStateMirror(type);
        qDebug() << "[DBG_UI_STATE] UiStateStore latest_points schedule stateChanged";
        scheduleStateChanged();
        return true;
    }

    if (type == QStringLiteral("devices_snapshot") ||
        type == QStringLiteral("state_snapshot") ||
        type == QStringLiteral("state_delta")) {
        if (m_dataManager && root.contains(QStringLiteral("devices"))) {
            m_dataManager->onDevicesSnapshotMessage(root);
        }
        rebuildStateMirror(type);
        scheduleStateChanged();
        return true;
    }

    if (type == QStringLiteral("gateway_status_snapshot")) {
        if (m_deviceManager) {
            m_deviceManager->setGateways(parseGatewayStatusSnapshot(root));
        }
        rebuildStateMirror(type);
        scheduleStateChanged();
        return true;
    }

    if (type == QStringLiteral("port_status_snapshot")) {
        if (m_deviceManager) {
            m_deviceManager->setPorts(parsePortStatusSnapshot(root));
        }
        rebuildStateMirror(type);
        scheduleStateChanged();
        return true;
    }

    return false;
}

void UiStateStore::refreshOfflineStates(qint64 timeoutMs)
{
    bool changed = false;
    if (m_dataManager) {
        changed = m_dataManager->refreshOfflineStates(timeoutMs);
    }
    if (!changed) {
        return;
    }
    rebuildStateMirror(QStringLiteral("offline_refresh"));
    scheduleStateChanged();
}

void UiStateStore::markAllDevicesOffline(const QString &reason)
{
    if (m_dataManager) {
        m_dataManager->markAllDevicesOffline(reason);
    }
    rebuildStateMirror(reason);
    scheduleStateChanged();
}

void UiStateStore::removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (m_dataManager) {
        m_dataManager->removeDeviceData(gatewayId, portId, deviceId);
    }
    rebuildStateMirror(QStringLiteral("remove_device"));
    scheduleStateChanged();
}

void UiStateStore::removeMasterData(const QString &gatewayId, const QString &portId)
{
    if (m_dataManager) {
        m_dataManager->removeMasterData(gatewayId, portId);
    }
    rebuildStateMirror(QStringLiteral("remove_master"));
    scheduleStateChanged();
}

void UiStateStore::forgetRemovedDevice(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (m_dataManager) {
        m_dataManager->forgetRemovedDevice(gatewayId, portId, deviceId);
    }
    rebuildStateMirror(QStringLiteral("forget_removed_device"));
    scheduleStateChanged();
}

void UiStateStore::clearRuntimeData()
{
    if (m_dataManager) {
        m_dataManager->clearRuntimeData();
    }
    rebuildStateMirror(QStringLiteral("clear_runtime_data"));
    scheduleStateChanged();
}

QList<DeviceState> UiStateStore::deviceStates() const
{
    return m_devices.values();
}

DeviceState UiStateStore::deviceState(const QString &deviceKey) const
{
    return m_devices.value(deviceKey);
}

QList<DeviceNode> UiStateStore::deviceTreeSnapshot() const
{
    QList<DeviceNode> devices;
    for (const DeviceState &state : m_devices) {
        if (state.node.gatewayId.isEmpty() ||
            state.node.port.isEmpty() ||
            state.node.deviceId <= 0) {
            continue;
        }
        devices.append(state.node);
    }
    return devices;
}

QList<RealtimeDeviceData> UiStateStore::realtimeDataSnapshot() const
{
    QList<RealtimeDeviceData> realtimeDevices;
    for (const DeviceState &state : m_devices) {
        if (state.realtime.node.gatewayId.isEmpty() ||
            state.realtime.node.port.isEmpty() ||
            state.realtime.node.deviceId <= 0) {
            continue;
        }
        realtimeDevices.append(state.realtime);
    }
    return realtimeDevices;
}

RealtimeDeviceData UiStateStore::realtimeData(const QString &deviceKey) const
{
    return m_devices.value(deviceKey).realtime;
}

QList<GatewayNode> UiStateStore::gatewaySnapshot() const
{
    return m_deviceManager ? m_deviceManager->allGateways() : QList<GatewayNode>();
}

QList<PortNode> UiStateStore::portSnapshot() const
{
    return m_deviceManager ? m_deviceManager->allPorts() : QList<PortNode>();
}

int UiStateStore::onlineGatewayCount() const
{
    QSet<QString> gateways;
    for (const DeviceState &state : m_devices) {
        if (state.lifecycleState == QStringLiteral("online") ||
            state.node.online) {
            gateways.insert(state.node.gatewayId);
        }
    }
    return gateways.size();
}

int UiStateStore::onlineDeviceCount() const
{
    int count = 0;
    for (const DeviceState &state : m_devices) {
        if (state.lifecycleState == QStringLiteral("online") ||
            state.node.online) {
            ++count;
        }
    }
    return count;
}

int UiStateStore::onlineMasterCount() const
{
    QSet<QString> masters;
    for (const DeviceState &state : m_devices) {
        if (state.lifecycleState == QStringLiteral("online") ||
            state.node.online) {
            masters.insert(state.node.gatewayId + QStringLiteral("/") + QString::number(state.node.masterSlot));
        }
    }
    return masters.size();
}

int UiStateStore::activeAlarmCount() const
{
    return m_alarmManager ? m_alarmManager->activeAlarmCount() : 0;
}

bool UiStateStore::isServiceOnline() const
{
    return m_dataManager ? m_dataManager->isServiceOnline() : false;
}

QList<GatewayNode> UiStateStore::parseGatewayStatusSnapshot(const QJsonObject &root) const
{
    QList<GatewayNode> gateways;
    const QJsonArray rows = root.value(QStringLiteral("gateways")).toArray();
    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject row = value.toObject();
        GatewayNode gateway;
        gateway.gatewayId = row.value(QStringLiteral("gatewayId")).toString();
        gateway.gatewayName = row.value(QStringLiteral("gatewayName")).toString();
        gateway.factoryId = row.value(QStringLiteral("factoryId")).toString();
        gateway.areaId = row.value(QStringLiteral("areaId")).toString();
        gateway.status = row.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        gateway.lastRegisterTimeMs = row.value(QStringLiteral("lastRegisterTimeMs")).toVariant().toLongLong();
        gateway.lastHeartbeatTimeMs = row.value(QStringLiteral("lastHeartbeatTimeMs")).toVariant().toLongLong();
        gateway.updateTimeMs = row.value(QStringLiteral("updateTimeMs")).toVariant().toLongLong();
        if (!gateway.gatewayId.isEmpty()) {
            gateways.append(gateway);
        }
    }
    return gateways;
}

QList<PortNode> UiStateStore::parsePortStatusSnapshot(const QJsonObject &root) const
{
    QList<PortNode> ports;
    const QJsonArray rows = root.value(QStringLiteral("ports")).toArray();
    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject row = value.toObject();
        PortNode port;
        port.gatewayId = row.value(QStringLiteral("gatewayId")).toString();
        port.portId = row.value(QStringLiteral("portId")).toString();
        port.portName = row.value(QStringLiteral("portName")).toString();
        port.slot = row.value(QStringLiteral("slot")).toInt();
        port.devicePath = row.value(QStringLiteral("devicePath")).toString();
        port.baud = row.value(QStringLiteral("baud")).toInt();
        port.status = row.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        port.lastRegisterTimeMs = row.value(QStringLiteral("lastRegisterTimeMs")).toVariant().toLongLong();
        port.updateTimeMs = row.value(QStringLiteral("updateTimeMs")).toVariant().toLongLong();
        if (!port.gatewayId.isEmpty() && !port.portId.isEmpty()) {
            ports.append(port);
        }
    }
    return ports;
}

void UiStateStore::rebuildStateMirror(const QString &source)
{
    QHash<QString, DeviceState> next;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QList<DeviceNode> devices = m_deviceManager ? m_deviceManager->allDevices() : QList<DeviceNode>();
    const QList<RealtimeDeviceData> realtimeDevices = m_dataManager ? m_dataManager->allRealtimeData() : QList<RealtimeDeviceData>();

    for (const DeviceNode &node : devices) {
        if (node.gatewayId.isEmpty() || node.port.isEmpty() || node.deviceId <= 0) {
            continue;
        }
        DeviceState state = m_devices.value(node.key());
        state.node = node;
        state.lastMutationSource = source;
        state.lastStateUpdateMs = nowMs;
        next.insert(node.key(), state);
    }

    for (const RealtimeDeviceData &realtime : realtimeDevices) {
        if (realtime.node.gatewayId.isEmpty() ||
            realtime.node.port.isEmpty() ||
            realtime.node.deviceId <= 0) {
            continue;
        }
        DeviceState state = next.value(realtime.node.key(), m_devices.value(realtime.node.key()));
        state.realtime = realtime;
        if (state.node.gatewayId.isEmpty() ||
            state.node.port.isEmpty() ||
            state.node.deviceId <= 0) {
            state.node = realtime.node;
        } else {
            state.node.online = realtime.node.online;
            state.node.status = realtime.node.status;
            state.node.statusReason = realtime.node.statusReason;
            state.node.lastUpdateTime = realtime.node.lastUpdateTime;
        }
        state.lastMutationSource = source;
        state.lastStateUpdateMs = nowMs;
        state.lifecycleState = lifecycleFor(state);
        next.insert(state.node.key(), state);
    }

    for (auto it = next.begin(); it != next.end(); ++it) {
        if (it->lifecycleState.isEmpty()) {
            it->lifecycleState = lifecycleFor(it.value());
        }
    }

    m_devices = next;
}

void UiStateStore::scheduleStateChanged()
{
    if (m_stateChangePending) {
        return;
    }

    m_stateChangePending = true;
    QTimer::singleShot(50, this, [this]() {
        m_stateChangePending = false;
        emit stateChanged();
    });
}

QString UiStateStore::lifecycleFor(const DeviceState &state) const
{
    if (state.realtime.serviceOffline || state.node.status == QStringLiteral("service_offline")) {
        return QStringLiteral("service_offline");
    }
    if (state.realtime.dataState == QStringLiteral("offline") ||
        state.node.status == QStringLiteral("offline")) {
        return QStringLiteral("offline");
    }
    if (state.realtime.dataState == QStringLiteral("stale") ||
        state.node.status == QStringLiteral("stale")) {
        return QStringLiteral("stale");
    }
    if (state.node.online || state.node.status == QStringLiteral("online")) {
        return QStringLiteral("online");
    }
    if (!state.node.status.isEmpty()) {
        return state.node.status;
    }
    return QStringLiteral("unknown");
}
