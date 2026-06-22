#include "PcDataService.hpp"

#include <chrono>
#include <iostream>
#include <algorithm>
#include <set>

#include "model/ModelConverter.hpp"
#include "storage/PcDatabase.hpp"

static const std::int64_t kRemovedDeviceIgnoreMs = 30000;

static std::int64_t currentTimeMs();
static std::string removedDeviceKey(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId);
static std::string removedMasterKey(const std::string& gatewayId,
                                    const std::string& portId);
static std::string deviceRegistryKey(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId);
static std::string portRegistryKey(const std::string& gatewayId,
                                   const std::string& portId);
static std::string pointRegistryKey(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId,
                                    const std::string& pointKey);
static std::string telemetryDeviceKey(const TelemetryPoint& point);

PcDataService::PcDataService()
{
}
void PcDataService::handleTelemetryPack(const TelemetryPack& pack)
{
    std::vector<TelemetryPoint> points = ModelConverter::toTelemetryPoints(pack);
    handleTelemetryPoints(points);
}
void PcDataService::handleTelemetryPoints(const std::vector<TelemetryPoint>& points)
{
    const std::int64_t receiveTimeMs = currentTimeMs();

    std::lock_guard<std::mutex> lock(m_mutex);
    pruneExpiredRemovedDevicesLocked(receiveTimeMs);
    int updatedCount = 0;
    int skippedCount = 0;

    std::cout << "[DBG_TELEMETRY] PcDataService handleTelemetryPoints inputCount="
              << points.size()
              << " snapshotBefore=" << m_snapshot.size()
              << std::endl;

    std::vector<TelemetryPoint> acceptedPoints;
    acceptedPoints.reserve(points.size());
    std::set<std::string> incomingDeviceKeys;

    for (auto point : points) {
        point.receiveTimeMs = receiveTimeMs;
        if (point.pointId.empty()) {
            ++skippedCount;
            continue;
        }
        if (isRemovedDeviceLocked(point.gatewayId, point.portId, point.deviceId) ||
            isRemovedMasterLocked(point.gatewayId, point.portId)) {
            ++skippedCount;
            continue;
        }

        acceptedPoints.push_back(point);
        incomingDeviceKeys.insert(telemetryDeviceKey(point));
    }

    int removedOldCount = 0;
    if (!incomingDeviceKeys.empty()) {
        for (auto it = m_snapshot.begin(); it != m_snapshot.end(); ) {
            if (incomingDeviceKeys.find(telemetryDeviceKey(it->second)) != incomingDeviceKeys.end()) {
                it = m_snapshot.erase(it);
                ++removedOldCount;
            } else {
                ++it;
            }
        }
    }

    for (const TelemetryPoint& point : acceptedPoints) {
        auto old = m_snapshot.find(point.pointId);
        if (old == m_snapshot.end() || point.receiveTimeMs >= old->second.receiveTimeMs) {
            m_snapshot[point.pointId] = point;
            ++updatedCount;
        } else {
            ++skippedCount;
        }
    }

    std::cout << "[DBG_TELEMETRY] PcDataService handleTelemetryPoints updated="
              << updatedCount
              << " skipped=" << skippedCount
              << " removedOld=" << removedOldCount
              << " snapshotAfter=" << m_snapshot.size()
              << std::endl;
}

void PcDataService::updateDeviceRegistry(const std::vector<DeviceRecord>& devices)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const DeviceRecord& device : devices) {
        if (device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0) {
            continue;
        }
        m_deviceRegistry[deviceRegistryKey(device.gatewayId, device.portId, device.deviceId)] = device;
        m_registeredGateways.insert(device.gatewayId);
        m_registeredPorts.insert(portRegistryKey(device.gatewayId, device.portId));
        m_registeredDeviceTypes[registryDeviceKey(device.gatewayId, device.portId, device.deviceId)] = device.deviceType;
    }
}

void PcDataService::updateDeviceRegistry(const DeviceRecord& device)
{
    updateDeviceRegistry(std::vector<DeviceRecord>{device});
}

