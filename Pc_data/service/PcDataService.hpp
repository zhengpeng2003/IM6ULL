#ifndef PC_DATA_SERVICE_HPP
#define PC_DATA_SERVICE_HPP

#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "model/DeviceRecord.hpp"
#include "model/PointConfig.hpp"
#include "model/TelemetryPack.hpp"
#include "model/TelemetryPoint.hpp"
#include "storage/PcDatabase.hpp"

struct SyncSelectedDevice
{
    std::string portId;
    int deviceId = 0;
};

struct SyncGatewaySelection
{
    std::string gatewayId;
    std::vector<SyncSelectedDevice> devices;
};

struct SyncGatewayPending
{
    int requestId = 0;
    std::int64_t seq = 0;
    std::string gatewayId;
    std::vector<SyncSelectedDevice> devices;
    std::int64_t requestTimeMs = 0;
};

struct SyncConfigResult
{
    bool ready = false;
    bool success = false;
    std::string message;
    int portCount = 0;
    int deviceCount = 0;
};

struct PendingCommandTarget
{
    std::string commandId;
    std::int64_t uiSeq = 0;
    std::int64_t boardSeq = 0;
    std::string commandType;
    std::string gatewayId;
    std::string portId;
    int deviceId = 0;
    std::int64_t requestTimeMs = 0;
    bool timeoutNotified = false;
    bool hardTimeoutNotified = false;
};

class PcDataService
{
public:
    PcDataService();

    void handleTelemetryPack(const TelemetryPack& pack);
    void handleTelemetryPoints(const std::vector<TelemetryPoint>& points);

    void updateDeviceRegistry(const std::vector<DeviceRecord>& devices);
    void updateDeviceRegistry(const DeviceRecord& device);
    void updateGatewayPorts(const std::vector<GatewayPort>& ports);
    void updateGatewayPort(const GatewayPort& port);
    void updatePointConfigs(const std::vector<PointConfig>& configs);
    void updatePointConfig(const PointConfig& config);
    void updateGatewayStatuses(const std::vector<GatewayStatus>& gateways);
    void updateGatewayStatus(const GatewayStatus& gateway);

    std::vector<DeviceRecord> getDeviceRegistrySnapshot() const;
    std::vector<GatewayPort> getGatewayPortSnapshot() const;
    std::vector<PointConfig> getPointConfigSnapshot() const;
    std::vector<GatewayStatus> getGatewayStatusSnapshot() const;

    std::vector<TelemetryPoint> getLatestPoints() const;
    bool getPointById(const std::string& pointId, TelemetryPoint& outPoint) const;
    bool getDeviceByKey(const std::string& gatewayId,
                        const std::string& portId,
                        int deviceId,
                        DeviceRecord& outDevice) const;
    std::vector<TelemetryPoint> getLatestPointsForDevice(const std::string& gatewayId,
                                                         const std::string& portId,
                                                         int deviceId) const;
    std::vector<TelemetryPoint> getLatestPointsSnapshot() const;

    bool removeDeviceData(const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId);

    bool removeMasterData(const std::string& gatewayId,
                          const std::string& portId);

    void forgetRemovedDevice(const std::string& gatewayId,
                             const std::string& portId,
                             int deviceId);

    bool isRemovedDevice(const std::string& gatewayId,
                         const std::string& portId,
                         int deviceId) const;

    std::vector<TelemetryPoint> filterRemovedPoints(const std::vector<TelemetryPoint>& points) const;

    void rememberGatewayRegistry(const GatewayRegistry& registry);
    void rememberGatewayPorts(const std::vector<GatewayPort>& ports);
    void rememberDeviceRegistry(const DeviceRecord& device);
    void rememberDeviceRegistries(const std::vector<DeviceRecord>& devices);
    void rememberPointConfigs(const std::vector<PointConfig>& configs);
    void replaceGatewayRegistry(const std::string& gatewayId,
                                const std::vector<GatewayPort>& ports,
                                const std::vector<DeviceRecord>& devices,
                                const std::vector<PointConfig>& pointConfigs,
                                bool fullSnapshot);
    bool gatewayRegistered(const std::string& gatewayId) const;
    bool portRegistered(const std::string& gatewayId, const std::string& portId) const;
    bool deviceRegistered(const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId) const;
    std::string registeredDeviceType(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId) const;
    bool pointConfigRegistered(const std::string& gatewayId,
                               const std::string& portId,
                               int deviceId,
                               const std::string& pointKey) const;

