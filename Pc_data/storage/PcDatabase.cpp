#include "PcDatabase.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <set>

#include "DatabaseSchema.hpp"
#include "common/JsonUtils.hpp"
#include "sqlite3.h"

static void bindText(sqlite3_stmt* stmt, int index, const std::string& value);
static std::string columnText(const unsigned char* value);
static bool execSqlQuiet(sqlite3* db, const std::string& sql);
static std::string telemetryDeviceKey(const TelemetryPoint& point);

PcDatabase::PcDatabase()
{
}

PcDatabase::~PcDatabase()
{
    close();
}

bool PcDatabase::openDatabase(const std::string& dbPath)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_db) {
        return true;
    }

    if (!ensureDirectoryForFile(dbPath)) {
        return false;
    }

    int rc = sqlite3_open_v2(dbPath.c_str(),
                             &m_db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                             nullptr);

    if (rc != SQLITE_OK) {
        std::cerr << "Open database failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;

        close();
        return false;
    }

    std::cout << "Database opened: " << dbPath << std::endl;

    sqlite3_busy_timeout(m_db, 3000);
    execSql("PRAGMA foreign_keys = ON;");
    execSql("PRAGMA journal_mode = WAL;");
    execSql("PRAGMA synchronous = NORMAL;");

    return true;
}

bool PcDatabase::initTables()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        std::cerr << "Database is not open." << std::endl;
        return false;
    }

    for (const auto& sql : DatabaseSchema::tableSqlList()) {
        if (!execSql(sql)) {
            return false;
        }
    }

    const char* migrations[] = {
        "ALTER TABLE device ADD COLUMN deleted INTEGER DEFAULT 0;",
        "ALTER TABLE device ADD COLUMN deleted_time_ms INTEGER DEFAULT 0;",
        "ALTER TABLE device ADD COLUMN status TEXT DEFAULT 'active';"
    };
    for (const char* sql : migrations) {
        execSqlQuiet(m_db, sql);
    }

    for (const auto& sql : DatabaseSchema::indexSqlList()) {
        if (!execSql(sql)) {
            return false;
        }
    }

    std::cout << "Database tables initialized." << std::endl;
    return true;
}

bool PcDatabase::isOpen() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return m_db != nullptr;
}

bool PcDatabase::saveTelemetryPoints(const std::vector<TelemetryPoint>& points)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        std::cerr << "Database is not open, skip telemetry save." << std::endl;
        return false;
    }

    if (points.empty()) {
        return true;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = true;
    std::vector<TelemetryPoint> acceptedPoints;
    acceptedPoints.reserve(points.size());
    std::map<std::string, TelemetryPoint> incomingDevices;

    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }

        if (!deviceExists(point.gatewayId, point.portId, point.deviceId)) {
            std::cout << "Skip telemetry save for unknown device, gateway: "
                      << point.gatewayId
                      << ", port: " << point.portId
                      << ", device: " << point.deviceId
                      << ", point: " << point.pointId << std::endl;
            continue;
        }

        acceptedPoints.push_back(point);
        incomingDevices[telemetryDeviceKey(point)] = point;
    }

    for (const auto& item : incomingDevices) {
        const TelemetryPoint& point = item.second;
        if (!deleteLatestPointsForDevice(point.gatewayId, point.portId, point.deviceId)) {
            ok = false;
            break;
        }
    }

    for (const auto& point : acceptedPoints) {
        if (!ok) {
            break;
        }

        if (!saveLatestPoint(point) || !saveHistoryPoint(point)) {
            ok = false;
            break;
        }

        if (!updateDeviceOnlineFromTelemetry(point)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    std::cout << "Database telemetry save "
              << (ok ? "ok" : "failed")
              << ", point count: "
              << points.size()
              << std::endl;

    return ok;
}

bool PcDatabase::savePointConfigs(const std::vector<PointConfig>& configs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        std::cerr << "Database is not open, skip point config save." << std::endl;
        return false;
    }

    if (configs.empty()) {
        return true;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = true;
    for (const auto& config : configs) {
        if (config.pointId.empty()) {
            continue;
        }
        if (!deviceExists(config.gatewayId, config.portId, config.deviceId)) {
            std::cout << "Skip point_config save for inactive device, gateway: "
                      << config.gatewayId << ", port: " << config.portId
                      << ", device: " << config.deviceId
                      << ", pointKey: " << config.pointKey << std::endl;
            continue;
        }

        if (!savePointConfig(config)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    std::cout << "Database point config save "
              << (ok ? "ok" : "failed")
              << ", config count: "
              << configs.size()
              << std::endl;

    return ok;
}

bool PcDatabase::upsertDevice(const DeviceRecord& device, bool allowDeletedReactivate)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0 ||
        device.deviceType.empty()) {
        return false;
    }

    if (deviceDeleted(device.gatewayId, device.portId, device.deviceId) && !allowDeletedReactivate) {
        std::cout << "Skip upsert tombstoned device, gateway: "
                  << device.gatewayId << ", port: " << device.portId
                  << ", device: " << device.deviceId << std::endl;
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = true;

    static const char* sql =
        "INSERT INTO device ("
        "factory_id,factory_name,area_id,area_name,gateway_id,gateway_name,"
        "port_id,port_name,device_id,device_name,device_type,poll_interval_ms,"
        "expect_telemetry,enabled,deleted,deleted_time_ms,status,create_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(gateway_id, port_id, device_id) DO UPDATE SET "
        "factory_id=excluded.factory_id,"
        "factory_name=excluded.factory_name,"
        "area_id=excluded.area_id,"
        "area_name=excluded.area_name,"
        "gateway_name=excluded.gateway_name,"
        "port_name=excluded.port_name,"
        "device_name=excluded.device_name,"
        "device_type=excluded.device_type,"
        "poll_interval_ms=excluded.poll_interval_ms,"
        "expect_telemetry=excluded.expect_telemetry,"
        "enabled=excluded.enabled,"
        "deleted=0,"
        "deleted_time_ms=0,"
        "status='active',"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare upsert device failed: " << sqlite3_errmsg(m_db) << std::endl;
        execSql("ROLLBACK;");
        return false;
    }

    const std::int64_t nowMs = device.updateTimeMs > 0 ? device.updateTimeMs : device.createTimeMs;

    bindText(stmt, 1, device.factoryId);
    bindText(stmt, 2, device.factoryName);
    bindText(stmt, 3, device.areaId);
    bindText(stmt, 4, device.areaName);
    bindText(stmt, 5, device.gatewayId);
    bindText(stmt, 6, device.gatewayName);
    bindText(stmt, 7, device.portId);
    bindText(stmt, 8, device.portName);
    sqlite3_bind_int(stmt, 9, device.deviceId);
    bindText(stmt, 10, device.deviceName);
    bindText(stmt, 11, device.deviceType);
    sqlite3_bind_int(stmt, 12, device.pollIntervalMs);
    sqlite3_bind_int(stmt, 13, device.expectTelemetry ? 1 : 0);
    sqlite3_bind_int(stmt, 14, device.enabled ? 1 : 0);
    sqlite3_bind_int(stmt, 15, 0);
    sqlite3_bind_int64(stmt, 16, 0);
    bindText(stmt, 17, "active");
    sqlite3_bind_int64(stmt, 18, device.createTimeMs > 0 ? device.createTimeMs : nowMs);
    sqlite3_bind_int64(stmt, 19, nowMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Upsert device failed: " << sqlite3_errmsg(m_db) << std::endl;
        ok = false;
    }

    if (ok) {
        ok = upsertDeviceStatus(device);
    }

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    return ok;
}

bool PcDatabase::updateDeviceOnlineFromTelemetry(const TelemetryPoint& point)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || point.gatewayId.empty() || point.portId.empty() || point.deviceId <= 0) {
        return false;
    }

    if (!deviceExists(point.gatewayId, point.portId, point.deviceId)) {
        std::cout << "Skip device_status update for unknown telemetry device, gateway: "
                  << point.gatewayId
                  << ", port: " << point.portId
                  << ", device: " << point.deviceId << std::endl;
        return true;
    }

    DeviceRecord device;
    device.factoryId = point.factoryId;
    device.factoryName = point.factoryName;
    device.areaId = point.areaId;
    device.areaName = point.areaName;
    device.gatewayId = point.gatewayId;
    device.gatewayName = point.gatewayName;
    device.portId = point.portId;
    device.portName = point.portName;
    device.deviceId = point.deviceId;
    device.deviceName = point.deviceName;
    device.deviceType = point.deviceType.empty() ? "unknown" : point.deviceType;
    device.pollIntervalMs = 1000;
    device.expectTelemetry = device.deviceType != "relay";
    device.enabled = true;
    device.status = point.valid ? "online" :
        (point.errorMessage == "device_offline" ? "offline" : "error");
    const std::int64_t receiveTimeMs = currentTimeMs();
    // point.timestampMs is board collection time for latest/history charts.
    // PC receive time must drive lastSeen/update/offline timeout decisions.
    device.lastSeenMs = receiveTimeMs;
    device.lastOfflineMs = device.status == "offline" ? receiveTimeMs : 0;
    device.statusReason = point.valid ? "" : point.errorMessage;
    device.createTimeMs = receiveTimeMs;
    device.updateTimeMs = receiveTimeMs;

    return upsertDeviceStatus(device);
}

