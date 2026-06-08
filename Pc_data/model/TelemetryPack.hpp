#ifndef TELEMETRY_PACK_HPP
#define TELEMETRY_PACK_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "SiteContext.hpp"
#include "DeviceData.hpp"

/*
 * TelemetryPack
 * -------------
 * 一包遥测数据。
 *
 * 推荐定义：
 * 一个网关 + 一个端口 + 一次采集周期内的多个设备数据。
 *
 * 例如：
 * 1号厂房 / 配电房 / i.MX6ULL_001 / RS485-1
 *     ├── 从站1 温湿度
 *     ├── 从站2 电表
 *     └── 从站3 继电器
 *
 * 对应你原来 Linux_data/data_protocol.h 的思想：
 *
 * protocol_msg_t
 *   └── telemetry_msg_t
 *         └── device_data_t devices[MAX_DEVICES_PER_PACK]
 */
struct TelemetryPack
{
    std::uint32_t version = 1;
    std::uint32_t sequence = 0;

    // 建议 Pc_data 内部统一使用毫秒时间戳
    // 后续 SQLite / UI / 趋势图都方便。
    std::int64_t timestampMs = 0;

    // MQTT client_id / target_id 思想保留
    std::string sourceId;   // 例如: gateway_001
    std::string targetId;   // 例如: pc_data_001

    // 公共现场信息
    SiteContext site;

    // 一包里可以有多个设备
    std::vector<DeviceData> devices;
};

#endif // TELEMETRY_PACK_HPP