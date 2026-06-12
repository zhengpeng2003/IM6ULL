#ifndef PC_DATA_SERVICE_HPP
#define PC_DATA_SERVICE_HPP

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "model/TelemetryPack.hpp"
#include "model/TelemetryPoint.hpp"

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

/*
 * PcDataService
 * -------------
 * Pc_data 的核心数据服务层。
 *
 * 当前阶段先不接 SQLite，也不接 MQTT。
 *
 * 当前负责：
 * 1. 接收 TelemetryPack
 * 2. 调用 ModelConverter 转成 TelemetryPoint
 * 3. 维护最新快照 m_snapshot
 * 4. 给 IPC 层提供查询接口
 *
 * 后续扩展：
 * - MqttClient 收到 MQTT 数据后调用 handleTelemetryPack()
 * - IpcServer 收到 Pc_ui 请求后调用 getLatestPoints()
 * - SqliteStorage 后续接入历史数据和快照数据
 */
class PcDataService
{
public:
    PcDataService();

    // 接收一包遥测数据，转换成测点，并更新内存快照
    void handleTelemetryPack(const TelemetryPack& pack);

    // 获取当前所有最新测点
    std::vector<TelemetryPoint> getLatestPoints() const;

    // 根据 pointId 获取单个测点
    bool getPointById(const std::string& pointId, TelemetryPoint& outPoint) const;

    bool removeDeviceData(const std::string& gatewayId,
                          const std::string& portId,
                          int deviceId);
    bool removeMasterData(const std::string& gatewayId,
                          const std::string& portId);

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

    // 清空当前快照
    void clear();

    // 生成模拟数据，用于当前阶段测试 model + service + IPC
    //void generateMockData();
    //void generateMockDataExtraCases();
private:
    mutable std::mutex m_mutex;

    /*
     * 最新快照缓存。
     *
     * key:
     *   pointId
     *
     * value:
     *   当前测点最新值
     *
     * 举例：
     * m_snapshot["factory_001.area_001.gateway_001.port_001.1.temperature"]
     *     = 从站1温度最新值
     *
     * m_snapshot["factory_001.area_001.gateway_001.port_001.1.humidity"]
     *     = 从站1湿度最新值
     */
    std::unordered_map<std::string, TelemetryPoint> m_snapshot;
    std::unordered_map<std::int64_t, SyncGatewayPending> m_syncPendingBySeq;
    std::unordered_map<int, std::vector<SyncGatewayPending> > m_syncRequests;
    std::unordered_map<int, SyncConfigResult> m_syncRequestResults;
    int m_nextSyncRequestId = 1;
    std::int64_t m_nextSyncSeq = 1;
};

#endif // PC_DATA_SERVICE_HPP