int PcDatabase::markOfflineDevices(std::int64_t nowMs, std::int64_t timeoutMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || nowMs <= 0 || timeoutMs <= 0) {
        return 0;
    }

    static const char* sql =
        "UPDATE device_status "
        "SET status='offline', last_offline_ms=?, status_reason='telemetry_timeout', update_time_ms=? "
        "WHERE status != 'offline' AND EXISTS ("
        "  SELECT 1 FROM device d "
        "  WHERE d.gateway_id=device_status.gateway_id "
        "    AND d.port_id=device_status.port_id "
        "    AND d.device_id=device_status.device_id "
        "    AND d.enabled=1 AND COALESCE(d.deleted,0)=0 AND COALESCE(d.status,'active')='active'"
        ") AND last_seen_ms > 0 AND (? - last_seen_ms) > ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare mark offline failed: " << sqlite3_errmsg(m_db) << std::endl;
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, nowMs);
    sqlite3_bind_int64(stmt, 2, nowMs);
    sqlite3_bind_int64(stmt, 3, nowMs);
    sqlite3_bind_int64(stmt, 4, timeoutMs);

    rc = sqlite3_step(stmt);
    const int changed = sqlite3_changes(m_db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Mark offline failed: " << sqlite3_errmsg(m_db) << std::endl;
        return 0;
    }

    return changed;
}

int PcDatabase::markStaleGateways(std::int64_t nowMs, std::int64_t timeoutMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || nowMs <= 0 || timeoutMs <= 0) {
        return 0;
    }

    static const char* sql =
        "UPDATE gateway_status "
        "SET status='stale', update_time_ms=? "
        "WHERE status='online' "
        "AND last_heartbeat_time_ms > 0 "
        "AND (? - last_heartbeat_time_ms) > ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare mark stale gateways failed: " << sqlite3_errmsg(m_db) << std::endl;
        return 0;
    }

    sqlite3_bind_int64(stmt, 1, nowMs);
    sqlite3_bind_int64(stmt, 2, nowMs);
    sqlite3_bind_int64(stmt, 3, timeoutMs);

    rc = sqlite3_step(stmt);
    const int changed = sqlite3_changes(m_db);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Mark stale gateways failed: " << sqlite3_errmsg(m_db) << std::endl;
        return 0;
    }

    return changed;
}

std::vector<GatewayStatus> PcDatabase::queryGatewayStatuses()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<GatewayStatus> gateways;
    if (!m_db) {
        return gateways;
    }

    static const char* sql =
        "SELECT gateway_id,gateway_name,factory_id,area_id,status,"
        "last_register_time_ms,last_heartbeat_time_ms,update_time_ms "
        "FROM gateway_status ORDER BY gateway_id;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query gateway_status failed: " << sqlite3_errmsg(m_db) << std::endl;
        return gateways;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const auto textColumn = [stmt](int column) -> std::string {
            const unsigned char* text = sqlite3_column_text(stmt, column);
            return text ? reinterpret_cast<const char*>(text) : "";
        };

        GatewayStatus gateway;
        gateway.gatewayId = textColumn(0);
        gateway.gatewayName = textColumn(1);
        gateway.factoryId = textColumn(2);
        gateway.areaId = textColumn(3);
        gateway.status = textColumn(4);
        gateway.lastRegisterTimeMs = sqlite3_column_int64(stmt, 5);
        gateway.lastHeartbeatTimeMs = sqlite3_column_int64(stmt, 6);
        gateway.updateTimeMs = sqlite3_column_int64(stmt, 7);
        gateways.push_back(gateway);
    }

    sqlite3_finalize(stmt);
    return gateways;
}

std::vector<GatewayPort> PcDatabase::queryGatewayPorts()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<GatewayPort> ports;
    if (!m_db) {
        return ports;
    }

    static const char* sql =
        "SELECT gateway_id,port_id,port_name,slot,device_path,baud,status,"
        "last_register_time_ms,update_time_ms "
        "FROM gateway_port ORDER BY gateway_id,slot,port_id;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query gateway_port failed: " << sqlite3_errmsg(m_db) << std::endl;
        return ports;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        const auto textColumn = [stmt](int column) -> std::string {
            const unsigned char* text = sqlite3_column_text(stmt, column);
            return text ? reinterpret_cast<const char*>(text) : "";
        };

        GatewayPort port;
        port.gatewayId = textColumn(0);
        port.portId = textColumn(1);
        port.portName = textColumn(2);
        port.slot = sqlite3_column_int(stmt, 3);
        port.devicePath = textColumn(4);
        port.baud = sqlite3_column_int(stmt, 5);
        port.status = textColumn(6);
        port.lastRegisterTimeMs = sqlite3_column_int64(stmt, 7);
        port.updateTimeMs = sqlite3_column_int64(stmt, 8);
        ports.push_back(port);
    }

    sqlite3_finalize(stmt);
    return ports;
}

std::vector<DeviceRecord> PcDatabase::queryDevices()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<DeviceRecord> devices;
    if (!m_db) {
        return devices;
    }

    static const char* sql =
        "SELECT d.factory_id,d.factory_name,d.area_id,d.area_name,"
        "d.gateway_id,d.gateway_name,d.port_id,d.port_name,d.device_id,"
        "d.device_name,d.device_type,d.poll_interval_ms,d.expect_telemetry,"
        "d.enabled,d.create_time_ms,d.update_time_ms,"
        "COALESCE(s.status,'unknown'),COALESCE(s.last_seen_ms,0),"
        "COALESCE(s.last_offline_ms,0),COALESCE(s.status_reason,''),"
        "COALESCE(s.update_time_ms,0) "
        "FROM device d "
        "LEFT JOIN device_status s "
        "ON s.gateway_id=d.gateway_id AND s.port_id=d.port_id AND s.device_id=d.device_id "
        "WHERE d.enabled=1 AND COALESCE(d.deleted,0)=0 AND COALESCE(d.status,'active')='active' "
        "ORDER BY d.gateway_id,d.port_id,d.device_id;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query devices failed: " << sqlite3_errmsg(m_db) << std::endl;
        return devices;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        DeviceRecord device;
        const auto textColumn = [stmt](int column) -> std::string {
            const unsigned char* text = sqlite3_column_text(stmt, column);
            return text ? reinterpret_cast<const char*>(text) : "";
        };

        device.factoryId = textColumn(0);
        device.factoryName = textColumn(1);
        device.areaId = textColumn(2);
        device.areaName = textColumn(3);
        device.gatewayId = textColumn(4);
        device.gatewayName = textColumn(5);
        device.portId = textColumn(6);
        device.portName = textColumn(7);
        device.deviceId = sqlite3_column_int(stmt, 8);
        device.deviceName = textColumn(9);
        device.deviceType = textColumn(10);
        device.pollIntervalMs = sqlite3_column_int(stmt, 11);
        device.expectTelemetry = sqlite3_column_int(stmt, 12) != 0;
        device.enabled = sqlite3_column_int(stmt, 13) != 0;
        device.createTimeMs = sqlite3_column_int64(stmt, 14);
        device.updateTimeMs = sqlite3_column_int64(stmt, 15);
        device.status = textColumn(16);
        device.lastSeenMs = sqlite3_column_int64(stmt, 17);
        device.lastOfflineMs = sqlite3_column_int64(stmt, 18);
        device.statusReason = textColumn(19);
        const std::int64_t statusUpdate = sqlite3_column_int64(stmt, 20);
        if (statusUpdate > device.updateTimeMs) {
            device.updateTimeMs = statusUpdate;
        }

        devices.push_back(device);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Query devices failed: " << sqlite3_errmsg(m_db) << std::endl;
        devices.clear();
    }

    sqlite3_finalize(stmt);
    return devices;
}

