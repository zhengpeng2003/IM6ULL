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

void DeviceManager::upsertDevice(const DeviceNode &node)
{
    m_devices.insert(node.key(), node);
    emit deviceConfigChanged();
}

QList<DeviceNode> DeviceManager::allDevices() const
{
    return m_devices.values();
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
