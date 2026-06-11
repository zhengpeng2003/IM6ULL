#include "DeviceManager.h"
#include <QSet>

DeviceManager::DeviceManager(QObject *parent) : QObject(parent) {}

void DeviceManager::setDevices(const QList<DeviceNode> &devices)
{
    m_devices.clear();
    for (const auto &d : devices) m_devices.insert(d.key(), d);
    emit deviceConfigChanged();
    emit onlineGatewayCountChanged(onlineGatewayCount());
    emit onlineDeviceCountChanged(onlineDeviceCount());
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
    emit onlineGatewayCountChanged(onlineGatewayCount());
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
}

void DeviceManager::upsertDevice(const DeviceNode &node)
{
    m_devices.insert(node.key(), node);
    emit deviceConfigChanged();
    emit onlineGatewayCountChanged(onlineGatewayCount());
    emit onlineDeviceCountChanged(onlineDeviceCount());
}

void DeviceManager::removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId)
{
    bool removed = false;
    for (auto it = m_devices.begin(); it != m_devices.end(); ) {
        const DeviceNode &node = it.value();
        if (node.gatewayId == gatewayId && node.port == portId && node.deviceId == deviceId) {
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
    emit onlineGatewayCountChanged(onlineGatewayCount());
    emit onlineDeviceCountChanged(onlineDeviceCount());
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
    emit onlineGatewayCountChanged(onlineGatewayCount());
    emit onlineDeviceCountChanged(onlineDeviceCount());
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
    emit onlineGatewayCountChanged(onlineGatewayCount());
    emit onlineDeviceCountChanged(onlineDeviceCount());
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