std::vector<PointConfig> PcDatabase::queryPointConfigs()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<PointConfig> configs;
    if (!m_db) {
        return configs;
    }

    static const char* sql =
        "SELECT point_id,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,enable_alarm,"
        "alarm_low,alarm_high,enabled,create_time_ms,update_time_ms "
        "FROM point_config WHERE enabled=1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query point_config failed: " << sqlite3_errmsg(m_db) << std::endl;
        return configs;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        PointConfig config;
        const auto textColumn = [stmt](int column) -> std::string {
            const unsigned char* text = sqlite3_column_text(stmt, column);
            return text ? reinterpret_cast<const char*>(text) : "";
        };

        config.pointId = textColumn(0);
        config.factoryId = textColumn(1);
        config.factoryName = textColumn(2);
        config.areaId = textColumn(3);
        config.areaName = textColumn(4);
        config.gatewayId = textColumn(5);
        config.gatewayName = textColumn(6);
        config.portId = textColumn(7);
        config.portName = textColumn(8);
        config.deviceId = sqlite3_column_int(stmt, 9);
        config.deviceName = textColumn(10);
        config.deviceType = textColumn(11);
        config.pointKey = textColumn(12);
        config.pointName = textColumn(13);
        config.unit = textColumn(14);
        config.valueType = textColumn(15);
        config.enableAlarm = sqlite3_column_int(stmt, 16) != 0;
        config.hasAlarmLow = sqlite3_column_type(stmt, 17) != SQLITE_NULL;
        config.alarmLow = config.hasAlarmLow ? sqlite3_column_double(stmt, 17) : 0.0;
        config.hasAlarmHigh = sqlite3_column_type(stmt, 18) != SQLITE_NULL;
        config.alarmHigh = config.hasAlarmHigh ? sqlite3_column_double(stmt, 18) : 0.0;
        config.enabled = sqlite3_column_int(stmt, 19) != 0;
        config.timestampMs = sqlite3_column_int64(stmt, 21);
        if (config.timestampMs <= 0) {
            config.timestampMs = sqlite3_column_int64(stmt, 20);
        }

        if (!config.gatewayId.empty() && !config.portId.empty() &&
            config.deviceId > 0 && !config.pointKey.empty()) {
            configs.push_back(config);
        }
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Query point_config failed: " << sqlite3_errmsg(m_db) << std::endl;
        configs.clear();
    }

    sqlite3_finalize(stmt);
    return configs;
}

std::vector<TelemetryPoint> PcDatabase::queryLatestPoints()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<TelemetryPoint> points;

    if (!m_db) {
        return points;
    }

    static const char* sql =
        "SELECT point_id,timestamp_ms,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,number_value,"
        "text_value,valid,error_message "
        "FROM latest_point;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query latest failed: " << sqlite3_errmsg(m_db) << std::endl;
        return points;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        TelemetryPoint point;

        const unsigned char* pointId = sqlite3_column_text(stmt, 0);
        point.pointId = pointId ? reinterpret_cast<const char*>(pointId) : "";
        point.timestampMs = sqlite3_column_int64(stmt, 1);

        const unsigned char* factoryId = sqlite3_column_text(stmt, 2);
        point.factoryId = factoryId ? reinterpret_cast<const char*>(factoryId) : "";
        const unsigned char* factoryName = sqlite3_column_text(stmt, 3);
        point.factoryName = factoryName ? reinterpret_cast<const char*>(factoryName) : "";
        const unsigned char* areaId = sqlite3_column_text(stmt, 4);
        point.areaId = areaId ? reinterpret_cast<const char*>(areaId) : "";
        const unsigned char* areaName = sqlite3_column_text(stmt, 5);
        point.areaName = areaName ? reinterpret_cast<const char*>(areaName) : "";
        const unsigned char* gatewayId = sqlite3_column_text(stmt, 6);
        point.gatewayId = gatewayId ? reinterpret_cast<const char*>(gatewayId) : "";
        const unsigned char* gatewayName = sqlite3_column_text(stmt, 7);
        point.gatewayName = gatewayName ? reinterpret_cast<const char*>(gatewayName) : "";
        const unsigned char* portId = sqlite3_column_text(stmt, 8);
        point.portId = portId ? reinterpret_cast<const char*>(portId) : "";
        const unsigned char* portName = sqlite3_column_text(stmt, 9);
        point.portName = portName ? reinterpret_cast<const char*>(portName) : "";

        point.deviceId = sqlite3_column_int(stmt, 10);
        const unsigned char* deviceName = sqlite3_column_text(stmt, 11);
        point.deviceName = deviceName ? reinterpret_cast<const char*>(deviceName) : "";
        const unsigned char* deviceType = sqlite3_column_text(stmt, 12);
        point.deviceType = deviceType ? reinterpret_cast<const char*>(deviceType) : "";
        const unsigned char* pointKey = sqlite3_column_text(stmt, 13);
        point.pointKey = pointKey ? reinterpret_cast<const char*>(pointKey) : "";
        const unsigned char* pointName = sqlite3_column_text(stmt, 14);
        point.pointName = pointName ? reinterpret_cast<const char*>(pointName) : "";
        const unsigned char* unit = sqlite3_column_text(stmt, 15);
        point.unit = unit ? reinterpret_cast<const char*>(unit) : "";

        const unsigned char* valueType = sqlite3_column_text(stmt, 16);
        const std::string valueTypeText = valueType ? reinterpret_cast<const char*>(valueType) : "";
        if (valueTypeText == "text") {
            point.valueType = PointValueType::Text;
        } else if (valueTypeText == "boolean") {
            point.valueType = PointValueType::Boolean;
        } else {
            point.valueType = PointValueType::Number;
        }

        point.numberValue = sqlite3_column_double(stmt, 17);
        const unsigned char* textValue = sqlite3_column_text(stmt, 18);
        point.textValue = textValue ? reinterpret_cast<const char*>(textValue) : "";
        point.valid = sqlite3_column_int(stmt, 19) != 0;
        const unsigned char* errorMessage = sqlite3_column_text(stmt, 20);
        point.errorMessage = errorMessage ? reinterpret_cast<const char*>(errorMessage) : "";

        if (!point.pointId.empty()) {
            points.push_back(point);
        }
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Query latest_point failed: " << sqlite3_errmsg(m_db) << std::endl;
        points.clear();
    }

    sqlite3_finalize(stmt);
    return points;
}

bool PcDatabase::deleteLatestPointsForDevice(const std::string& gatewayId,
                                             const std::string& portId,
                                             int deviceId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    return deleteRowsByDevice("latest_point", gatewayId, portId, deviceId);
}

std::vector<TelemetryPoint> PcDatabase::queryHistoryPoints(const std::string& pointId,
                                                           std::int64_t startMs,
                                                           std::int64_t endMs,
                                                           int limit,
                                                           bool* ok,
                                                           std::string* reason)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<TelemetryPoint> points;
    if (ok) {
        *ok = false;
    }
    if (reason) {
        reason->clear();
    }

    if (pointId.empty()) {
        if (reason) {
            *reason = "invalid_point_id";
        }
        return points;
    }
    if (!m_db) {
        if (reason) {
            *reason = "db_not_open";
        }
        return points;
    }

    if (startMs < 0) {
        startMs = 0;
    }

    if (endMs <= 0) {
        endMs = std::numeric_limits<std::int64_t>::max();
    }

    if (endMs < startMs) {
        if (reason) {
            *reason = "invalid_time_range";
        }
        return points;
    }

    if (limit <= 0) {
        limit = 1000;
    } else if (limit > 5000) {
        limit = 5000;
    }

    static const char* sql =
        "SELECT timestamp_ms, number_value, text_value, valid "
        "FROM telemetry_history "
        "WHERE point_id = ? AND timestamp_ms >= ? AND timestamp_ms <= ? "
        "ORDER BY timestamp_ms ASC "
        "LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query history failed: " << sqlite3_errmsg(m_db) << std::endl;
        if (reason) {
            *reason = "db_query_failed";
        }
        return points;
    }

    sqlite3_bind_text(stmt, 1, pointId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, startMs);
    sqlite3_bind_int64(stmt, 3, endMs);
    sqlite3_bind_int(stmt, 4, limit);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        TelemetryPoint point;
        point.pointId = pointId;
        point.timestampMs = sqlite3_column_int64(stmt, 0);
        point.numberValue = sqlite3_column_double(stmt, 1);

        const unsigned char* text = sqlite3_column_text(stmt, 2);
        point.textValue = text ? reinterpret_cast<const char*>(text) : "";
        point.valid = sqlite3_column_int(stmt, 3) != 0;

        points.push_back(point);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Query telemetry_history failed: " << sqlite3_errmsg(m_db) << std::endl;
        points.clear();
        if (reason) {
            *reason = "db_query_failed";
        }
        sqlite3_finalize(stmt);
        return points;
    }

    if (ok) {
        *ok = true;
    }
    if (reason && points.empty()) {
        *reason = "no_data";
    }

    sqlite3_finalize(stmt);
    return points;
}

