#include "DeviceManager.h"
#include <QSet>
#include <QDebug>

namespace {

bool configFieldsChanged(const DeviceNode &oldNode, const DeviceNode &newNode)
{
    return oldNode.factoryId != newNode.factoryId ||
           oldNode.factoryName != newNode.factoryName ||
           oldNode.areaId != newNode.areaId ||
           oldNode.areaName != newNode.areaName ||
           oldNode.gatewayId != newNode.gatewayId ||
           oldNode.gatewayName != newNode.gatewayName ||
           oldNode.masterSlot != newNode.masterSlot ||
           oldNode.masterName != newNode.masterName ||
           oldNode.port != newNode.port ||
           oldNode.baud != newNode.baud ||
           oldNode.slaveAddr != newNode.slaveAddr ||
           oldNode.deviceId != newNode.deviceId ||
           oldNode.deviceName != newNode.deviceName ||
           oldNode.deviceType != newNode.deviceType ||
           oldNode.expectTelemetry != newNode.expectTelemetry;
}

bool realtimeFieldsChanged(const DeviceNode &oldNode, const DeviceNode &newNode)
{
    return oldNode.online != newNode.online ||
           oldNode.status != newNode.status ||
           oldNode.statusReason != newNode.statusReason ||
           oldNode.lastUpdateTime != newNode.lastUpdateTime;
}

} // namespace

DeviceManager::DeviceManager(QObject *parent) : QObject(parent) {}

void DeviceManager::setDevices(const QList<DeviceNode> &devices)
{
    const int before = devices.size();
    m_devices.clear();
    int dropped = 0;
    int duplicates = 0;
    for (const auto &d : devices) {
        if (d.gatewayId.isEmpty() || d.port.isEmpty() || d.deviceId <= 0) {
            ++dropped;
            qWarning() << "DeviceManager dropped invalid device" << d.gatewayId << d.port << d.deviceId;
            continue;
        }
        const QString key = d.key();
        if (m_devices.contains(key)) {
            ++duplicates;
            qWarning() << "DeviceManager duplicate device overwritten" << key;
        }
        m_devices.insert(key, d);
    }
    qDebug() << "DeviceManager setDevices dedup" << before << "->" << m_devices.size()
             << "dropped" << dropped << "duplicates" << duplicates;
    emit deviceConfigChanged();
    emitOnlineCountsIfChanged();
}

void DeviceManager::setGateways(const QList<GatewayNode> &gateways)
{
    m_gateways.clear();
    for (const auto &gateway : gateways) {
        if (!gateway.gatewayId.isEmpty()) {
            m_gateways.insert(gateway.gatewayId, gateway);
        }
    }
    emit gatewayStatusChanged();
    emitOnlineCountsIfChanged();
}

void DeviceManager::setPorts(const QList<PortNode> &ports)
{
    m_ports.clear();
    for (const auto &port : ports) {
        if (!port.gatewayId.isEmpty() && !port.portId.isEmpty()) {
            m_ports.insert(port.key(), port);
        }
    }
    emit portStatusChanged();
    emit deviceConfigChanged();
    emitOnlineCountsIfChanged();
}

void DeviceManager::clearAll()
{
    m_devices.clear();
    m_gateways.clear();
    m_ports.clear();
    emit deviceConfigChanged();
    emit gatewayStatusChanged();
    emit portStatusChanged();
    emitOnlineCountsIfChanged();
}