    std::vector<SyncGatewayPending> beginSyncConfigRequest(const std::vector<SyncGatewaySelection>& targets);

    bool findSyncPending(std::int64_t seq, SyncGatewayPending& pending) const;

    bool completeSyncConfig(std::int64_t seq,
                            bool success,
                            const std::string& message,
                            int portCount,
                            int deviceCount,
                            SyncConfigResult& result);

    std::vector<SyncConfigResult> collectSyncConfigTimeouts(std::int64_t nowMs,
                                                            std::int64_t timeoutMs);

    void rememberPendingCommand(const PendingCommandTarget& target);

    bool findPendingCommand(const std::string& commandId, PendingCommandTarget& target) const;

    bool takePendingCommand(const std::string& commandId, PendingCommandTarget& target);

    std::vector<PendingCommandTarget> collectCommandSoftTimeouts(std::int64_t nowMs,
                                                                 std::int64_t timeoutMs);

    std::vector<PendingCommandTarget> collectCommandHardTimeouts(std::int64_t nowMs,
                                                                 std::int64_t timeoutMs);

    void clear();

private:
    void pruneExpiredRemovedDevicesLocked(std::int64_t nowMs) const;

    void rememberRemovedDeviceLocked(const std::string& gatewayId,
                                     const std::string& portId,
                                     int deviceId,
                                     std::int64_t nowMs);

    bool isRemovedDeviceLocked(const std::string& gatewayId,
                               const std::string& portId,
                               int deviceId) const;

    bool isRemovedMasterLocked(const std::string& gatewayId,
                               const std::string& portId) const;

    bool shouldAcceptNewDeviceDataLocked(const std::string& gatewayId,
                                         const std::string& portId,
                                         int deviceId,
                                         std::int64_t dataTimeMs) const;

    static std::string registryDeviceKey(const std::string& gatewayId,
                                         const std::string& portId,
                                         int deviceId);
    static std::string registryPortKey(const std::string& gatewayId,
                                       const std::string& portId);
    static std::string registryPointKey(const std::string& gatewayId,
                                        const std::string& portId,
                                        int deviceId,
                                        const std::string& pointKey);

private:
    mutable std::mutex m_mutex;

    std::unordered_map<std::string, TelemetryPoint> m_snapshot;
    std::unordered_map<std::string, DeviceRecord> m_deviceRegistry;
    std::unordered_map<std::string, GatewayPort> m_gatewayPorts;
    std::unordered_map<std::string, GatewayStatus> m_gatewayStatuses;
    std::unordered_map<std::string, PointConfig> m_pointConfigs;
    std::set<std::string> m_registeredGateways;
    std::set<std::string> m_registeredPorts;
    std::unordered_map<std::string, std::string> m_registeredDeviceTypes;
    std::set<std::string> m_registeredPointConfigs;

    mutable std::unordered_map<std::string, std::int64_t> m_removedDevices;
    mutable std::unordered_map<std::string, std::int64_t> m_removedMasters;

    std::unordered_map<std::int64_t, SyncGatewayPending> m_syncPendingBySeq;
    std::unordered_map<int, std::vector<SyncGatewayPending> > m_syncRequests;
    std::unordered_map<int, SyncConfigResult> m_syncRequestResults;

    std::unordered_map<std::string, PendingCommandTarget> m_pendingCommandsByCommandId;

    int m_nextSyncRequestId = 1;
    std::int64_t m_nextSyncSeq = 1;
};

#endif // PC_DATA_SERVICE_HPP
