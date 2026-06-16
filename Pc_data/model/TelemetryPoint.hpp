#ifndef TELEMETRY_POINT_HPP
#define TELEMETRY_POINT_HPP

#include <cstdint>
#include <string>

/*
 * PointValueType
 * --------------
 * 测点值类型。
 *
 * 大部分传感器是 Number。
 * 系统信息可能是 Text。
 * 继电器状态也可以转成 Boolean：0 / 1。
 */
enum class PointValueType
{
    Number,
    Text,
    Boolean
};

/*
 * TelemetryPoint
 * --------------
 * 展开后的统一测点。
 *
 * 这是后续 SQLite、UI、趋势图、告警系统真正使用的数据结构。
 *
 * 例子：
 * 1号厂房 / 配电房 / i.MX6ULL_001 / RS485-1 / 从站1 / temperature = 26.5 ℃
 *
 * 一个 DeviceData 可以展开成多个 TelemetryPoint：
 *
 * 温湿度设备：
 *   temperature -> TelemetryPoint
 *   humidity    -> TelemetryPoint
 *
 * 电表：
 *   voltage -> TelemetryPoint
 *   current -> TelemetryPoint
 *   power   -> TelemetryPoint
 *   energy  -> TelemetryPoint
 *
 * 继电器：
 *   relay_1 -> TelemetryPoint
 *   relay_2 -> TelemetryPoint
 */
struct TelemetryPoint
{
    std::int64_t timestampMs = 0;
    std::int64_t receiveTimeMs = 0;

    // 现场上下文
    std::string factoryId;
    std::string factoryName;

    std::string areaId;
    std::string areaName;

    std::string gatewayId;
    std::string gatewayName;

    std::string portId;
    std::string portName;

    // 设备信息
    int deviceId = 0;
    std::string deviceName;
    std::string deviceType;

    // 测点信息
    //
    // pointId 是全局唯一测点编号。
    // 例如：
    // factory_001.area_001.gateway_001.port_001.1.temperature
    //
    // 它由：
    // factoryId + areaId + gatewayId + portId + deviceId + pointKey
    // 组合生成。
    //
    // 用途：
    // 1. PcDataService 快照缓存 key
    // 2. SQLite 快照表主键
    // 3. SQLite 历史表查询条件
    // 4. UI 趋势图查询某一个测点
    std::string pointId;

    std::string pointKey;       // temperature / humidity / voltage / relay_1
    std::string pointName;      // 温度 / 湿度 / 电压 / 继电器1
    std::string unit;           // ℃ / % / V / A / W / kWh

    PointValueType valueType = PointValueType::Number;

    // 数值型
    double numberValue = 0.0;

    // 文本型，比如 kernel、arch、os
    std::string textValue;

    // 数据是否有效
    bool valid = false;
    std::string errorMessage;
};

#endif // TELEMETRY_POINT_HPP
