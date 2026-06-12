#include "PcDataService.hpp"

#include <chrono>
#include <iostream>

#include "model/ModelConverter.hpp"

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

    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }
        if (m_removedDevices.find(removedDeviceKey(point.gatewayId,
                                                   point.portId,
                                                   point.deviceId)) != m_removedDevices.end()) {
            continue;
        }
        if (m_removedMasters.find(removedMasterKey(point.gatewayId, point.portId)) != m_removedMasters.end()) {
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
    m_removedDevices.insert(removedDeviceKey(gatewayId, portId, deviceId));

    return removed;
}

bool PcDataService::removeMasterData(const std::string& gatewayId,
                                     const std::string& portId)
{
    if (gatewayId.empty() || portId.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    bool removed = false;

    for (auto it = m_snapshot.begin(); it != m_snapshot.end(); ) {
        const TelemetryPoint& point = it->second;
        if (point.gatewayId == gatewayId && point.portId == portId) {
            m_removedDevices.insert(removedDeviceKey(point.gatewayId, point.portId, point.deviceId));
            it = m_snapshot.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    m_removedMasters.insert(removedMasterKey(gatewayId, portId));

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
    return m_removedDevices.find(removedDeviceKey(gatewayId, portId, deviceId)) != m_removedDevices.end();
}

std::vector<TelemetryPoint> PcDataService::filterRemovedPoints(const std::vector<TelemetryPoint>& points) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TelemetryPoint> result;
    result.reserve(points.size());
    for (const TelemetryPoint& point : points) {
        if (m_removedDevices.find(removedDeviceKey(point.gatewayId,
                                                   point.portId,
                                                   point.deviceId)) != m_removedDevices.end()) {
            continue;
        }
        if (m_removedMasters.find(removedMasterKey(point.gatewayId, point.portId)) != m_removedMasters.end()) {
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
