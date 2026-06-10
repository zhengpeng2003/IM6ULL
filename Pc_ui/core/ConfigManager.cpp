#include "ConfigManager.h"

ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {}

QList<DeviceNode> ConfigManager::loadDeviceConfig() const
{
    return {};
}

void ConfigManager::saveDeviceConfig(const QList<DeviceNode> &devices) const
{
    Q_UNUSED(devices);
}