bool PcDatabase::deviceExists(const std::string& gatewayId,
                              const std::string& portId,
                              int deviceId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    static const char* sql =
        "SELECT 1 FROM device "
        "WHERE gateway_id=? AND port_id=? AND device_id=? "
        "AND enabled=1 AND COALESCE(deleted,0)=0 AND COALESCE(status,'active')='active' LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare device exists failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, deviceId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

bool PcDatabase::deviceDeleted(const std::string& gatewayId,
                               const std::string& portId,
                               int deviceId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    static const char* sql =
        "SELECT 1 FROM device "
        "WHERE gateway_id=? AND port_id=? AND device_id=? "
        "AND (COALESCE(deleted,0)!=0 OR COALESCE(status,'active')='deleted') LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare device deleted failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, deviceId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

bool PcDatabase::reactivateDeletedDevice(const std::string& gatewayId,
                                         const std::string& portId,
                                         int deviceId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    static const char* sql =
        "UPDATE device SET deleted=0, deleted_time_ms=0, status='active', enabled=1, update_time_ms=? "
        "WHERE gateway_id=? AND port_id=? AND device_id=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare reactivate device failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, currentTimeMs());
    sqlite3_bind_text(stmt, 2, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, deviceId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::string PcDatabase::queryDeviceType(const std::string& gatewayId,
                                        const std::string& portId,
                                        int deviceId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return std::string();
    }

    static const char* sql =
        "SELECT device_type FROM device "
        "WHERE gateway_id=? AND port_id=? AND device_id=? "
        "AND enabled=1 AND COALESCE(deleted,0)=0 AND COALESCE(status,'active')='active' LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query device type failed: " << sqlite3_errmsg(m_db) << std::endl;
        return std::string();
    }

    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, deviceId);

    std::string deviceType;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        deviceType = columnText(sqlite3_column_text(stmt, 0));
    }

    sqlite3_finalize(stmt);
    return deviceType;
}

bool PcDatabase::pointConfigExists(const std::string& gatewayId,
                                   const std::string& portId,
                                   int deviceId,
                                   const std::string& pointKey) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0 || pointKey.empty()) {
        return false;
    }
    if (!deviceExists(gatewayId, portId, deviceId)) {
        return false;
    }

    static const char* sql =
        "SELECT 1 FROM point_config "
        "WHERE gateway_id=? AND port_id=? AND device_id=? AND point_key=? "
        "LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare point_config exists failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, deviceId);
    sqlite3_bind_text(stmt, 4, pointKey.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

int PcDatabase::deviceCount() const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        return 0;
    }

    static const char* sql = "SELECT COUNT(1) FROM device;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare device count failed: " << sqlite3_errmsg(m_db) << std::endl;
        return 0;
    }

    rc = sqlite3_step(stmt);
    const int count = rc == SQLITE_ROW ? sqlite3_column_int(stmt, 0) : 0;
    sqlite3_finalize(stmt);
    return count;
}

bool PcDatabase::deleteDeviceData(const std::string& gatewayId,
                                  const std::string& portId,
                                  int deviceId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = markDeviceDeleted(gatewayId, portId, deviceId, currentTimeMs());

    const char* tables[] = {
        "device_status", "latest_point", "point_config",
        "telemetry_history", "alarm_event"
    };
    for (const char* table : tables) {
        if (!ok) {
            break;
        }
        const int rows = deleteRowsByDevice(table, gatewayId, portId, deviceId) ? sqlite3_changes(m_db) : -1;
        std::cout << "Delete device table " << table << " rows=" << rows
                  << ", gateway: " << gatewayId
                  << ", port: " << portId
                  << ", device: " << deviceId << std::endl;
        if (rows < 0) {
            ok = false;
            break;
        }
    }

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    std::cout << "Delete device data "
              << (ok ? "ok" : "failed")
              << ", gateway: " << gatewayId
              << ", port: " << portId
              << ", device: " << deviceId
              << std::endl;

    return ok;
}

bool PcDatabase::deleteMasterData(const std::string& gatewayId,
                                  const std::string& portId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty()) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    const std::vector<DeviceRecord> devices = queryDevices();
    bool ok = true;
    const std::int64_t nowMs = currentTimeMs();
    for (const DeviceRecord& device : devices) {
        if (device.gatewayId == gatewayId && device.portId == portId) {
            ok = markDeviceDeleted(gatewayId, portId, device.deviceId, nowMs) && ok;
        }
    }

    ok = ok &&
              deleteRowsByMaster("device_status", gatewayId, portId) &&
              deleteRowsByMaster("latest_point", gatewayId, portId) &&
              deleteRowsByMaster("point_config", gatewayId, portId) &&
              deleteRowsByMaster("telemetry_history", gatewayId, portId) &&
              deleteRowsByMaster("alarm_event", gatewayId, portId);

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    std::cout << "Delete master data "
              << (ok ? "ok" : "failed")
              << ", gateway: " << gatewayId
              << ", port: " << portId
              << std::endl;

    return ok;
}

bool PcDatabase::clearRuntimeData()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = execSql("DELETE FROM alarm_event;") &&
              execSql("DELETE FROM telemetry_history;") &&
              execSql("DELETE FROM latest_point;") &&
              execSql("DELETE FROM command_log;");

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
    }

    std::cout << "Clear runtime data " << (ok ? "ok" : "failed") << std::endl;
    return ok;
}

bool PcDatabase::clearAllData()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    return clearRuntimeData();
}

bool PcDatabase::replaceSelectedDeviceConfig(const std::string& gatewayId,
                                             const std::vector<DbSelectedDevice>& selectedDevices,
                                             const std::vector<GatewayPort>& ports,
                                             const std::vector<ConfigSnapshotDevice>& devices,
                                             const std::vector<PointConfig>& pointConfigs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || selectedDevices.empty()) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = true;
    for (const DbSelectedDevice& selected : selectedDevices) {
        if (selected.portId.empty() || selected.deviceId <= 0) {
            continue;
        }

        ok = deleteRowsByDevice("device_status", gatewayId, selected.portId, selected.deviceId) &&
             deleteRowsByDevice("point_config", gatewayId, selected.portId, selected.deviceId);
        if (!ok) {
            break;
        }
    }

    if (ok) {
        ok = execSql("COMMIT;");
    } else {
        execSql("ROLLBACK;");
        return false;
    }

    for (const GatewayPort& port : ports) {
        if (port.gatewayId == gatewayId && !port.portId.empty()) {
            ok = upsertGatewayPort(port) && ok;
        }
    }

    for (const ConfigSnapshotDevice& snapshotDevice : devices) {
        const DeviceRecord& device = snapshotDevice.device;
        if (device.gatewayId == gatewayId && !device.portId.empty() && device.deviceId > 0) {
            if (deviceDeleted(device.gatewayId, device.portId, device.deviceId)) {
                std::cout << "Replace selected config skipped tombstoned device, gateway: "
                          << device.gatewayId << ", port: " << device.portId
                          << ", device: " << device.deviceId << std::endl;
                continue;
            }
            ok = upsertDevice(device, true) && ok;
        }
    }

    if (!pointConfigs.empty()) {
        std::vector<PointConfig> acceptedPointConfigs;
        for (const PointConfig& config : pointConfigs) {
            if (deviceExists(config.gatewayId, config.portId, config.deviceId)) {
                acceptedPointConfigs.push_back(config);
            }
        }
        ok = savePointConfigs(acceptedPointConfigs) && ok;
    }

    std::cout << "Replace selected config "
              << (ok ? "ok" : "failed")
              << ", gateway: " << gatewayId
              << ", selected: " << selectedDevices.size()
              << ", ports: " << ports.size()
              << ", devices: " << devices.size()
              << std::endl;

    return ok;
}


bool PcDatabase::upsertGatewayConfigSnapshot(const std::string& gatewayId,
                                             const std::vector<GatewayPort>& ports,
                                             const std::vector<ConfigSnapshotDevice>& devices,
                                             const std::vector<PointConfig>& pointConfigs,
                                             bool fullSnapshot)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty()) {
        return false;
    }

    bool ok = true;
    if (fullSnapshot) {
        std::set<std::pair<std::string, int> > snapshotDevices;
        for (const ConfigSnapshotDevice& snapshotDevice : devices) {
            const DeviceRecord& device = snapshotDevice.device;
            if (device.gatewayId == gatewayId && !device.portId.empty() && device.deviceId > 0) {
                snapshotDevices.insert(std::make_pair(device.portId, device.deviceId));
            }
        }

        const std::vector<DeviceRecord> existingDevices = queryDevices();
        for (const DeviceRecord& existing : existingDevices) {
            if (existing.gatewayId != gatewayId || existing.portId.empty() || existing.deviceId <= 0) {
                continue;
            }
            if (snapshotDevices.find(std::make_pair(existing.portId, existing.deviceId)) != snapshotDevices.end()) {
                continue;
            }
            ok = deleteDeviceData(existing.gatewayId, existing.portId, existing.deviceId) && ok;
            std::cout << "Config snapshot removed missing device, gateway: " << existing.gatewayId
                      << ", port: " << existing.portId
                      << ", device: " << existing.deviceId << std::endl;
        }
    }

    for (const GatewayPort& port : ports) {
        if (port.gatewayId == gatewayId && !port.portId.empty()) {
            ok = upsertGatewayPort(port) && ok;
        }
    }
    for (const ConfigSnapshotDevice& snapshotDevice : devices) {
        const DeviceRecord& device = snapshotDevice.device;
        if (device.gatewayId == gatewayId && !device.portId.empty() && device.deviceId > 0) {
            if (deviceDeleted(device.gatewayId, device.portId, device.deviceId)) {
                std::cout << "Config snapshot skipped tombstoned device, gateway: "
                          << device.gatewayId << ", port: " << device.portId
                          << ", device: " << device.deviceId << std::endl;
                continue;
            }
            ok = upsertDevice(device) && ok;
        }
    }
    if (!pointConfigs.empty()) {
        std::vector<PointConfig> acceptedPointConfigs;
        for (const PointConfig& config : pointConfigs) {
            if (deviceExists(config.gatewayId, config.portId, config.deviceId)) {
                acceptedPointConfigs.push_back(config);
            }
        }
        ok = savePointConfigs(acceptedPointConfigs) && ok;
    }

    std::cout << "Upsert config snapshot " << (ok ? "ok" : "failed")
              << ", gateway: " << gatewayId
              << ", ports: " << ports.size()
              << ", devices: " << devices.size()
              << std::endl;
    return ok;
}