void PcDataService::updateGatewayPorts(const std::vector<GatewayPort>& ports)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const GatewayPort& port : ports) {
        if (port.gatewayId.empty() || port.portId.empty()) {
            continue;
        }
        m_gatewayPorts[portRegistryKey(port.gatewayId, port.portId)] = port;
        m_registeredGateways.insert(port.gatewayId);
        m_registeredPorts.insert(portRegistryKey(port.gatewayId, port.portId));
    }
}

void PcDataService::updateGatewayPort(const GatewayPort& port)
{
    updateGatewayPorts(std::vector<GatewayPort>{port});
}

void PcDataService::updateGatewayStatuses(const std::vector<GatewayStatus>& gateways)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const GatewayStatus& gateway : gateways) {
        if (gateway.gatewayId.empty()) {
            continue;
        }
        m_gatewayStatuses[gateway.gatewayId] = gateway;
        m_registeredGateways.insert(gateway.gatewayId);
    }
}

void PcDataService::updateGatewayStatus(const GatewayStatus& gateway)
{
    updateGatewayStatuses(std::vector<GatewayStatus>{gateway});
}

void PcDataService::updatePointConfigs(const std::vector<PointConfig>& configs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const PointConfig& config : configs) {
        if (config.gatewayId.empty() || config.portId.empty() || config.deviceId <= 0 || config.pointKey.empty()) {
            continue;
        }
        m_pointConfigs[pointRegistryKey(config.gatewayId,
                                        config.portId,
                                        config.deviceId,
                                        config.pointKey)] = config;
        m_registeredPointConfigs.insert(registryPointKey(config.gatewayId,
                                                         config.portId,
                                                         config.deviceId,
                                                         config.pointKey));
    }
}

std::vector<DeviceRecord> PcDataService::getDeviceRegistrySnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<DeviceRecord> result;
    result.reserve(m_deviceRegistry.size());
    for (const auto& item : m_deviceRegistry) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<GatewayPort> PcDataService::getGatewayPortSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<GatewayPort> result;
    result.reserve(m_gatewayPorts.size());
    for (const auto& item : m_gatewayPorts) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<GatewayStatus> PcDataService::getGatewayStatusSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<GatewayStatus> result;
    result.reserve(m_gatewayStatuses.size());
    for (const auto& item : m_gatewayStatuses) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<PointConfig> PcDataService::getPointConfigSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<PointConfig> result;
    result.reserve(m_pointConfigs.size());
    for (const auto& item : m_pointConfigs) {
        result.push_back(item.second);
    }
    return result;
}

std::vector<TelemetryPoint> PcDataService::getLatestPoints() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TelemetryPoint> result;
    result.reserve(m_snapshot.size());

    for (const auto& item : m_snapshot) {
        result.push_back(item.second);
    }

    return result;
}

std::vector<TelemetryPoint> PcDataService::getLatestPointsSnapshot() const
{
    return getLatestPoints();
}

std::vector<TelemetryPoint> PcDataService::getLatestPointsForDevice(const std::string& gatewayId,
                                                                    const std::string& portId,
                                                                    int deviceId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TelemetryPoint> result;
    for (const auto& item : m_snapshot) {
        const TelemetryPoint& point = item.second;
        if (point.gatewayId == gatewayId &&
            point.portId == portId &&
            point.deviceId == deviceId) {
            result.push_back(point);
        }
    }
    return result;
}

bool PcDataService::getDeviceByKey(const std::string& gatewayId,
                                   const std::string& portId,
                                   int deviceId,
                                   DeviceRecord& outDevice) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_deviceRegistry.find(deviceRegistryKey(gatewayId, portId, deviceId));
    if (it == m_deviceRegistry.end()) {
        return false;
    }
    outDevice = it->second;
    return true;
}

bool PcDataService::getPointById(const std::string& pointId, TelemetryPoint& outPoint) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_snapshot.find(pointId);
    if (it == m_snapshot.end()) {
        return false;
    }

    outPoint = it->second;
    return true;
}

