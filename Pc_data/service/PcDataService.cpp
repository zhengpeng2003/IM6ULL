#include "PcDataService.hpp"

#include <chrono>
#include <iostream>

#include "model/ModelConverter.hpp"

static const std::int64_t kRemovedDeviceIgnoreMs = 30000;

static std::int64_t currentTimeMs();
static std::string removedDeviceKey(const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId);
static std::string removedMasterKey(const std::string& gatewayId,
                                    const std::string& portId);

PcDataService::PcDataService()
{
}

void PcDataService::handleTelemetryPack(const TelemetryPack& pack)
{
    std::vector<TelemetryPoint> points = ModelConverter::toTelemetryPoints(pack);

    std::lock_guard<std::mutex> lock(m_mutex);
    pruneExpiredRemovedDevicesLocked(currentTimeMs());

    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }
        if (shouldAcceptNewDeviceDataLocked(point.gatewayId,
                                            point.portId,
                                            point.deviceId,
                                            point.timestampMs)) {
            m_snapshot[point.pointId] = point;
            continue;
        }
        if (isRemovedDeviceLocked(point.gatewayId, point.portId, point.deviceId)) {
            continue;
        }
        if (isRemovedMasterLocked(point.gatewayId, point.portId)) {
            continue;
        }

        /*
         * Same pointId means the same telemetry point.
         * When new data arrives, overwrite the old value.
         * This is the latest snapshot.
         */
        m_snapshot[point.pointId] = point;
    }
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
    result.reserve(points.size());
    for (const TelemetryPoint& point : points) {
        if (shouldAcceptNewDeviceDataLocked(point.gatewayId,
                                            point.portId,
                                            point.deviceId,
                                            point.timestampMs)) {
            result.push_back(point);
            continue;
        }
        if (isRemovedDeviceLocked(point.gatewayId, point.portId, point.deviceId)) {
            continue;
        }
        if (isRemovedMasterLocked(point.gatewayId, point.portId)) {
            continue;
        }
        result.push_back(point);
    }
    return result;
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

void PcDataService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
    m_removedDevices.clear();
    m_removedMasters.clear();
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