bool PcDatabase::markDeviceDeleted(const std::string& gatewayId,
                                   const std::string& portId,
                                   int deviceId,
                                   std::int64_t deletedTimeMs)
{
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    static const char* sql =
        "UPDATE device SET deleted=1, deleted_time_ms=?, status='deleted', enabled=0, update_time_ms=? "
        "WHERE gateway_id=? AND port_id=? AND device_id=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare mark device deleted failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, deletedTimeMs);
    sqlite3_bind_int64(stmt, 2, deletedTimeMs);
    sqlite3_bind_text(stmt, 3, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, deviceId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

static bool execSqlQuiet(sqlite3* db, const std::string& sql)
{
    if (!db) {
        return false;
    }
    char* errorMessage = nullptr;
    const int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errorMessage);
    if (rc == SQLITE_OK) {
        return true;
    }
    if (errorMessage) {
        sqlite3_free(errorMessage);
    }
    return false;
}

bool PcDatabase::saveAlarmEvent(const AlarmEvent& event)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || event.alarmId.empty() || event.gatewayId.empty()) {
        return false;
    }

    const std::string pointId = event.alarmId;
    const std::string state = event.state.empty() ? "active" : event.state;
    const bool recovered = state == "recovered";
    const bool acknowledged = state == "acked" || state == "acknowledged";
    const std::int64_t nowMs = event.timestampMs > 0 ? event.timestampMs : currentTimeMs();
    const std::int64_t ackTimeMs = event.ackTimeMs > 0 ? event.ackTimeMs : (acknowledged ? nowMs : 0);

    static const char* updateSql =
        "UPDATE alarm_event SET "
        "factory_id=?,factory_name=?,area_id=?,area_name=?,gateway_id=?,gateway_name=?,"
        "port_id=?,port_name=?,device_id=?,device_name=?,device_type=?,point_key=?,point_name=?,"
        "alarm_level=?,alarm_message=?,recover_time_ms=?,ack_time_ms=?,status=?,acked=?,trigger_value=?,threshold_value=?,error_message=? "
        "WHERE point_id=? AND alarm_type=? AND status IN ('active','acked','acknowledged');";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, updateSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare alarm update failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    int i = 1;
    bindText(stmt, i++, event.factoryId);
    bindText(stmt, i++, event.factoryName);
    bindText(stmt, i++, event.areaId);
    bindText(stmt, i++, event.areaName);
    bindText(stmt, i++, event.gatewayId);
    bindText(stmt, i++, event.gatewayName);
    bindText(stmt, i++, event.portId);
    bindText(stmt, i++, event.portName);
    sqlite3_bind_int(stmt, i++, event.deviceId);
    bindText(stmt, i++, event.deviceName);
    bindText(stmt, i++, event.deviceType);
    bindText(stmt, i++, event.pointKey);
    bindText(stmt, i++, event.pointName);
    bindText(stmt, i++, event.level.empty() ? "warning" : event.level);
    bindText(stmt, i++, event.message);
    if (recovered) sqlite3_bind_int64(stmt, i++, nowMs); else sqlite3_bind_null(stmt, i++);
    if (ackTimeMs > 0) sqlite3_bind_int64(stmt, i++, ackTimeMs); else sqlite3_bind_null(stmt, i++);
    bindText(stmt, i++, state);
    sqlite3_bind_int(stmt, i++, acknowledged ? 1 : 0);
    sqlite3_bind_double(stmt, i++, event.value);
    sqlite3_bind_double(stmt, i++, event.threshold);
    bindText(stmt, i++, event.alarmId);
    bindText(stmt, i++, pointId);
    bindText(stmt, i++, event.alarmType.empty() ? "emergency" : event.alarmType);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Update alarm event failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    if (sqlite3_changes(m_db) > 0) {
        return true;
    }

    static const char* insertSql =
        "INSERT INTO alarm_event ("
        "point_id,factory_id,factory_name,area_id,area_name,gateway_id,gateway_name,port_id,port_name,"
        "device_id,device_name,device_type,point_key,point_name,alarm_type,alarm_level,alarm_message,"
        "start_time_ms,recover_time_ms,ack_time_ms,status,acked,trigger_value,threshold_value,error_message"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    rc = sqlite3_prepare_v2(m_db, insertSql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare alarm insert failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    i = 1;
    bindText(stmt, i++, pointId);
    bindText(stmt, i++, event.factoryId);
    bindText(stmt, i++, event.factoryName);
    bindText(stmt, i++, event.areaId);
    bindText(stmt, i++, event.areaName);
    bindText(stmt, i++, event.gatewayId);
    bindText(stmt, i++, event.gatewayName);
    bindText(stmt, i++, event.portId);
    bindText(stmt, i++, event.portName);
    sqlite3_bind_int(stmt, i++, event.deviceId);
    bindText(stmt, i++, event.deviceName);
    bindText(stmt, i++, event.deviceType);
    bindText(stmt, i++, event.pointKey);
    bindText(stmt, i++, event.pointName);
    bindText(stmt, i++, event.alarmType.empty() ? "emergency" : event.alarmType);
    bindText(stmt, i++, event.level.empty() ? "warning" : event.level);
    bindText(stmt, i++, event.message);
    sqlite3_bind_int64(stmt, i++, nowMs);
    if (recovered) sqlite3_bind_int64(stmt, i++, nowMs); else sqlite3_bind_null(stmt, i++);
    if (ackTimeMs > 0) sqlite3_bind_int64(stmt, i++, ackTimeMs); else sqlite3_bind_null(stmt, i++);
    bindText(stmt, i++, state);
    sqlite3_bind_int(stmt, i++, acknowledged ? 1 : 0);
    sqlite3_bind_double(stmt, i++, event.value);
    sqlite3_bind_double(stmt, i++, event.threshold);
    bindText(stmt, i++, event.alarmId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Insert alarm event failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    return true;
}

bool PcDatabase::clearRecoveredAlarms()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    static const char* sql =
        "DELETE FROM alarm_event "
        "WHERE status = 'recovered' OR status = '已恢复';";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare clear recovered alarms failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Clear recovered alarms failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    std::cout << "Clear recovered alarms ok" << std::endl;
    return true;
}

bool PcDatabase::clearAcknowledgedAlarms()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db) {
        return false;
    }

    static const char* sql =
        "DELETE FROM alarm_event "
        "WHERE status IN ('acked','acknowledged','已确认') OR acked=1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare clear acknowledged alarms failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Clear acknowledged alarms failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    std::cout << "Clear acknowledged alarms ok" << std::endl;
    return true;
}

bool PcDatabase::acknowledgeAlarm(const std::string& alarmId, std::int64_t ackTimeMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || alarmId.empty()) {
        return false;
    }

    const std::int64_t nowMs = ackTimeMs > 0 ? ackTimeMs : currentTimeMs();
    static const char* sql =
        "UPDATE alarm_event "
        "SET status='acknowledged', state='acknowledged', acked=1, ack_time_ms=? "
        "WHERE point_id=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare acknowledge alarm failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    sqlite3_bind_int64(stmt, 1, nowMs);
    bindText(stmt, 2, alarmId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Acknowledge alarm failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return false;
    }

    return sqlite3_changes(m_db) > 0;
}

