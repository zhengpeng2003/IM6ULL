#ifndef SITE_CONTEXT_HPP
#define SITE_CONTEXT_HPP

#include <string>

/*
 * SiteContext
 * ------------
 * 表示一包数据的公共现场上下文。
 *
 * 对应现场结构：
 * 1号厂房 / 配电房 / i.MX6ULL_001 / RS485-1
 *
 * 这些字段不放到每个设备里，避免重复。
 */
struct SiteContext
{
    // 厂房
    std::string factoryId;      // 例如: factory_001
    std::string factoryName;    // 例如: 1号厂房

    // 区域
    std::string areaId;         // 例如: area_001
    std::string areaName;       // 例如: 配电房

    // 网关
    std::string gatewayId;      // 例如: gateway_001
    std::string gatewayName;    // 例如: i.MX6ULL_001

    // 端口
    std::string portId;         // 例如: port_001
    std::string portName;       // 例如: RS485-1
};

#endif // SITE_CONTEXT_HPP