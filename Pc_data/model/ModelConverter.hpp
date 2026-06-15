#ifndef MODEL_CONVERTER_HPP
#define MODEL_CONVERTER_HPP

#include <string>
#include <vector>

#include "TelemetryPack.hpp"
#include "TelemetryPoint.hpp"

/*
 * ModelConverter
 * --------------
 * 负责把 Pc_data 内部的一包遥测数据展开成统一测点。
 *
 * 输入：
 *   TelemetryPack
 *
 * 输出：
 *   std::vector<TelemetryPoint>
 *
 * 用途：
 *   1. SQLite 历史数据存储
 *   2. 快照表更新
 *   3. UI 实时数据显示
 *   4. 趋势图
 *   5. 告警判断
 */
class ModelConverter
{
public:
    static std::vector<TelemetryPoint> toTelemetryPoints(const TelemetryPack& pack);

    /*
     * 构造全局唯一测点 ID。
     * 格式：factoryId.areaId.gatewayId.portId.deviceId.pointKey
     */
    static std::string buildPointId(const std::string& factoryId,
                                    const std::string& areaId,
                                    const std::string& gatewayId,
                                    const std::string& portId,
                                    int deviceId,
                                    const std::string& pointKey);

private:
    static TelemetryPoint makeBasePoint(const TelemetryPack& pack,
                                        const DeviceData& device);

    static std::string buildPointId(const TelemetryPack& pack,
                                    const DeviceData& device,
                                    const std::string& pointKey);

    static void appendTemperatureHumidityPoints(const TelemetryPack& pack,
                                                const DeviceData& device,
                                                std::vector<TelemetryPoint>& points);

    static void appendElectricMeterPoints(const TelemetryPack& pack,
                                          const DeviceData& device,
                                          std::vector<TelemetryPoint>& points);

    static void appendRelayPoints(const TelemetryPack& pack,
                                  const DeviceData& device,
                                  std::vector<TelemetryPoint>& points);

    static void appendSysInfoPoints(const TelemetryPack& pack,
                                    const DeviceData& device,
                                    std::vector<TelemetryPoint>& points);

    static void appendUnknownDevicePoint(const TelemetryPack& pack,
                                         const DeviceData& device,
                                         std::vector<TelemetryPoint>& points);
};

#endif // MODEL_CONVERTER_HPP
