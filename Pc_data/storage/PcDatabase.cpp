#include "PcDatabase.hpp"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>

#include "DatabaseSchema.hpp"
#include "sqlite3.h"

PcDatabase::PcDatabase()
{
}

PcDatabase::~PcDatabase()
{
    close();
}

bool PcDatabase::openDatabase(const std::string& dbPath)
{
    if (m_db) {
        return true;
    }

    if (!ensureDirectoryForFile(dbPath)) {
        return false;
    }

    int rc = sqlite3_open(dbPath.c_str(), &m_db);

    if (rc != SQLITE_OK) {
        std::cerr << "Open database failed: "
                  << sqlite3_errmsg(m_db)
                  << std::endl;

        close();
        return false;
    }

    std::cout << "Database opened: " << dbPath << std::endl;

    execSql("PRAGMA foreign_keys = ON;");
    execSql("PRAGMA journal_mode = WAL;");

    return true;
}

bool PcDatabase::initTables()
{
    if (!m_db) {
        std::cerr << "Database is not open." << std::endl;
        return false;
    }

    for (const auto& sql : DatabaseSchema::tableSqlList()) {
        if (!execSql(sql)) {
            return false;
        }
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
    return m_db != nullptr;
}

bool PcDatabase::saveTelemetryPoints(const std::vector<TelemetryPoint>& points)
{
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
    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }

        if (!saveLatestPoint(point) || !saveHistoryPoint(point)) {
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

std::vector<TelemetryPoint> PcDatabase::queryLatestPoints()
{
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

std::vector<TelemetryPoint> PcDatabase::queryHistoryPoints(const std::string& pointId,
                                                           std::int64_t startMs,
                                                           std::int64_t endMs,
                                                           int limit)
{
    std::vector<TelemetryPoint> points;

    if (!m_db || pointId.empty()) {
        return points;
    }

    if (startMs < 0) {
        startMs = 0;
    }

    if (endMs <= 0) {
        endMs = std::numeric_limits<std::int64_t>::max();
    }

    if (endMs < startMs) {
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
    }

    sqlite3_finalize(stmt);
    return points;
}

bool PcDatabase::deleteDeviceData(const std::string& gatewayId,
                                  const std::string& portId,
                                  int deviceId)
{
    if (!m_db || gatewayId.empty() || portId.empty() || deviceId <= 0) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = deleteRowsByDevice("latest_point", gatewayId, portId, deviceId) &&
              deleteRowsByDevice("point_config", gatewayId, portId, deviceId) &&
              deleteRowsByDevice("telemetry_history", gatewayId, portId, deviceId) &&
              deleteRowsByDevice("alarm_event", gatewayId, portId, deviceId);

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
    if (!m_db || gatewayId.empty() || portId.empty()) {
        return false;
    }

    if (!execSql("BEGIN TRANSACTION;")) {
        return false;
    }

    bool ok = deleteRowsByMaster("latest_point", gatewayId, portId) &&
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

bool PcDatabase::clearRecoveredAlarms()
{
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

void PcDatabase::close()
{
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
        "REPLACE INTO latest_point ("
        "point_id,timestamp_ms,factory_id,factory_name,area_id,area_name,"
        "gateway_id,gateway_name,port_id,port_name,device_id,device_name,"
        "device_type,point_key,point_name,unit,value_type,number_value,"
        "text_value,valid,error_message"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

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
