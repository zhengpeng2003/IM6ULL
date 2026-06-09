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
    /*
 * ============================================================
 * 额外模拟一包数据：
 * 2号厂房 / 水泵房 / i.MX6ULL_002 / RS485-1
 *
 * 用来测试：
 * 1. 多厂房
 * 2. 多网关
 * 3. 多主站
 * 4. 高温异常
 * 5. 电流过高
 * 6. 设备通信失败 valid=false
 * 7. 系统 CPU / 内存过高
 * ============================================================
 */
    {
        TelemetryPack pack2;

        pack2.timestampMs = currentTimeMs();

        pack2.site.factoryId = "factory_002";
        pack2.site.factoryName = "Factory 002";

        pack2.site.areaId = "area_001";
        pack2.site.areaName = "Pump Room";

        pack2.site.gatewayId = "gateway_002";
        pack2.site.gatewayName = "i.MX6ULL_002";

        pack2.site.portId = "port_001";
        pack2.site.portName = "RS485-1";

        /*
     * 从站1：水泵房温湿度
     * 场景：温度过高，但是通信正常
     * valid = true
     * 后续 AlarmManager 可以根据 temperature > 60 判断高温告警
     */
        {
            DeviceData dev;

            dev.deviceId = 1;
            dev.deviceName = "Pump Room High Temperature Sensor";
            dev.type = DeviceType::SensorTH;
            dev.valid = true;
            dev.errorMessage = "";

            dev.th.temperature = 78.6f;
            dev.th.humidity = 68.4f;

            pack2.devices.push_back(dev);
        }

        /*
     * 从站2：水泵电表
     * 场景：电流过高、功率过高，但是通信正常
     * valid = true
     * 后续可以根据 current / power 判断负载异常
     */
        {
            DeviceData dev;

            dev.deviceId = 2;
            dev.deviceName = "Pump Electric Meter";
            dev.type = DeviceType::ElectricMeter;
            dev.valid = true;
            dev.errorMessage = "";

            dev.meter.voltage = 221.3f;
            dev.meter.current = 26.8f;
            dev.meter.power = 5920.5f;
            dev.meter.energy = 891.7f;

            pack2.devices.push_back(dev);
        }

        /*
     * 从站3：备用电表
     * 场景：Modbus CRC 错误，数据无效
     * valid = false
     * UI 应该显示异常状态，errorMessage 显示 modbus_crc_error
     */
        {
            DeviceData dev;

            dev.deviceId = 3;
            dev.deviceName = "Backup Electric Meter";
            dev.type = DeviceType::ElectricMeter;
            dev.valid = false;
            dev.errorMessage = "modbus_crc_error";

            dev.meter.voltage = 0.0f;
            dev.meter.current = 0.0f;
            dev.meter.power = 0.0f;
            dev.meter.energy = 0.0f;

            pack2.devices.push_back(dev);
        }

        /*
     * 从站4：水泵继电器
     * 场景：继电器通信超时
     * valid = false
     * 会生成 relay_1 ~ relay_4 四个异常点
     */
        {
            DeviceData dev;

            dev.deviceId = 4;
            dev.deviceName = "Pump Relay Controller";
            dev.type = DeviceType::Relay;
            dev.valid = false;
            dev.errorMessage = "modbus_timeout";

            dev.relay.channelCount = 4;
            dev.relay.relayStates = 0b0000;

            pack2.devices.push_back(dev);
        }

        /*
     * 从站5：另一个正常温湿度设备
     * 场景：普通正常设备，用来和异常设备做对比
     */
        {
            DeviceData dev;

            dev.deviceId = 5;
            dev.deviceName = "Pump Room Normal TH Sensor";
            dev.type = DeviceType::SensorTH;
            dev.valid = true;
            dev.errorMessage = "";

            dev.th.temperature = 31.2f;
            dev.th.humidity = 55.9f;

            pack2.devices.push_back(dev);
        }

        /*
     * 系统信息
     * 场景：网关在线，但是 CPU / 内存偏高
     */
        {
            DeviceData dev;

            dev.deviceId = 100;
            dev.deviceName = "i.MX6ULL_002 System Info";
            dev.type = DeviceType::SysInfo;
            dev.valid = true;
            dev.errorMessage = "";

            dev.sys.kernel = "4.19.35";
            dev.sys.arch = "armv7l";
            dev.sys.os = "Debian 10";
            dev.sys.screenWidth = 480;
            dev.sys.screenHeight = 272;
            dev.sys.cpuUsage = 93.5;
            dev.sys.memoryUsage = 89.6;

            pack2.devices.push_back(dev);
        }

        handleTelemetryPack(pack2);
    }
}
void PcDataService::generateMockDataExtraCases()
{
    /*
     * 这个 tick 是 static 的：
     * 每调用一次 generateMockDataExtraCases()，tick 就会 +1。
     *
     * 所以你只要外面每秒调用一次这个函数，
     * 数据就会每秒变化一次。
     */
    static int tick = 0;
    ++tick;

    const int64_t now = currentTimeMs();

    /*
     * 每 3 次调用切换一次错误状态。
     * 如果你外面是 1 秒调用一次，
     * 那就是每 3 秒切换一次状态。
     *
     * errorMode:
     * 0: 正常
     * 1: modbus_timeout
     * 2: modbus_crc_error
     * 3: invalid_data
     * 4: device_offline
     */
    const int errorMode = (tick / 3) % 5;

    /*
     * wave 范围：-20 ~ 19
     * 用来让温度、湿度、电压、电流明显变化。
     */
    const float wave = static_cast<float>((tick % 40) - 20);

    /*
     * ============================================================
     * Pack 1:
     * 1号厂房 / 配电房 / i.MX6ULL_001 / RS485-1
     *
     * 正常数据，每秒大幅变化。
     * ============================================================
     */
    {
        TelemetryPack pack;

        pack.timestampMs = now;

        pack.site.factoryId = "factory_001";
        pack.site.factoryName = "Factory 001";

        pack.site.areaId = "area_001";
        pack.site.areaName = "Power Distribution Room";

        pack.site.gatewayId = "gateway_001";
        pack.site.gatewayName = "i.MX6ULL_001";

        pack.site.portId = "port_001";
        pack.site.portName = "RS485-1";

        /*
         * 从站1：正常温湿度传感器
         * 变化非常明显，方便 UI 测试。
         */
        {
            DeviceData dev;

            dev.deviceId = 1;
            dev.deviceName = "Slave 1 Temperature Humidity Sensor";
            dev.type = DeviceType::SensorTH;
            dev.valid = true;
            dev.errorMessage = "";

            /*
             * 温度大概范围：2.5 ~ 49.3
             * 湿度大概范围：12 ~ 90
             */
            dev.th.temperature = 26.5f + wave * 1.2f;
            dev.th.humidity = 52.0f + wave * 2.0f;

            pack.devices.push_back(dev);
        }

        /*
         * 从站2：正常电表
         * 电压、电流、功率每秒明显变化。
         */
        {
            DeviceData dev;

            dev.deviceId = 2;
            dev.deviceName = "Slave 2 Electric Meter";
            dev.type = DeviceType::ElectricMeter;
            dev.valid = true;
            dev.errorMessage = "";

            /*
             * 电压大概范围：180 ~ 258
             * 电流大概范围：0 ~ 15.6
             */
            dev.meter.voltage = 220.0f + wave * 2.0f;
            dev.meter.current = 8.0f + wave * 0.4f;

            if (dev.meter.current < 0.0f) {
                dev.meter.current = 0.0f;
            }

            dev.meter.power = dev.meter.voltage * dev.meter.current;
            dev.meter.energy = 100.0f + tick * 0.5f;

            pack.devices.push_back(dev);
        }

        /*
         * 从站3：4通道继电器
         * 每秒切换状态，测试 UI 开关变化。
         */
        {
            DeviceData dev;

            dev.deviceId = 3;
            dev.deviceName = "Slave 3 Relay Controller";
            dev.type = DeviceType::Relay;
            dev.valid = true;
            dev.errorMessage = "";

            dev.relay.channelCount = 4;

            if (tick % 4 == 0) {
                dev.relay.relayStates = 0b0001;
            } else if (tick % 4 == 1) {
                dev.relay.relayStates = 0b0011;
            } else if (tick % 4 == 2) {
                dev.relay.relayStates = 0b0111;
            } else {
                dev.relay.relayStates = 0b1111;
            }

            pack.devices.push_back(dev);
        }

        /*
         * 系统信息
         * CPU / 内存大幅跳动，方便测试 UI。
         */
        {
            DeviceData dev;

            dev.deviceId = 100;
            dev.deviceName = "i.MX6ULL_001 System Info";
            dev.type = DeviceType::SysInfo;
            dev.valid = true;
            dev.errorMessage = "";

            dev.sys.kernel = "4.19.35";
            dev.sys.arch = "armv7l";
            dev.sys.os = "Debian 10";
            dev.sys.screenWidth = 480;
            dev.sys.screenHeight = 272;

            dev.sys.cpuUsage = 10.0 + (tick * 7) % 90;
            dev.sys.memoryUsage = 20.0 + (tick * 9) % 80;

            pack.devices.push_back(dev);
        }

        handleTelemetryPack(pack);
    }

    /*
     * ============================================================
     * Pack 2:
     * 2号厂房 / 水泵房 / i.MX6ULL_002 / RS485-1
     *
     * 专门测试异常状态变化。
     * ============================================================
     */
    {
        TelemetryPack pack;

        pack.timestampMs = now;

        pack.site.factoryId = "factory_002";
        pack.site.factoryName = "Factory 002";

        pack.site.areaId = "area_001";
        pack.site.areaName = "Pump Room";

        pack.site.gatewayId = "gateway_002";
        pack.site.gatewayName = "i.MX6ULL_002";

        pack.site.portId = "port_001";
        pack.site.portName = "RS485-1";

        /*
         * 从站1：水泵房温湿度
         *
         * errorMode == 1: modbus_timeout
         * errorMode == 3: invalid_data
         * errorMode == 4: device_offline
         * 其他状态：正常，但是数值偏高，用来测试告警颜色
         */
        {
            DeviceData dev;

            dev.deviceId = 1;
            dev.deviceName = "Pump Room Temperature Humidity Sensor";
            dev.type = DeviceType::SensorTH;

            if (errorMode == 1) {
                dev.valid = false;
                dev.errorMessage = "modbus_timeout";

                dev.th.temperature = 0.0f;
                dev.th.humidity = 0.0f;
            } else if (errorMode == 3) {
                dev.valid = false;
                dev.errorMessage = "invalid_data";

                dev.th.temperature = -999.0f;
                dev.th.humidity = 999.0f;
            } else if (errorMode == 4) {
                dev.valid = false;
                dev.errorMessage = "device_offline";

                dev.th.temperature = 0.0f;
                dev.th.humidity = 0.0f;
            } else {
                dev.valid = true;
                dev.errorMessage = "";

                /*
                 * 高温高湿，大幅变化。
                 * 温度大概范围：40 ~ 98.5
                 * 湿度大概范围：62 ~ 101
                 */
                dev.th.temperature = 70.0f + wave * 1.5f;
                dev.th.humidity = 82.0f + wave * 1.0f;

                if (dev.th.humidity > 100.0f) {
                    dev.th.humidity = 100.0f;
                }
            }

            pack.devices.push_back(dev);
        }

        /*
         * 从站2：水泵电表
         *
         * errorMode == 2: modbus_crc_error
         * errorMode == 4: device_offline
         * 其他状态：正常，但是电流和功率偏高
         */
        {
            DeviceData dev;

            dev.deviceId = 2;
            dev.deviceName = "Pump Electric Meter";
            dev.type = DeviceType::ElectricMeter;

            if (errorMode == 2) {
                dev.valid = false;
                dev.errorMessage = "modbus_crc_error";

                dev.meter.voltage = 0.0f;
                dev.meter.current = 0.0f;
                dev.meter.power = 0.0f;
                dev.meter.energy = 0.0f;
            } else if (errorMode == 4) {
                dev.valid = false;
                dev.errorMessage = "device_offline";

                dev.meter.voltage = 0.0f;
                dev.meter.current = 0.0f;
                dev.meter.power = 0.0f;
                dev.meter.energy = 0.0f;
            } else {
                dev.valid = true;
                dev.errorMessage = "";

                /*
                 * 电压大概范围：191 ~ 249.5
                 * 电流大概范围：8 ~ 39.2
                 */
                dev.meter.voltage = 221.0f + wave * 1.5f;
                dev.meter.current = 24.0f + wave * 0.8f;

                if (dev.meter.current < 0.0f) {
                    dev.meter.current = 0.0f;
                }

                dev.meter.power = dev.meter.voltage * dev.meter.current;
                dev.meter.energy = 800.0f + tick * 1.5f;
            }

            pack.devices.push_back(dev);
        }

        /*
         * 从站3：水泵继电器
         *
         * errorMode == 1: modbus_timeout
         * errorMode == 4: device_offline
         * 其他状态：继电器状态变化
         */
        {
            DeviceData dev;

            dev.deviceId = 3;
            dev.deviceName = "Pump Relay Controller";
            dev.type = DeviceType::Relay;
            dev.relay.channelCount = 4;

            if (errorMode == 1) {
                dev.valid = false;
                dev.errorMessage = "modbus_timeout";
                dev.relay.relayStates = 0b0000;
            } else if (errorMode == 4) {
                dev.valid = false;
                dev.errorMessage = "device_offline";
                dev.relay.relayStates = 0b0000;
            } else {
                dev.valid = true;
                dev.errorMessage = "";

                if (tick % 3 == 0) {
                    dev.relay.relayStates = 0b0001;
                } else if (tick % 3 == 1) {
                    dev.relay.relayStates = 0b0011;
                } else {
                    dev.relay.relayStates = 0b1111;
                }
            }

            pack.devices.push_back(dev);
        }

        /*
         * 系统信息：CPU / 内存偏高并动态变化
         */
        {
            DeviceData dev;

            dev.deviceId = 100;
            dev.deviceName = "i.MX6ULL_002 System Info";
            dev.type = DeviceType::SysInfo;
            dev.valid = true;
            dev.errorMessage = "";

            dev.sys.kernel = "4.19.35";
            dev.sys.arch = "armv7l";
            dev.sys.os = "Debian 10";
            dev.sys.screenWidth = 480;
            dev.sys.screenHeight = 272;

            dev.sys.cpuUsage = 60.0 + (tick * 11) % 40;
            dev.sys.memoryUsage = 55.0 + (tick * 13) % 45;

            if (dev.sys.cpuUsage > 99.0) {
                dev.sys.cpuUsage = 99.0;
            }

            if (dev.sys.memoryUsage > 99.0) {
                dev.sys.memoryUsage = 99.0;
            }

            pack.devices.push_back(dev);
        }

        handleTelemetryPack(pack);
    }

    /*
     * ============================================================
     * Pack 3:
     * 1号厂房 / 配电房 / i.MX6ULL_001 / RS485-2
     *
     * 测试同一网关下面第二个 RS485 端口。
     * ============================================================
     */
    {
        TelemetryPack pack;

        pack.timestampMs = now;

        pack.site.factoryId = "factory_001";
        pack.site.factoryName = "Factory 001";

        pack.site.areaId = "area_001";
        pack.site.areaName = "Power Distribution Room";

        pack.site.gatewayId = "gateway_001";
        pack.site.gatewayName = "i.MX6ULL_001";

        pack.site.portId = "port_002";
        pack.site.portName = "RS485-2";

        /*
         * RS485-2 温湿度设备
         * 每 10 秒正常 / 离线切换一次
         */
        {
            DeviceData dev;

            dev.deviceId = 1;
            dev.deviceName = "RS485-2 TH Sensor";
            dev.type = DeviceType::SensorTH;

            if ((tick / 10) % 2 == 1) {
                dev.valid = false;
                dev.errorMessage = "device_offline";

                dev.th.temperature = 0.0f;
                dev.th.humidity = 0.0f;
            } else {
                dev.valid = true;
                dev.errorMessage = "";

                dev.th.temperature = 30.0f + wave * 1.0f;
                dev.th.humidity = 48.0f + wave * 1.5f;

                if (dev.th.humidity < 0.0f) {
                    dev.th.humidity = 0.0f;
                }

                if (dev.th.humidity > 100.0f) {
                    dev.th.humidity = 100.0f;
                }
            }

            pack.devices.push_back(dev);
        }

        /*
         * RS485-2 8通道继电器
         * 做流水灯效果
         */
        {
            DeviceData dev;

            dev.deviceId = 2;
            dev.deviceName = "RS485-2 Relay 8CH";
            dev.type = DeviceType::Relay;
            dev.valid = true;
            dev.errorMessage = "";

            dev.relay.channelCount = 8;
            dev.relay.relayStates = 1 << (tick % 8);

            pack.devices.push_back(dev);
        }

        handleTelemetryPack(pack);
    }

    /*
     * ============================================================
     * Pack 4:
     * 3号厂房 / 仓库 / i.MX6ULL_003 / RS485-1
     *
     * 测试：
     * 1. 仓库温湿度
     * 2. 低电压电表
     * 3. 16通道继电器
     * 4. unknown device
     * ============================================================
     */
    {
        TelemetryPack pack;

        pack.timestampMs = now;

        pack.site.factoryId = "factory_003";
        pack.site.factoryName = "Factory 003";

        pack.site.areaId = "area_001";
        pack.site.areaName = "Warehouse";

        pack.site.gatewayId = "gateway_003";
        pack.site.gatewayName = "i.MX6ULL_003";

        pack.site.portId = "port_001";
        pack.site.portName = "RS485-1";

        /*
         * 从站1：仓库温湿度
         * errorMode == 3 时非法数据
         */
        {
            DeviceData dev;

            dev.deviceId = 1;
            dev.deviceName = "Warehouse TH Sensor";
            dev.type = DeviceType::SensorTH;

            if (errorMode == 3) {
                dev.valid = false;
                dev.errorMessage = "invalid_data";

                dev.th.temperature = -999.0f;
                dev.th.humidity = 999.0f;
            } else {
                dev.valid = true;
                dev.errorMessage = "";

                dev.th.temperature = 24.0f + wave * 1.0f;
                dev.th.humidity = 60.0f + wave * 1.5f;

                if (dev.th.humidity < 0.0f) {
                    dev.th.humidity = 0.0f;
                }

                if (dev.th.humidity > 100.0f) {
                    dev.th.humidity = 100.0f;
                }
            }

            pack.devices.push_back(dev);
        }

        /*
         * 从站2：低电压电表
         * 低电压明显变化，测试电压告警。
         */
        {
            DeviceData dev;

            dev.deviceId = 2;
            dev.deviceName = "Warehouse Low Voltage Meter";
            dev.type = DeviceType::ElectricMeter;
            dev.valid = true;
            dev.errorMessage = "";

            /*
             * 大概范围：138 ~ 214
             */
            dev.meter.voltage = 176.0f + wave * 2.0f;
            dev.meter.current = 10.0f + wave * 0.3f;

            if (dev.meter.current < 0.0f) {
                dev.meter.current = 0.0f;
            }

            dev.meter.power = dev.meter.voltage * dev.meter.current;
            dev.meter.energy = 300.0f + tick * 0.6f;

            pack.devices.push_back(dev);
        }

        /*
         * 从站3：16通道继电器
         * 状态每秒变化。
         */
        {
            DeviceData dev;

            dev.deviceId = 3;
            dev.deviceName = "Warehouse Relay 16CH";
            dev.type = DeviceType::Relay;
            dev.valid = true;
            dev.errorMessage = "";

            dev.relay.channelCount = 16;

            if (tick % 4 == 0) {
                dev.relay.relayStates = 0b0000000000001111;
            } else if (tick % 4 == 1) {
                dev.relay.relayStates = 0b0000000011110000;
            } else if (tick % 4 == 2) {
                dev.relay.relayStates = 0b0000111100000000;
            } else {
                dev.relay.relayStates = 0b1111000000000000;
            }

            pack.devices.push_back(dev);
        }

        /*
         * 从站88：未知设备类型
         * 用来测试程序不要崩。
         */
        {
            DeviceData dev;

            dev.deviceId = 88;
            dev.deviceName = "Unknown Device";
            dev.type = DeviceType::Unknown;
            dev.valid = false;
            dev.errorMessage = "unknown_device_type";

            pack.devices.push_back(dev);
        }

        /*
         * 系统信息：正常变化
         */
        {
            DeviceData dev;

            dev.deviceId = 100;
            dev.deviceName = "i.MX6ULL_003 System Info";
            dev.type = DeviceType::SysInfo;
            dev.valid = true;
            dev.errorMessage = "";

            dev.sys.kernel = "4.19.35";
            dev.sys.arch = "armv7l";
            dev.sys.os = "Debian 10";
            dev.sys.screenWidth = 480;
            dev.sys.screenHeight = 272;

            dev.sys.cpuUsage = 20.0 + (tick * 5) % 80;
            dev.sys.memoryUsage = 30.0 + (tick * 6) % 70;

            pack.devices.push_back(dev);
        }

        handleTelemetryPack(pack);
    }

    std::cout << "PcDataService dynamic mock tick: "
              << tick
              << ", errorMode: "
              << errorMode
              << ", point count: "
              << getLatestPoints().size()
              << std::endl;
}