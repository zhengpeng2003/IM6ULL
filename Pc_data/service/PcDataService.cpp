#include "PcDataService.hpp"

#include <chrono>
#include <iostream>

#include "model/ModelConverter.hpp"

PcDataService::PcDataService()
{
}

void PcDataService::handleTelemetryPack(const TelemetryPack& pack)
{
    std::vector<TelemetryPoint> points = ModelConverter::toTelemetryPoints(pack);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }

        /*
         * Same pointId means the same telemetry point.
         * When new data arrives, overwrite the old value.
         * This is the latest snapshot.
         */
        m_snapshot[point.pointId] = point;
    }
}

std::vector<TelemetryPoint> PcDataService::getLatestPoints() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TelemetryPoint> result;
    result.reserve(m_snapshot.size());

    for (const auto& item : m_snapshot) {
        result.push_back(item.second);
    }

    return result;
}

bool PcDataService::getPointById(const std::string& pointId, TelemetryPoint& outPoint) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_snapshot.find(pointId);
    if (it == m_snapshot.end()) {
        return false;
    }

    outPoint = it->second;
    return true;
}

void PcDataService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
}

static std::int64_t currentTimeMs()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
               ).count();
}

void PcDataService::generateMockData()
{
    TelemetryPack pack;

    pack.timestampMs = currentTimeMs();

    pack.site.factoryId = "factory_001";
    pack.site.factoryName = "Factory 001";

    pack.site.areaId = "area_001";
    pack.site.areaName = "Power Distribution Room";

    pack.site.gatewayId = "gateway_001";
    pack.site.gatewayName = "i.MX6ULL_001";

    pack.site.portId = "port_001";
    pack.site.portName = "RS485-1";

    /*
     * 从站1：温湿度设备
     * 生成 2 个点：
     * temperature / humidity
     */
    {
        DeviceData dev;

        dev.deviceId = 1;
        dev.deviceName = "Slave 1 Temperature Humidity Sensor";
        dev.type = DeviceType::SensorTH;
        dev.valid = true;
        dev.errorMessage = "";

        dev.th.temperature = 26.5f;
        dev.th.humidity = 60.2f;

        pack.devices.push_back(dev);
    }

    /*
     * 从站2：温湿度设备
     * 生成 2 个点：
     * temperature / humidity
     */
    {
        DeviceData dev;

        dev.deviceId = 2;
        dev.deviceName = "Slave 2 Temperature Humidity Sensor";
        dev.type = DeviceType::SensorTH;
        dev.valid = true;
        dev.errorMessage = "";

        dev.th.temperature = 27.1f;
        dev.th.humidity = 58.6f;

        pack.devices.push_back(dev);
    }

    /*
     * 从站3：电表设备
     * 生成 4 个点：
     * voltage / current / power / energy
     */
    {
        DeviceData dev;

        dev.deviceId = 3;
        dev.deviceName = "Slave 3 Electric Meter";
        dev.type = DeviceType::ElectricMeter;
        dev.valid = true;
        dev.errorMessage = "";

        dev.meter.voltage = 220.5f;
        dev.meter.current = 3.2f;
        dev.meter.power = 705.6f;
        dev.meter.energy = 128.9f;

        pack.devices.push_back(dev);
    }

    /*
     * 从站4：继电器设备
     * 生成 4 个点：
     * relay_1 / relay_2 / relay_3 / relay_4
     */
    {
        DeviceData dev;

        dev.deviceId = 4;
        dev.deviceName = "Slave 4 Relay";
        dev.type = DeviceType::Relay;
        dev.valid = true;
        dev.errorMessage = "";

        dev.relay.channelCount = 4;

        /*
         * 二进制 0101：
         * relay_1 = on
         * relay_2 = off
         * relay_3 = on
         * relay_4 = off
         */
        dev.relay.relayStates = 0b0101;

        pack.devices.push_back(dev);
    }

    /*
     * 系统信息
     * 生成 7 个点：
     * kernel / arch / os / screen_width / screen_height / cpu_usage / memory_usage
     */
    {
        DeviceData dev;

        dev.deviceId = 100;
        dev.deviceName = "i.MX6ULL System Info";
        dev.type = DeviceType::SysInfo;
        dev.valid = true;
        dev.errorMessage = "";

        dev.sys.kernel = "4.19.35";
        dev.sys.arch = "armv7l";
        dev.sys.os = "Debian 10";
        dev.sys.screenWidth = 480;
        dev.sys.screenHeight = 272;
        dev.sys.cpuUsage = 35.6;
        dev.sys.memoryUsage = 62.8;

        pack.devices.push_back(dev);
    }

    clear();
    handleTelemetryPack(pack);

    std::cout << "PcDataService mock point count: "
              << getLatestPoints().size()
              << std::endl;
}