void DeviceManager::upsertDevice(const DeviceNode &node)
{
    if (node.gatewayId.isEmpty() || node.port.isEmpty() || node.deviceId <= 0) {
        qWarning() << "DeviceManager upsert dropped invalid device" << node.gatewayId << node.port << node.deviceId;
        return;
    }
    const QString key = node.key();
    if (!m_devices.contains(key)) {
        qDebug() << "[DBG_DEVICE] upsertDevice key:" << key
                 << "oldLastUpdate:" << 0
                 << "newLastUpdate:" << node.lastUpdateTime
                 << "oldStatus:" << QStringLiteral("<missing>")
                 << "newStatus:" << node.status
                 << "configChanged:" << true
                 << "realtimeChanged:" << true
                 << "emitDeviceConfigChanged:" << true
                 << "emitDeviceOnlineStateChanged:" << true;
        m_devices.insert(key, node);
        emit deviceConfigChanged();
        emit deviceOnlineStateChanged(key, node.online);
        emitOnlineCountsIfChanged();
        return;
    }

    const DeviceNode oldNode = m_devices.value(key);
    const bool configChanged = configFieldsChanged(oldNode, node);
    const bool realtimeChanged = realtimeFieldsChanged(oldNode, node);
    qDebug() << "[DBG_DEVICE] upsertDevice key:" << key
             << "oldLastUpdate:" << oldNode.lastUpdateTime
             << "newLastUpdate:" << node.lastUpdateTime
             << "oldStatus:" << oldNode.status
             << "newStatus:" << node.status
             << "configChanged:" << configChanged
             << "realtimeChanged:" << realtimeChanged
             << "emitDeviceConfigChanged:" << configChanged
             << "emitDeviceOnlineStateChanged:" << (oldNode.online != node.online);
    if (!configChanged && !realtimeChanged) {
        return;
    }

    m_devices.insert(key, node);
    if (configChanged) {
        emit deviceConfigChanged();
    }
    if (oldNode.online != node.online) {
        emit deviceOnlineStateChanged(key, node.online);
    }
    emitOnlineCountsIfChanged();
}

void DeviceManager::removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId)
{
    bool removed = false;
    for (auto it = m_devices.begin(); it != m_devices.end(); ) {
        const DeviceNode &node = it.value();
        const int slaveId = node.slaveAddr > 0 ? node.slaveAddr : node.deviceId;
        if (node.gatewayId == gatewayId && node.port == portId && slaveId == deviceId) {
            it = m_devices.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    if (!removed) {
        return;
    }

    emit deviceConfigChanged();
    emitOnlineCountsIfChanged();
}

void DeviceManager::removeMasterData(const QString &gatewayId, const QString &portId)
{
    bool removed = false;
    for (auto it = m_devices.begin(); it != m_devices.end(); ) {
        const DeviceNode &node = it.value();
        if (node.gatewayId == gatewayId && node.port == portId) {
            it = m_devices.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }

    if (!removed) {
        return;
    }

    emit deviceConfigChanged();
    emitOnlineCountsIfChanged();
}

QList<DeviceNode> DeviceManager::allDevices() const
{
    return m_devices.values();
}

QList<GatewayNode> DeviceManager::allGateways() const
{
    return m_gateways.values();
}

QList<PortNode> DeviceManager::allPorts() const
{
    return m_ports.values();
}

DeviceNode DeviceManager::device(const QString &key) const
{
    return m_devices.value(key);
}

void DeviceManager::updateDeviceOnline(const QString &key, bool online)
{
    if (!m_devices.contains(key)) return;
    auto node = m_devices.value(key);
    if (node.online == online) return;
    node.online = online;
    m_devices.insert(key, node);
    emit deviceOnlineStateChanged(key, online);
    emitOnlineCountsIfChanged();
}

int DeviceManager::onlineGatewayCount() const
{
    if (!m_gateways.isEmpty()) {
        int count = 0;
        for (const auto &gateway : m_gateways) {
            if (gateway.online()) ++count;
        }
        return count;
    }

    QSet<QString> gateways;
    for (const auto &d : m_devices) if (d.online) gateways.insert(d.gatewayId);
    return gateways.size();
}

int DeviceManager::onlineDeviceCount() const
{
    int count = 0;
    for (const auto &d : m_devices) if (d.online) ++count;
    return count;
}

void DeviceManager::emitOnlineCountsIfChanged()
{
    const int gatewayCount = onlineGatewayCount();
    const int deviceCount = onlineDeviceCount();
    if (m_lastOnlineGatewayCount != gatewayCount) {
        m_lastOnlineGatewayCount = gatewayCount;
        emit onlineGatewayCountChanged(gatewayCount);
    }
    if (m_lastOnlineDeviceCount != deviceCount) {
        m_lastOnlineDeviceCount = deviceCount;
        emit onlineDeviceCountChanged(deviceCount);
    }
}