std::vector<AlarmEvent> PcDatabase::queryAlarmEvents()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<AlarmEvent> events;
    if (!m_db) {
        return events;
    }

    static const char* sql =
        "SELECT point_id,start_time_ms,ack_time_ms,recover_time_ms,"
        "factory_id,factory_name,area_id,area_name,gateway_id,gateway_name,"
        "port_id,port_name,device_id,device_name,device_type,point_key,point_name,"
        "alarm_type,alarm_level,alarm_message,status,trigger_value,threshold_value,error_message "
        "FROM alarm_event ORDER BY start_time_ms DESC, alarm_id DESC;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query alarm events failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
        return events;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        AlarmEvent event;
        event.alarmId = columnText(sqlite3_column_text(stmt, 0));
        event.timestampMs = sqlite3_column_int64(stmt, 1);
        event.ackTimeMs = sqlite3_column_int64(stmt, 2);
        event.recoverTimeMs = sqlite3_column_int64(stmt, 3);
        event.factoryId = columnText(sqlite3_column_text(stmt, 4));
        event.factoryName = columnText(sqlite3_column_text(stmt, 5));
        event.areaId = columnText(sqlite3_column_text(stmt, 6));
        event.areaName = columnText(sqlite3_column_text(stmt, 7));
        event.gatewayId = columnText(sqlite3_column_text(stmt, 8));
        event.gatewayName = columnText(sqlite3_column_text(stmt, 9));
        event.portId = columnText(sqlite3_column_text(stmt, 10));
        event.portName = columnText(sqlite3_column_text(stmt, 11));
        event.deviceId = sqlite3_column_int(stmt, 12);
        event.deviceName = columnText(sqlite3_column_text(stmt, 13));
        event.deviceType = columnText(sqlite3_column_text(stmt, 14));
        event.pointKey = columnText(sqlite3_column_text(stmt, 15));
        event.pointName = columnText(sqlite3_column_text(stmt, 16));
        event.alarmType = columnText(sqlite3_column_text(stmt, 17));
        event.level = columnText(sqlite3_column_text(stmt, 18));
        event.message = columnText(sqlite3_column_text(stmt, 19));
        event.state = columnText(sqlite3_column_text(stmt, 20));
        if (event.state == "acked") {
            event.state = "acknowledged";
        }
        event.value = sqlite3_column_double(stmt, 21);
        event.threshold = sqlite3_column_double(stmt, 22);
        events.push_back(event);
    }

    if (rc != SQLITE_DONE) {
        std::cerr << "Query alarm events failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;
    }

    sqlite3_finalize(stmt);
    return events;
}

bool PcDatabase::upsertGatewayStatus(const GatewayStatus& gateway)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gateway.gatewayId.empty()) {
        return false;
    }

    static const char* sql =
        "INSERT INTO gateway_status ("
        "gateway_id,gateway_name,factory_id,area_id,status,"
        "last_register_time_ms,last_heartbeat_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?) "
        "ON CONFLICT(gateway_id) DO UPDATE SET "
        "gateway_name=excluded.gateway_name,"
        "factory_id=excluded.factory_id,"
        "area_id=excluded.area_id,"
        "status=excluded.status,"
        "last_register_time_ms=excluded.last_register_time_ms,"
        "last_heartbeat_time_ms=excluded.last_heartbeat_time_ms,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare upsert gateway_status failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gateway.gatewayId);
    bindText(stmt, 2, gateway.gatewayName);
    bindText(stmt, 3, gateway.factoryId);
    bindText(stmt, 4, gateway.areaId);
    bindText(stmt, 5, gateway.status.empty() ? std::string("online") : gateway.status);
    sqlite3_bind_int64(stmt, 6, gateway.lastRegisterTimeMs);
    sqlite3_bind_int64(stmt, 7, gateway.lastHeartbeatTimeMs);
    sqlite3_bind_int64(stmt, 8, gateway.updateTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool PcDatabase::upsertGatewayRegistry(const GatewayRegistry& registry)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || registry.gatewayId.empty()) {
        return false;
    }

    static const char* sql =
        "INSERT INTO gateway_registry ("
        "gateway_id,gateway_name,status,up_topic,cmd_topic,broadcast_topic,"
        "last_register_time_ms,last_heartbeat_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(gateway_id) DO UPDATE SET "
        "gateway_name=excluded.gateway_name,"
        "status=excluded.status,"
        "up_topic=excluded.up_topic,"
        "cmd_topic=excluded.cmd_topic,"
        "broadcast_topic=excluded.broadcast_topic,"
        "last_register_time_ms=excluded.last_register_time_ms,"
        "last_heartbeat_time_ms=excluded.last_heartbeat_time_ms,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare upsert gateway_registry failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, registry.gatewayId);
    bindText(stmt, 2, registry.gatewayName);
    bindText(stmt, 3, registry.status.empty() ? std::string("online") : registry.status);
    bindText(stmt, 4, registry.upTopic);
    bindText(stmt, 5, registry.cmdTopic);
    bindText(stmt, 6, registry.broadcastTopic);
    sqlite3_bind_int64(stmt, 7, registry.lastRegisterTimeMs);
    sqlite3_bind_int64(stmt, 8, registry.lastHeartbeatTimeMs);
    sqlite3_bind_int64(stmt, 9, registry.updateTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::string PcDatabase::queryGatewayCmdTopic(const std::string& gatewayId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GatewayRegistry registry;
    return queryGatewayRegistry(gatewayId, registry) ? registry.cmdTopic : std::string();
}

std::string PcDatabase::queryGatewayUpTopic(const std::string& gatewayId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    GatewayRegistry registry;
    return queryGatewayRegistry(gatewayId, registry) ? registry.upTopic : std::string();
}

bool PcDatabase::queryGatewayRegistry(const std::string& gatewayId, GatewayRegistry& registry)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty()) {
        return false;
    }

    static const char* sql =
        "SELECT gateway_id,gateway_name,status,up_topic,cmd_topic,broadcast_topic,"
        "last_register_time_ms,last_heartbeat_time_ms,update_time_ms "
        "FROM gateway_registry WHERE gateway_id=? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query gateway_registry failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gatewayId);
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        registry.gatewayId = columnText(sqlite3_column_text(stmt, 0));
        registry.gatewayName = columnText(sqlite3_column_text(stmt, 1));
        registry.status = columnText(sqlite3_column_text(stmt, 2));
        registry.upTopic = columnText(sqlite3_column_text(stmt, 3));
        registry.cmdTopic = columnText(sqlite3_column_text(stmt, 4));
        registry.broadcastTopic = columnText(sqlite3_column_text(stmt, 5));
        registry.lastRegisterTimeMs = sqlite3_column_int64(stmt, 6);
        registry.lastHeartbeatTimeMs = sqlite3_column_int64(stmt, 7);
        registry.updateTimeMs = sqlite3_column_int64(stmt, 8);
        found = true;
    }

    sqlite3_finalize(stmt);
    return found;
}

bool PcDatabase::gatewayRegistryExists(const std::string& gatewayId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty()) {
        return false;
    }

    static const char* sql =
        "SELECT 1 FROM gateway_registry WHERE gateway_id=? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare gateway_registry exists failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gatewayId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

std::vector<GatewayRegistry> PcDatabase::getAllGatewayRegistry()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<GatewayRegistry> rows;
    if (!m_db) {
        return rows;
    }

    static const char* sql =
        "SELECT gateway_id,gateway_name,status,up_topic,cmd_topic,broadcast_topic,"
        "last_register_time_ms,last_heartbeat_time_ms,update_time_ms "
        "FROM gateway_registry ORDER BY gateway_id;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query all gateway_registry failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return rows;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GatewayRegistry registry;
        registry.gatewayId = columnText(sqlite3_column_text(stmt, 0));
        registry.gatewayName = columnText(sqlite3_column_text(stmt, 1));
        registry.status = columnText(sqlite3_column_text(stmt, 2));
        registry.upTopic = columnText(sqlite3_column_text(stmt, 3));
        registry.cmdTopic = columnText(sqlite3_column_text(stmt, 4));
        registry.broadcastTopic = columnText(sqlite3_column_text(stmt, 5));
        registry.lastRegisterTimeMs = sqlite3_column_int64(stmt, 6);
        registry.lastHeartbeatTimeMs = sqlite3_column_int64(stmt, 7);
        registry.updateTimeMs = sqlite3_column_int64(stmt, 8);
        rows.push_back(registry);
    }

    sqlite3_finalize(stmt);
    return rows;
}

