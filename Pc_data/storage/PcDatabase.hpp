#ifndef PC_DATABASE_HPP
#define PC_DATABASE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "model/PointConfig.hpp"
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
    std::vector<TelemetryPoint> queryHistoryPoints(const std::string& pointId,
                                                   std::int64_t startMs,
                                                   std::int64_t endMs,
                                                   int limit);

    bool isOpen() const;
    void close();

private:
    bool execSql(const std::string& sql);
    bool ensureDirectoryForFile(const std::string& dbPath);
    bool saveLatestPoint(const TelemetryPoint& point);
    bool saveHistoryPoint(const TelemetryPoint& point);
    bool savePointConfig(const PointConfig& config);

private:
    sqlite3* m_db = nullptr;
};

#endif // PC_DATABASE_HPP
