#ifndef PC_DATABASE_HPP
#define PC_DATABASE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "model/PointConfig.hpp"
#include "model/DeviceRecord.hpp"
#include "model/TelemetryPoint.hpp"

struct sqlite3;

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
    bool updateDeviceOnlineFromTelemetry(const TelemetryPoint& point);
    int markOfflineDevices(std::int64_t nowMs, std::int64_t timeoutMs);
    std::vector<DeviceRecord> queryDevices();
    std::vector<TelemetryPoint> queryLatestPoints();
    std::vector<TelemetryPoint> queryHistoryPoints(const std::string& pointId,
                                                   std::int64_t startMs,
                                                   std::int64_t endMs,
                                                   int limit);
    bool deleteDeviceData(const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId);
    bool deleteMasterData(const std::string& gatewayId,
                          const std::string& portId);
    bool clearRecoveredAlarms();

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