bool PcDatabase::updateGatewayRegistryHeartbeat(const std::string& gatewayId,
                                                std::int64_t /*heartbeatTimeMs*/,
                                                const std::string& status)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty()) {
        return false;
    }

    const std::int64_t receiveTimeMs = currentTimeMs();
    static const char* sql =
        "INSERT INTO gateway_registry ("
        "gateway_id,status,last_register_time_ms,last_heartbeat_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?) "
        "ON CONFLICT(gateway_id) DO UPDATE SET "
        "status=excluded.status,"
        "last_heartbeat_time_ms=excluded.last_heartbeat_time_ms,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare update gateway_registry heartbeat failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gatewayId);
    bindText(stmt, 2, status.empty() ? std::string("online") : status);
    sqlite3_bind_int64(stmt, 3, 0);
    sqlite3_bind_int64(stmt, 4, receiveTimeMs);
    sqlite3_bind_int64(stmt, 5, receiveTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool PcDatabase::updateGatewayHeartbeat(const std::string& gatewayId,
                                        std::int64_t /*heartbeatTimeMs*/,
                                        const std::string& status)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty()) {
        return false;
    }

    const std::int64_t receiveTimeMs = currentTimeMs();
    // Heartbeat payload timestampMs may be board time. Store PC receive time for stale checks.

    static const char* sql =
        "INSERT INTO gateway_status ("
        "gateway_id,status,last_register_time_ms,last_heartbeat_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?) "
        "ON CONFLICT(gateway_id) DO UPDATE SET "
        "status=excluded.status,"
        "last_heartbeat_time_ms=excluded.last_heartbeat_time_ms,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare update gateway heartbeat failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gatewayId);
    bindText(stmt, 2, status.empty() ? std::string("online") : status);
    sqlite3_bind_int64(stmt, 3, 0);
    sqlite3_bind_int64(stmt, 4, receiveTimeMs);
    sqlite3_bind_int64(stmt, 5, receiveTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool PcDatabase::upsertGatewayPort(const GatewayPort& port)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || port.gatewayId.empty() || port.portId.empty()) {
        return false;
    }

    static const char* sql =
        "INSERT INTO gateway_port ("
        "gateway_id,port_id,port_name,slot,device_path,baud,status,"
        "last_register_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(gateway_id, port_id) DO UPDATE SET "
        "port_name=excluded.port_name,"
        "slot=excluded.slot,"
        "device_path=excluded.device_path,"
        "baud=excluded.baud,"
        "status=excluded.status,"
        "last_register_time_ms=excluded.last_register_time_ms,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare upsert gateway_port failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, port.gatewayId);
    bindText(stmt, 2, port.portId);
    bindText(stmt, 3, port.portName);
    sqlite3_bind_int(stmt, 4, port.slot);
    bindText(stmt, 5, port.devicePath);
    sqlite3_bind_int(stmt, 6, port.baud);
    bindText(stmt, 7, port.status.empty() ? std::string("connected") : port.status);
    sqlite3_bind_int64(stmt, 8, port.lastRegisterTimeMs);
    sqlite3_bind_int64(stmt, 9, port.updateTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return rc == SQLITE_DONE;
}

bool PcDatabase::isGatewayPortConnected(const std::string& gatewayId,
                                        const std::string& portId)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty()) {
        return false;
    }

    static const char* sql =
        "SELECT status FROM gateway_port WHERE gateway_id=? AND port_id=? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        return false;
    }

    bindText(stmt, 1, gatewayId);
    bindText(stmt, 2, portId);

    bool connected = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* text = sqlite3_column_text(stmt, 0);
        connected = text && std::string(reinterpret_cast<const char*>(text)) == "connected";
    }

    sqlite3_finalize(stmt);
    return connected;
}

bool PcDatabase::gatewayPortExists(const std::string& gatewayId,
                                   const std::string& portId) const
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || gatewayId.empty() || portId.empty()) {
        return false;
    }

    static const char* sql =
        "SELECT 1 FROM gateway_port WHERE gateway_id=? AND port_id=? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare gateway_port exists failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, gatewayId);
    bindText(stmt, 2, portId);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_ROW;
}

bool PcDatabase::createCommandLog(const std::string& commandId,
                                  std::int64_t seq,
                                  const std::string& commandType,
                                  const std::string& gatewayId,
                                  const std::string& portId,
                                  int deviceId,
                                  std::int64_t createTimeMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || commandId.empty() || seq <= 0 || commandType.empty()) {
        return false;
    }

    static const char* sql =
        "INSERT OR REPLACE INTO command_log ("
        "command_id,seq,command_type,gateway_id,port_id,device_id,status,"
        "reason,message,create_time_ms,send_time_ms,finish_time_ms"
        ") VALUES (?,?,?,?,?,?,?,'','',?,?,0);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare create command_log failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, commandId);
    sqlite3_bind_int64(stmt, 2, seq);
    bindText(stmt, 3, commandType);
    bindText(stmt, 4, gatewayId);
    bindText(stmt, 5, portId);
    sqlite3_bind_int(stmt, 6, deviceId);
    bindText(stmt, 7, "pending");
    sqlite3_bind_int64(stmt, 8, createTimeMs);
    sqlite3_bind_int64(stmt, 9, createTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool PcDatabase::updateCommandLogBySeq(std::int64_t seq,
                                       const std::string& status,
                                       const std::string& reason,
                                       const std::string& message,
                                       std::int64_t finishTimeMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || seq <= 0 || status.empty()) {
        return false;
    }

    static const char* sql =
        "UPDATE command_log "
        "SET status=?, reason=?, message=?, finish_time_ms=? "
        "WHERE seq=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare update command_log failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, status);
    bindText(stmt, 2, reason);
    bindText(stmt, 3, message);
    sqlite3_bind_int64(stmt, 4, finishTimeMs);
    sqlite3_bind_int64(stmt, 5, seq);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool PcDatabase::updateCommandLogByCommandId(const std::string& commandId,
                                             const std::string& status,
                                             const std::string& reason,
                                             const std::string& message,
                                             std::int64_t finishTimeMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || commandId.empty() || status.empty()) {
        return false;
    }

    static const char* sql =
        "UPDATE command_log "
        "SET status=?, reason=?, message=?, finish_time_ms=? "
        "WHERE command_id=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare update command_log by command_id failed: "
                  << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, status);
    bindText(stmt, 2, reason);
    bindText(stmt, 3, message);
    sqlite3_bind_int64(stmt, 4, finishTimeMs);
    bindText(stmt, 5, commandId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool PcDatabase::queryCommandTargetBySeq(std::int64_t seq, CommandLogTarget& target)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (!m_db || seq <= 0) {
        return false;
    }

    static const char* sql =
        "SELECT command_id,command_type,gateway_id,port_id,device_id "
        "FROM command_log WHERE seq=? LIMIT 1;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare query command_log target failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_int64(stmt, 1, seq);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* commandId = sqlite3_column_text(stmt, 0);
        const unsigned char* commandType = sqlite3_column_text(stmt, 1);
        const unsigned char* gatewayId = sqlite3_column_text(stmt, 2);
        const unsigned char* portId = sqlite3_column_text(stmt, 3);
        target.commandId = commandId ? reinterpret_cast<const char*>(commandId) : "";
        target.seq = seq;
        target.commandType = commandType ? reinterpret_cast<const char*>(commandType) : "";
        target.gatewayId = gatewayId ? reinterpret_cast<const char*>(gatewayId) : "";
        target.portId = portId ? reinterpret_cast<const char*>(portId) : "";
        target.deviceId = sqlite3_column_int(stmt, 4);
        found = true;
    }

    sqlite3_finalize(stmt);
    return found;
}

std::vector<CommandLogTarget> PcDatabase::collectCommandTimeouts(std::int64_t nowMs, std::int64_t timeoutMs)
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    std::vector<CommandLogTarget> results;
    if (!m_db) {
        return results;
    }

    static const char* sql =
        "SELECT command_id,seq,command_type,gateway_id,port_id,device_id "
        "FROM command_log WHERE status='sent' AND send_time_ms>0 AND ?-send_time_ms>=?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare collect command timeouts failed: " << sqlite3_errmsg(m_db) << std::endl;
        return results;
    }
    sqlite3_bind_int64(stmt, 1, nowMs);
    sqlite3_bind_int64(stmt, 2, timeoutMs);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CommandLogTarget target;
        const unsigned char* commandId = sqlite3_column_text(stmt, 0);
        target.commandId = commandId ? reinterpret_cast<const char*>(commandId) : "";
        target.seq = sqlite3_column_int64(stmt, 1);
        target.deviceId = sqlite3_column_int(stmt, 5);
        const unsigned char* commandType = sqlite3_column_text(stmt, 2);
        const unsigned char* gatewayId = sqlite3_column_text(stmt, 3);
        const unsigned char* portId = sqlite3_column_text(stmt, 4);
        target.commandType = commandType ? reinterpret_cast<const char*>(commandType) : "";
        target.gatewayId = gatewayId ? reinterpret_cast<const char*>(gatewayId) : "";
        target.portId = portId ? reinterpret_cast<const char*>(portId) : "";
        results.push_back(target);
    }
    sqlite3_finalize(stmt);

    for (const CommandLogTarget& target : results) {
        updateCommandLogByCommandId(target.commandId,
                                    "timeout",
                                    "linux_data_ack_timeout",
                                    "device execution timeout",
                                    nowMs);
    }
    return results;
}

