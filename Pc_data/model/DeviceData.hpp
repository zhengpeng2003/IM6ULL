#ifndef DEVICE_DATA_HPP
#define DEVICE_DATA_HPP

#include <cstdint>
#include <string>
#include <vector>

/*
 * DeviceType
 * ----------
 * Pc_data 内部设备类型。
 *
 * 这个枚举思想和 Linux_data/data_protocol.h 里的 device_type_t 保持一致。
 * 后续你修改 data_protocol.h 的时候，可以继续保持编号一致。
 */
enum class DeviceType : int
{
    Unknown       = 0,
    SensorTH      = 1,     // 温湿度
    Relay         = 2,     // 继电器
    ElectricMeter = 3,     // 电表，Pc_data 先预留
    SysInfo       = 100    // 系统信息
};

/*
 * TemperatureHumidityData
 * -----------------------
 * 温湿度设备数据。
 *
 * 对应原来的：
 * data.th.temperature
 * data.th.humidity
 */
struct TemperatureHumidityData
{
    float temperature = 0.0f;
    float humidity = 0.0f;
};

/*
 * ElectricMeterData
 * -----------------
 * 电表设备数据。
 *
 * 后续 Linux_data/data_protocol.h 可以增加对应结构体。
 */
struct ElectricMeterData
{
    float voltage = 0.0f;   // 电压 V
    float current = 0.0f;   // 电流 A
    float power = 0.0f;     // 功率 W
    float energy = 0.0f;    // 电能 kWh
};

/*
 * RelayData
 * ---------
 * 继电器设备数据。
 *
 * relayStates 用位表示：
 * bit0 = 继电器1
 * bit1 = 继电器2
 * bit2 = 继电器3
 *
 * 这和你原来的 uint16_t relay_states 思路一致。
 */
struct RelayData
{
    std::uint16_t relayStates = 0;
    int channelCount = 16;
};

/*
 * SysInfoData
 * -----------
 * 系统信息。
 *
 * 对应原来的：
 * kernel / arch / os / screen_w / screen_h
 *
 * Pc_data 内部可以比 Linux_data 结构稍微丰富一点，
 * 但是不要破坏原来的字段思想。
 */
struct SysInfoData
{
    std::string kernel;
    std::string arch;
    std::string os;

    int screenWidth = 0;
    int screenHeight = 0;

    double cpuUsage = 0.0;      // %
    double memoryUsage = 0.0;   // %
};

/*
 * DeviceData
 * ----------
 * 单个设备的原始数据。
 *
 * 注意：
 * 1. 这里保留“不同设备不同结构体”的思想。
 * 2. 不直接把这个结构存 SQLite。
 * 3. 后续要通过 ModelConverter 展开成 TelemetryPoint。
 *
 * 这里没有使用 union，原因是：
 * - C++ 里 std::string 不适合直接放传统 union。
 * - Pc_data 内部更适合用普通结构体字段 + type 判断。
 * - 逻辑上仍然保持和 data_protocol.h 一样：
 *   DeviceType 决定使用哪一组数据。
 */
struct DeviceData
{
    int deviceId = 0;              // 从站地址，例如 1、2、3
    std::string deviceName;        // 例如: 从站1 温湿度
    DeviceType type = DeviceType::Unknown;

    bool valid = false;            // 本轮是否读取成功
    std::string errorMessage;      // 如果 valid=false，可以记录 timeout / crc_error 等
    std::vector<std::string> pointKeys; // telemetry_pack points[] 中携带的原始 pointKey

    TemperatureHumidityData th;
    ElectricMeterData meter;
    RelayData relay;
    SysInfoData sys;
};

inline std::string deviceTypeToString(DeviceType type)
{
    switch (type) {
    case DeviceType::SensorTH:
        return "sensor_th";
    case DeviceType::Relay:
        return "relay";
    case DeviceType::ElectricMeter:
        return "electric_meter";
    case DeviceType::SysInfo:
        return "sysinfo";
    default:
        return "unknown";
    }
}

inline std::string deviceTypeToDisplayName(DeviceType type)
{
    switch (type) {
    case DeviceType::SensorTH:
        return "SensorTH";
    case DeviceType::Relay:
        return "Relay";
    case DeviceType::ElectricMeter:
        return "ElectricMeter";
    case DeviceType::SysInfo:
        return "SysInfo";
    default:
        return "Unknown";
    }
}

#endif // DEVICE_DATA_HPP