bool PcDataService::removeDeviceData(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId)
{
    if (gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const std::int64_t nowMs = currentTimeMs();
    pruneExpiredRemovedDevicesLocked(nowMs);
    bool removed = false;

    for (auto it = m_snapshot.begin(); it != m_snapshot.end(); ) {
        const TelemetryPoint& point = it->second;
        if (point.gatewayId == gatewayId && point.portId == portId && point.deviceId == deviceId) {
            it = m_snapshot.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    m_registeredDeviceTypes.erase(registryDeviceKey(gatewayId, portId, deviceId));
    const std::string pointPrefix = registryDeviceKey(gatewayId, portId, deviceId) + "/";
    for (auto it = m_registeredPointConfigs.begin(); it != m_registeredPointConfigs.end(); ) {
        if (it->rfind(pointPrefix, 0) == 0) {
            it = m_registeredPointConfigs.erase(it);
        } else {
            ++it;
        }
    }
    rememberRemovedDeviceLocked(gatewayId, portId, deviceId, nowMs);

    return removed;
}

bool PcDataService::removeMasterData(const std::string& gatewayId,
                                     const std::string& portId)
{
    if (gatewayId.empty() || portId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    const std::int64_t nowMs = currentTimeMs();
    pruneExpiredRemovedDevicesLocked(nowMs);
    bool removed = false;

    for (auto it = m_snapshot.begin(); it != m_snapshot.end(); ) {
        const TelemetryPoint& point = it->second;
        if (point.gatewayId == gatewayId && point.portId == portId) {
            rememberRemovedDeviceLocked(point.gatewayId, point.portId, point.deviceId, nowMs);
            it = m_snapshot.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    const std::string masterPrefix = registryPortKey(gatewayId, portId) + "/";
    for (auto it = m_registeredDeviceTypes.begin(); it != m_registeredDeviceTypes.end(); ) {
        if (it->first.rfind(masterPrefix, 0) == 0) {
            it = m_registeredDeviceTypes.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_registeredPointConfigs.begin(); it != m_registeredPointConfigs.end(); ) {
        if (it->rfind(masterPrefix, 0) == 0) {
            it = m_registeredPointConfigs.erase(it);
        } else {
            ++it;
        }
    }
    m_registeredPorts.erase(registryPortKey(gatewayId, portId));
    m_removedMasters[removedMasterKey(gatewayId, portId)] = nowMs + kRemovedDeviceIgnoreMs;

    return removed;
}

void PcDataService::forgetRemovedDevice(const std::string& gatewayId,
                                        const std::string& portId,
                                        int deviceId)
{
    if (gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    pruneExpiredRemovedDevicesLocked(currentTimeMs());
    std::cout << "forget removed device tombstone: gatewayId=" << gatewayId
              << " portId=" << portId
              << " deviceId=" << deviceId << std::endl;
    m_removedDevices.erase(removedDeviceKey(gatewayId, portId, deviceId));
    m_removedMasters.erase(removedMasterKey(gatewayId, portId));
}

bool PcDataService::isRemovedDevice(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId) const
{
    if (gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    pruneExpiredRemovedDevicesLocked(currentTimeMs());
    return isRemovedDeviceLocked(gatewayId, portId, deviceId);
}

std::vector<TelemetryPoint> PcDataService::filterRemovedPoints(const std::vector<TelemetryPoint>& points) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    pruneExpiredRemovedDevicesLocked(currentTimeMs());

    std::vector<TelemetryPoint> result;
    int filteredCount = 0;
    result.reserve(points.size());
    for (const TelemetryPoint& point : points) {
        if (isRemovedDeviceLocked(point.gatewayId, point.portId, point.deviceId) ||
            isRemovedMasterLocked(point.gatewayId, point.portId)) {
            std::cout << "deleted device telemetry dropped: gatewayId=" << point.gatewayId
                      << " portId=" << point.portId
                      << " deviceId=" << point.deviceId
                      << " pointId=" << point.pointId << std::endl;
            ++filteredCount;
            continue;
        }
        result.push_back(point);
    }
    std::cout << "[DBG_TELEMETRY] filterRemovedPoints before="
              << points.size()
              << " after=" << result.size()
              << " filtered=" << filteredCount
              << std::endl;
    return result;
}

void PcDataService::rememberGatewayRegistry(const GatewayRegistry& registry)
{
    if (registry.gatewayId.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_registeredGateways.insert(registry.gatewayId);
}

void PcDataService::rememberGatewayPorts(const std::vector<GatewayPort>& ports)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const GatewayPort& port : ports) {
        if (port.gatewayId.empty() || port.portId.empty()) {
            continue;
        }
        m_registeredGateways.insert(port.gatewayId);
        m_registeredPorts.insert(registryPortKey(port.gatewayId, port.portId));
        m_gatewayPorts[portRegistryKey(port.gatewayId, port.portId)] = port;
    }
}

void PcDataService::rememberDeviceRegistry(const DeviceRecord& device)
{
    if (device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_registeredGateways.insert(device.gatewayId);
    m_registeredPorts.insert(registryPortKey(device.gatewayId, device.portId));
    m_registeredDeviceTypes[registryDeviceKey(device.gatewayId, device.portId, device.deviceId)] =
        device.deviceType;
    m_deviceRegistry[deviceRegistryKey(device.gatewayId, device.portId, device.deviceId)] = device;
}

void PcDataService::rememberDeviceRegistries(const std::vector<DeviceRecord>& devices)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const DeviceRecord& device : devices) {
        if (device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0) {
            continue;
        }
        m_registeredGateways.insert(device.gatewayId);
        m_registeredPorts.insert(registryPortKey(device.gatewayId, device.portId));
        m_registeredDeviceTypes[registryDeviceKey(device.gatewayId, device.portId, device.deviceId)] =
            device.deviceType;
        m_deviceRegistry[deviceRegistryKey(device.gatewayId, device.portId, device.deviceId)] = device;
    }
}

void PcDataService::rememberPointConfigs(const std::vector<PointConfig>& configs)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const PointConfig& config : configs) {
        if (config.gatewayId.empty() || config.portId.empty() ||
            config.deviceId <= 0 || config.pointKey.empty()) {
            continue;
        }
        m_registeredPointConfigs.insert(registryPointKey(config.gatewayId,
                                                         config.portId,
                                                         config.deviceId,
                                                         config.pointKey));
        m_pointConfigs[pointRegistryKey(config.gatewayId,
                                        config.portId,
                                        config.deviceId,
                                        config.pointKey)] = config;
    }
}

void PcDataService::updatePointConfig(const PointConfig& config)
{
    updatePointConfigs(std::vector<PointConfig>{config});
}

void PcDataService::replaceGatewayRegistry(const std::string& gatewayId,
                                           const std::vector<GatewayPort>& ports,
                                           const std::vector<DeviceRecord>& devices,
                                           const std::vector<PointConfig>& pointConfigs,
                                           bool fullSnapshot)
{
    if (gatewayId.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_registeredGateways.insert(gatewayId);

    if (fullSnapshot) {
        for (auto it = m_registeredPorts.begin(); it != m_registeredPorts.end(); ) {
            if (it->rfind(gatewayId + "/", 0) == 0) {
                it = m_registeredPorts.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_registeredDeviceTypes.begin(); it != m_registeredDeviceTypes.end(); ) {
            if (it->first.rfind(gatewayId + "/", 0) == 0) {
                it = m_registeredDeviceTypes.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_registeredPointConfigs.begin(); it != m_registeredPointConfigs.end(); ) {
            if (it->rfind(gatewayId + "/", 0) == 0) {
                it = m_registeredPointConfigs.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_deviceRegistry.begin(); it != m_deviceRegistry.end(); ) {
            const DeviceRecord& device = it->second;
            if (device.gatewayId == gatewayId) {
                it = m_deviceRegistry.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_gatewayPorts.begin(); it != m_gatewayPorts.end(); ) {
            if (it->second.gatewayId == gatewayId) {
                it = m_gatewayPorts.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = m_pointConfigs.begin(); it != m_pointConfigs.end(); ) {
            if (it->second.gatewayId == gatewayId) {
                it = m_pointConfigs.erase(it);
            } else {
                ++it;
            }
        }
    }

    for (const GatewayPort& port : ports) {
        if (port.gatewayId == gatewayId && !port.portId.empty()) {
            m_registeredPorts.insert(registryPortKey(port.gatewayId, port.portId));
            m_gatewayPorts[portRegistryKey(port.gatewayId, port.portId)] = port;
        }
    }

    for (const DeviceRecord& device : devices) {
        if (device.gatewayId == gatewayId && !device.portId.empty() && device.deviceId > 0) {
            m_registeredPorts.insert(registryPortKey(device.gatewayId, device.portId));
            m_registeredDeviceTypes[registryDeviceKey(device.gatewayId, device.portId, device.deviceId)] =
                device.deviceType;
            m_deviceRegistry[deviceRegistryKey(device.gatewayId, device.portId, device.deviceId)] = device;
        }
    }

    for (const PointConfig& config : pointConfigs) {
        if (config.gatewayId == gatewayId && !config.portId.empty() &&
            config.deviceId > 0 && !config.pointKey.empty()) {
            m_registeredPointConfigs.insert(registryPointKey(config.gatewayId,
                                                             config.portId,
                                                             config.deviceId,
                                                             config.pointKey));
            m_pointConfigs[pointRegistryKey(config.gatewayId,
                                            config.portId,
                                            config.deviceId,
                                            config.pointKey)] = config;
        }
    }
}

bool PcDataService::gatewayRegistered(const std::string& gatewayId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registeredGateways.find(gatewayId) != m_registeredGateways.end() ||
           std::any_of(m_deviceRegistry.begin(), m_deviceRegistry.end(), [&](const auto& item) {
               return item.second.gatewayId == gatewayId;
           }) ||
           std::any_of(m_gatewayPorts.begin(), m_gatewayPorts.end(), [&](const auto& item) {
               return item.second.gatewayId == gatewayId;
           });
}

bool PcDataService::portRegistered(const std::string& gatewayId, const std::string& portId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registeredPorts.find(registryPortKey(gatewayId, portId)) != m_registeredPorts.end() ||
           m_gatewayPorts.find(portRegistryKey(gatewayId, portId)) != m_gatewayPorts.end();
}

bool PcDataService::deviceRegistered(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registeredDeviceTypes.find(registryDeviceKey(gatewayId, portId, deviceId)) !=
           m_registeredDeviceTypes.end() ||
           m_deviceRegistry.find(deviceRegistryKey(gatewayId, portId, deviceId)) != m_deviceRegistry.end();
}

std::string PcDataService::registeredDeviceType(const std::string& gatewayId,
                                                const std::string& portId,
                                                int deviceId) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto it = m_registeredDeviceTypes.find(registryDeviceKey(gatewayId, portId, deviceId));
    return it == m_registeredDeviceTypes.end() ? std::string() : it->second;
}

bool PcDataService::pointConfigRegistered(const std::string& gatewayId,
                                          const std::string& portId,
                                          int deviceId,
                                          const std::string& pointKey) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_registeredPointConfigs.find(registryPointKey(gatewayId, portId, deviceId, pointKey)) !=
           m_registeredPointConfigs.end() ||
           m_pointConfigs.find(pointRegistryKey(gatewayId, portId, deviceId, pointKey)) != m_pointConfigs.end();
}

std::vector<SyncGatewayPending> PcDataService::beginSyncConfigRequest(const std::vector<SyncGatewaySelection>& targets)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<SyncGatewayPending> result;
    const int requestId = m_nextSyncRequestId++;
    const std::int64_t nowMs = currentTimeMs();

    for (const SyncGatewaySelection& target : targets) {
        if (target.gatewayId.empty() || target.devices.empty()) {
            continue;
        }

        SyncGatewayPending pending;
        pending.requestId = requestId;
        pending.seq = m_nextSyncSeq++;
        pending.gatewayId = target.gatewayId;
        pending.devices = target.devices;
        pending.requestTimeMs = nowMs;

        m_syncPendingBySeq[pending.seq] = pending;
        m_syncRequests[requestId].push_back(pending);
        result.push_back(pending);
    }

    if (!result.empty()) {
        SyncConfigResult aggregate;
        aggregate.ready = false;
        aggregate.success = true;
        aggregate.message = "config sync success";
        m_syncRequestResults[requestId] = aggregate;
    }

    return result;
}

bool PcDataService::findSyncPending(std::int64_t seq, SyncGatewayPending& pending) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_syncPendingBySeq.find(seq);
    if (it == m_syncPendingBySeq.end()) {
        return false;
    }

    pending = it->second;
    return true;
}

bool PcDataService::completeSyncConfig(std::int64_t seq,
                                       bool success,
                                       const std::string& message,
                                       int portCount,
                                       int deviceCount,
                                       SyncConfigResult& result)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_syncPendingBySeq.find(seq);
    if (it == m_syncPendingBySeq.end()) {
        return false;
    }

    const int requestId = it->second.requestId;
    m_syncPendingBySeq.erase(it);

    SyncConfigResult& aggregate = m_syncRequestResults[requestId];
    aggregate.success = aggregate.success && success;
    aggregate.portCount += portCount;
    aggregate.deviceCount += deviceCount;
    if (!success) {
        aggregate.message = message.empty() ? "gateway offline or sync timeout" : message;
    }

    auto requestIt = m_syncRequests.find(requestId);
    if (requestIt != m_syncRequests.end()) {
        std::vector<SyncGatewayPending>& pendingList = requestIt->second;
        for (auto pendingIt = pendingList.begin(); pendingIt != pendingList.end(); ++pendingIt) {
            if (pendingIt->seq == seq) {
                pendingList.erase(pendingIt);
                break;
            }
        }

        if (!pendingList.empty()) {
            return false;
        }
        m_syncRequests.erase(requestIt);
    }

    aggregate.ready = true;
    if (aggregate.success && aggregate.message.empty()) {
        aggregate.message = "config sync success";
    }
    result = aggregate;
    m_syncRequestResults.erase(requestId);
    return true;
}

std::vector<SyncConfigResult> PcDataService::collectSyncConfigTimeouts(std::int64_t nowMs,
                                                                       std::int64_t timeoutMs)
{
    std::vector<std::int64_t> timedOutSeqs;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& item : m_syncPendingBySeq) {
            if (nowMs - item.second.requestTimeMs >= timeoutMs) {
                timedOutSeqs.push_back(item.first);
            }
        }
    }

    std::vector<SyncConfigResult> results;
    for (std::int64_t seq : timedOutSeqs) {
        SyncConfigResult result;
        if (completeSyncConfig(seq, false, "gateway offline or sync timeout", 0, 0, result)) {
            results.push_back(result);
        }
    }

    return results;
}

void PcDataService::rememberPendingCommand(const PendingCommandTarget& target)
{
    if (target.commandId.empty() || target.commandType.empty()) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_pendingCommandsByCommandId[target.commandId] = target;
}

bool PcDataService::findPendingCommand(const std::string& commandId, PendingCommandTarget& target) const
{
    if (commandId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pendingCommandsByCommandId.find(commandId);
    if (it == m_pendingCommandsByCommandId.end()) {
        return false;
    }

    target = it->second;
    return true;
}

bool PcDataService::takePendingCommand(const std::string& commandId, PendingCommandTarget& target)
{
    if (commandId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pendingCommandsByCommandId.find(commandId);
    if (it == m_pendingCommandsByCommandId.end()) {
        return false;
    }

    target = it->second;
    m_pendingCommandsByCommandId.erase(it);
    return true;
}

std::vector<PendingCommandTarget> PcDataService::collectCommandSoftTimeouts(std::int64_t nowMs,
                                                                            std::int64_t timeoutMs)
{
    std::vector<PendingCommandTarget> results;
    if (nowMs <= 0 || timeoutMs <= 0) {
        return results;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_pendingCommandsByCommandId.begin(); it != m_pendingCommandsByCommandId.end(); ++it) {
        const std::int64_t ageMs = nowMs - it->second.requestTimeMs;
        if (!it->second.timeoutNotified && ageMs >= timeoutMs) {
            it->second.timeoutNotified = true;
            results.push_back(it->second);
        }
    }

    return results;
}

std::vector<PendingCommandTarget> PcDataService::collectCommandHardTimeouts(std::int64_t nowMs,
                                                                            std::int64_t timeoutMs)
{
    std::vector<PendingCommandTarget> results;
    if (nowMs <= 0 || timeoutMs <= 0) {
        return results;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_pendingCommandsByCommandId.begin(); it != m_pendingCommandsByCommandId.end(); ++it) {
        const std::int64_t ageMs = nowMs - it->second.requestTimeMs;
        if (!it->second.hardTimeoutNotified && ageMs >= timeoutMs) {
            it->second.hardTimeoutNotified = true;
            results.push_back(it->second);
        }
    }

    return results;
}

void PcDataService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
    m_deviceRegistry.clear();
    m_gatewayPorts.clear();
    m_gatewayStatuses.clear();
    m_pointConfigs.clear();
    m_registeredGateways.clear();
    m_registeredPorts.clear();
    m_registeredDeviceTypes.clear();
    m_registeredPointConfigs.clear();
    m_removedDevices.clear();
    m_removedMasters.clear();
    m_pendingCommandsByCommandId.clear();
}

std::string PcDataService::registryDeviceKey(const std::string& gatewayId,
                                             const std::string& portId,
                                             int deviceId)
{
    return gatewayId + "/" + portId + "/" + std::to_string(deviceId);
}

std::string PcDataService::registryPortKey(const std::string& gatewayId,
                                           const std::string& portId)
{
    return gatewayId + "/" + portId;
}

std::string PcDataService::registryPointKey(const std::string& gatewayId,
                                            const std::string& portId,
                                            int deviceId,
                                            const std::string& pointKey)
{
    return registryDeviceKey(gatewayId, portId, deviceId) + "/" + pointKey;
}

void PcDataService::pruneExpiredRemovedDevicesLocked(std::int64_t nowMs) const
{
    for (auto it = m_removedDevices.begin(); it != m_removedDevices.end(); ) {
        if (it->second <= nowMs) {
            it = m_removedDevices.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_removedMasters.begin(); it != m_removedMasters.end(); ) {
        if (it->second <= nowMs) {
            it = m_removedMasters.erase(it);
        } else {
            ++it;
        }
    }
}

void PcDataService::rememberRemovedDeviceLocked(const std::string& gatewayId,
                                                const std::string& portId,
                                                int deviceId,
                                                std::int64_t nowMs)
{
    if (gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return;
    }

    m_removedDevices[removedDeviceKey(gatewayId, portId, deviceId)] = nowMs + kRemovedDeviceIgnoreMs;
}

bool PcDataService::isRemovedDeviceLocked(const std::string& gatewayId,
                                          const std::string& portId,
                                          int deviceId) const
{
    return m_removedDevices.find(removedDeviceKey(gatewayId, portId, deviceId)) != m_removedDevices.end();
}

bool PcDataService::isRemovedMasterLocked(const std::string& gatewayId,
                                          const std::string& portId) const
{
    return m_removedMasters.find(removedMasterKey(gatewayId, portId)) != m_removedMasters.end();
}

bool PcDataService::shouldAcceptNewDeviceDataLocked(const std::string& gatewayId,
                                                    const std::string& portId,
                                                    int deviceId,
                                                    std::int64_t dataTimeMs) const
{
    if (dataTimeMs <= 0) {
        return false;
    }

    const std::string deviceKey = removedDeviceKey(gatewayId, portId, deviceId);
    auto deviceIt = m_removedDevices.find(deviceKey);
    if (deviceIt != m_removedDevices.end() &&
        dataTimeMs >= deviceIt->second - kRemovedDeviceIgnoreMs) {
        m_removedDevices.erase(deviceIt);
        m_removedMasters.erase(removedMasterKey(gatewayId, portId));
        return true;
    }

    const std::string masterKey = removedMasterKey(gatewayId, portId);
    auto masterIt = m_removedMasters.find(masterKey);
    if (masterIt != m_removedMasters.end() &&
        dataTimeMs >= masterIt->second - kRemovedDeviceIgnoreMs) {
        m_removedMasters.erase(masterIt);
        return true;
    }

    return false;
}

static std::int64_t currentTimeMs()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
               ).count();
}

static std::string removedDeviceKey(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId)
{
    return gatewayId + "/" + portId + "/" + std::to_string(deviceId);
}

static std::string removedMasterKey(const std::string& gatewayId,
                                    const std::string& portId)
{
    return gatewayId + "/" + portId;
}

static std::string deviceRegistryKey(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId)
{
    return gatewayId + "/" + portId + "/" + std::to_string(deviceId);
}

static std::string portRegistryKey(const std::string& gatewayId,
                                   const std::string& portId)
{
    return gatewayId + "/" + portId;
}

static std::string pointRegistryKey(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId,
                                    const std::string& pointKey)
{
    return deviceRegistryKey(gatewayId, portId, deviceId) + "/" + pointKey;
}

static std::string telemetryDeviceKey(const TelemetryPoint& point)
{
    return deviceRegistryKey(point.gatewayId, point.portId, point.deviceId);
}