void PcDatabase::close()
{
    std::lock_guard<std::recursive_mutex> lock(m_mutex);
    if (m_db) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

static const char* valueTypeToText(PointValueType type)
{
    switch (type) {
    case PointValueType::Number:
        return "number";
    case PointValueType::Text:
        return "text";
    case PointValueType::Boolean:
        return "boolean";
    default:
        return "unknown";
    }
}

static void bindText(sqlite3_stmt* stmt, int index, const std::string& value)
{
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

static std::string columnText(const unsigned char* value)
{
    return value ? reinterpret_cast<const char*>(value) : std::string();
}

static std::string telemetryDeviceKey(const TelemetryPoint& point)
{
    return point.gatewayId + "/" + point.portId + "/" + std::to_string(point.deviceId);
}

static void bindCommonPointColumns(sqlite3_stmt* stmt, int offset, const TelemetryPoint& point)
{
    sqlite3_bind_int64(stmt, offset + 1, point.timestampMs);
    bindText(stmt, offset + 2, point.factoryId);
    bindText(stmt, offset + 3, point.factoryName);
    bindText(stmt, offset + 4, point.areaId);
    bindText(stmt, offset + 5, point.areaName);
    bindText(stmt, offset + 6, point.gatewayId);
    bindText(stmt, offset + 7, point.gatewayName);
    bindText(stmt, offset + 8, point.portId);
    bindText(stmt, offset + 9, point.portName);
    sqlite3_bind_int(stmt, offset + 10, point.deviceId);
    bindText(stmt, offset + 11, point.deviceName);
    bindText(stmt, offset + 12, point.deviceType);
    bindText(stmt, offset + 13, point.pointKey);
    bindText(stmt, offset + 14, point.pointName);
    bindText(stmt, offset + 15, point.unit);
    bindText(stmt, offset + 16, valueTypeToText(point.valueType));
    sqlite3_bind_double(stmt, offset + 17, point.numberValue);
    bindText(stmt, offset + 18, point.textValue);
    sqlite3_bind_int(stmt, offset + 19, point.valid ? 1 : 0);
    bindText(stmt, offset + 20, point.errorMessage);
}

bool PcDatabase::saveLatestPoint(const TelemetryPoint& point)
{
    static const char* sql =
        "INSERT INTO latest_point ("
        "point_id,timestamp_ms,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,number_value,"
        "text_value,valid,error_message"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(point_id) DO UPDATE SET "
        "timestamp_ms=excluded.timestamp_ms,factory_id=excluded.factory_id,"
        "factory_name=excluded.factory_name,area_id=excluded.area_id,"
        "area_name=excluded.area_name,gateway_id=excluded.gateway_id,"
        "gateway_name=excluded.gateway_name,port_id=excluded.port_id,"
        "port_name=excluded.port_name,device_id=excluded.device_id,"
        "device_name=excluded.device_name,device_type=excluded.device_type,"
        "point_key=excluded.point_key,point_name=excluded.point_name,"
        "unit=excluded.unit,value_type=excluded.value_type,"
        "number_value=excluded.number_value,text_value=excluded.text_value,"
        "valid=excluded.valid,error_message=excluded.error_message;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare latest_point failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, point.pointId);
    sqlite3_bind_int64(stmt, 2, point.timestampMs);
    bindText(stmt, 3, point.factoryId);
    bindText(stmt, 4, point.factoryName);
    bindText(stmt, 5, point.areaId);
    bindText(stmt, 6, point.areaName);
    bindText(stmt, 7, point.gatewayId);
    bindText(stmt, 8, point.gatewayName);
    bindText(stmt, 9, point.portId);
    bindText(stmt, 10, point.portName);
    sqlite3_bind_int(stmt, 11, point.deviceId);
    bindText(stmt, 12, point.deviceName);
    bindText(stmt, 13, point.deviceType);
    bindText(stmt, 14, point.pointKey);
    bindText(stmt, 15, point.pointName);
    bindText(stmt, 16, point.unit);
    bindText(stmt, 17, valueTypeToText(point.valueType));
    sqlite3_bind_double(stmt, 18, point.numberValue);
    bindText(stmt, 19, point.textValue);
    sqlite3_bind_int(stmt, 20, point.valid ? 1 : 0);
    bindText(stmt, 21, point.errorMessage);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Insert latest_point failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::saveHistoryPoint(const TelemetryPoint& point)
{
    static const char* sql =
        "INSERT INTO telemetry_history ("
        "point_id,timestamp_ms,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,number_value,"
        "text_value,valid,error_message"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare telemetry_history failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, point.pointId);
    bindCommonPointColumns(stmt, 1, point);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Insert telemetry_history failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::savePointConfig(const PointConfig& config)
{
    static const char* sql =
        "REPLACE INTO point_config ("
        "point_id,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,enable_alarm,"
        "alarm_low,alarm_high,enabled,create_time_ms,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare point_config failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, config.pointId);
    bindText(stmt, 2, config.factoryId);
    bindText(stmt, 3, config.factoryName);
    bindText(stmt, 4, config.areaId);
    bindText(stmt, 5, config.areaName);
    bindText(stmt, 6, config.gatewayId);
    bindText(stmt, 7, config.gatewayName);
    bindText(stmt, 8, config.portId);
    bindText(stmt, 9, config.portName);
    sqlite3_bind_int(stmt, 10, config.deviceId);
    bindText(stmt, 11, config.deviceName);
    bindText(stmt, 12, config.deviceType);
    bindText(stmt, 13, config.pointKey);
    bindText(stmt, 14, config.pointName);
    bindText(stmt, 15, config.unit);
    bindText(stmt, 16, config.valueType);
    sqlite3_bind_int(stmt, 17, config.enableAlarm ? 1 : 0);
    if (config.hasAlarmLow) {
        sqlite3_bind_double(stmt, 18, config.alarmLow);
    } else {
        sqlite3_bind_null(stmt, 18);
    }
    if (config.hasAlarmHigh) {
        sqlite3_bind_double(stmt, 19, config.alarmHigh);
    } else {
        sqlite3_bind_null(stmt, 19);
    }
    sqlite3_bind_int(stmt, 20, config.enabled ? 1 : 0);
    sqlite3_bind_int64(stmt, 21, config.timestampMs);
    sqlite3_bind_int64(stmt, 22, config.timestampMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Insert point_config failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::upsertDeviceStatus(const DeviceRecord& device)
{
    if (!m_db || device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0) {
        return false;
    }

    static const char* sql =
        "INSERT INTO device_status ("
        "gateway_id,port_id,device_id,status,last_seen_ms,last_offline_ms,"
        "status_reason,update_time_ms"
        ") VALUES (?,?,?,?,?,?,?,?) "
        "ON CONFLICT(gateway_id, port_id, device_id) DO UPDATE SET "
        "status=excluded.status,"
        "last_seen_ms=CASE "
        "  WHEN excluded.last_seen_ms > device_status.last_seen_ms THEN excluded.last_seen_ms "
        "  ELSE device_status.last_seen_ms END,"
        "last_offline_ms=excluded.last_offline_ms,"
        "status_reason=excluded.status_reason,"
        "update_time_ms=excluded.update_time_ms;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare upsert device_status failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    bindText(stmt, 1, device.gatewayId);
    bindText(stmt, 2, device.portId);
    sqlite3_bind_int(stmt, 3, device.deviceId);
    bindText(stmt, 4, device.status.empty() ? std::string("unknown") : device.status);
    sqlite3_bind_int64(stmt, 5, device.lastSeenMs);
    sqlite3_bind_int64(stmt, 6, device.lastOfflineMs);
    bindText(stmt, 7, device.statusReason);
    sqlite3_bind_int64(stmt, 8, device.updateTimeMs);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Upsert device_status failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::execSql(const std::string& sql)
{
    if (!m_db) {
        std::cerr << "Database is not open." << std::endl;
        return false;
    }

    char* errorMessage = nullptr;

    int rc = sqlite3_exec(
        m_db,
        sql.c_str(),
        nullptr,
        nullptr,
        &errorMessage
        );

    if (rc != SQLITE_OK) {
        std::cerr << "SQL exec failed: ";

        if (errorMessage) {
            std::cerr << errorMessage;
            sqlite3_free(errorMessage);
        } else {
            std::cerr << sqlite3_errmsg(m_db);
        }

        std::cerr << std::endl;
        std::cerr << "SQL: " << sql << std::endl;

        return false;
    }

    return true;
}

bool PcDatabase::deleteRowsByDevice(const std::string& table,
                                    const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId)
{
    const std::string sql =
        "DELETE FROM " + table +
        " WHERE gateway_id = ? AND port_id = ? AND device_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare delete device failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, deviceId);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Delete device rows failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::deleteRowsByMaster(const std::string& table,
                                    const std::string& gatewayId,
                                    const std::string& portId)
{
    const std::string sql =
        "DELETE FROM " + table +
        " WHERE gateway_id = ? AND port_id = ?;";

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Prepare delete master failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    sqlite3_bind_text(stmt, 1, gatewayId.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, portId.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        std::cerr << "Delete master rows failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }

    return true;
}

bool PcDatabase::ensureDirectoryForFile(const std::string& dbPath)
{
    try {
        std::filesystem::path path(dbPath);
        std::filesystem::path dir = path.parent_path();

        if (dir.empty()) {
            return true;
        }

        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Create database directory failed: "
                  << e.what()
                  << std::endl;
        return false;
    }
}
