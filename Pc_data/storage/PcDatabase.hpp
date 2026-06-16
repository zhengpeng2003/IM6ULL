#ifndef PC_DATABASE_HPP
#define PC_DATABASE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "model/PointConfig.hpp"
#include "model/DeviceRecord.hpp"
#include "model/TelemetryPoint.hpp"

struct sqlite3;

struct GatewayStatus
{
    std::string gatewayId;
    std::string gatewayName;
    std::string factoryId;
    std::string areaId;
    std::string status;
    std::int64_t lastRegisterTimeMs = 0;
    std::int64_t lastHeartbeatTimeMs = 0;
    std::int64_t updateTimeMs = 0;
};

struct GatewayPort
{
    std::string gatewayId;
    std::string portId;
    std::string portName;
    int slot = 0;
    std::string devicePath;
    int baud = 0;
    std::string status;
    std::int64_t lastRegisterTimeMs = 0;
    std::int64_t updateTimeMs = 0;
};

struct ConfigSnapshotDevice
{
    DeviceRecord device;
    std::string thresholdConfigJson;
    bool thresholdEnabled = false;
};

struct DbSelectedDevice
{
    std::string portId;
    int deviceId = 0;
};


struct AlarmEvent
{
    std::string alarmId;
    std::int64_t timestampMs = 0;
    std::string factoryId;
    std::string factoryName;
    std::string areaId;
    std::string areaName;
    std::string gatewayId;
    std::string gatewayName;
    std::string portId;
    std::string portName;
    int deviceId = 0;
    std::string deviceName;
    std::string deviceType;
    std::string pointKey;
    std::string pointName;
    std::string alarmType;
    std::string level;
    std::string state;
    double value = 0.0;
    double threshold = 0.0;
    std::string message;
};

struct CommandLogTarget
{
    std::string commandId;
    std::int64_t seq = 0;
    std::string commandType;
    std::string gatewayId;
    std::string portId;
    int deviceId = 0;
};

class PcDatabase
{
public:
    PcDatabase();
    ~PcDatabase();

    bool openDatabase(const std::string& dbPath);
    bool initTables();
    bool saveTelemetryPoints(const std::vector<TelemetryPoint>& points);
    bool savePointConfigs(const std::vector<PointConfig>& configs);
    bool upsertDevice(const DeviceRecord& device);
    bool upsertGatewayStatus(const GatewayStatus& gateway);
    bool updateGatewayHeartbeat(const std::string& gatewayId,
                                std::int64_t heartbeatTimeMs,
                                const std::string& status);
    bool upsertGatewayPort(const GatewayPort& port);
    bool isGatewayPortConnected(const std::string& gatewayId,
                                const std::string& portId);
    bool createCommandLog(const std::string& commandId,
                          std::int64_t seq,
                          const std::string& commandType,
                          const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId,
                          std::int64_t createTimeMs);
    bool updateCommandLogBySeq(std::int64_t seq,
                               const std::string& status,
                               const std::string& reason,
                               const std::string& message,
                               std::int64_t finishTimeMs);
    bool queryCommandTargetBySeq(std::int64_t seq, CommandLogTarget& target);
    std::vector<CommandLogTarget> collectCommandTimeouts(std::int64_t nowMs, std::int64_t timeoutMs);
    bool updateDeviceOnlineFromTelemetry(const TelemetryPoint& point);
    int markOfflineDevices(std::int64_t nowMs, std::int64_t timeoutMs);
    int markStaleGateways(std::int64_t nowMs, std::int64_t timeoutMs);
    std::vector<GatewayStatus> queryGatewayStatuses();
    std::vector<GatewayPort> queryGatewayPorts();
    std::vector<DeviceRecord> queryDevices();
    std::vector<TelemetryPoint> queryLatestPoints();
    std::vector<TelemetryPoint> queryHistoryPoints(const std::string& pointId,
                                                   std::int64_t startMs,
                                                   std::int64_t endMs,
                                                   int limit,
                                                   bool* ok = nullptr,
                                                   std::string* reason = nullptr);
    bool deleteDeviceData(const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId);
    bool deviceExists(const std::string& gatewayId,
                      const std::string& portId,
                      int deviceId) const;
    int deviceCount() const;
    bool deleteMasterData(const std::string& gatewayId,
                          const std::string& portId);
    bool clearRuntimeData();
    bool clearAllData();
    bool clearRecoveredAlarms();
    bool saveAlarmEvent(const AlarmEvent& event);
    bool replaceSelectedDeviceConfig(const std::string& gatewayId,
                                     const std::vector<DbSelectedDevice>& selectedDevices,
                                     const std::vector<GatewayPort>& ports,
                                     const std::vector<ConfigSnapshotDevice>& devices,
                                     const std::vector<PointConfig>& pointConfigs);
    bool upsertGatewayConfigSnapshot(const std::string& gatewayId,
                                     const std::vector<GatewayPort>& ports,
                                     const std::vector<ConfigSnapshotDevice>& devices,
                                     const std::vector<PointConfig>& pointConfigs,
                                     bool fullSnapshot);

    bool isOpen() const;
    void close();

private:
    bool execSql(const std::string& sql);
    bool deleteRowsByDevice(const std::string& table,
                            const std::string& gatewayId,
                            const std::string& portId,
                            int deviceId);
    bool deleteRowsByMaster(const std::string& table,
                            const std::string& gatewayId,
                            const std::string& portId);
    bool ensureDirectoryForFile(const std::string& dbPath);
    bool saveLatestPoint(const TelemetryPoint& point);
    bool saveHistoryPoint(const TelemetryPoint& point);
    bool savePointConfig(const PointConfig& config);
    bool upsertDeviceStatus(const DeviceRecord& device);

private:
    sqlite3* m_db = nullptr;
};

#endif // PC_DATABASE_HPP
