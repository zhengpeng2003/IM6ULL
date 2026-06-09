#include "ModelConverter.hpp"

#include <string>

std::vector<TelemetryPoint> ModelConverter::toTelemetryPoints(const TelemetryPack& pack)
{
    std::vector<TelemetryPoint> points;

    for (const auto& device : pack.devices) {
        switch (device.type) {
        case DeviceType::SensorTH:
            appendTemperatureHumidityPoints(pack, device, points);
            break;

        case DeviceType::ElectricMeter:
            appendElectricMeterPoints(pack, device, points);
            break;

        case DeviceType::Relay:
            appendRelayPoints(pack, device, points);
            break;

        case DeviceType::SysInfo:
            appendSysInfoPoints(pack, device, points);
            break;

        case DeviceType::Unknown:
            appendUnknownDevicePoint(pack, device, points);
            break;

        default:
            break;
        }
    }

    return points;
}

TelemetryPoint ModelConverter::makeBasePoint(const TelemetryPack& pack,
                                             const DeviceData& device)
{
    TelemetryPoint point;

    point.timestampMs = pack.timestampMs;

    point.factoryId = pack.site.factoryId;
    point.factoryName = pack.site.factoryName;

    point.areaId = pack.site.areaId;
    point.areaName = pack.site.areaName;

    point.gatewayId = pack.site.gatewayId;
    point.gatewayName = pack.site.gatewayName;

    point.portId = pack.site.portId;
    point.portName = pack.site.portName;

    point.deviceId = device.deviceId;
    point.deviceName = device.deviceName;
    point.deviceType = deviceTypeToString(device.type);

    point.valid = device.valid;
    point.errorMessage = device.errorMessage;

    return point;
}

std::string ModelConverter::buildPointId(const TelemetryPack& pack,
                                         const DeviceData& device,
                                         const std::string& pointKey)
{
    return pack.site.factoryId + "." +
           pack.site.areaId + "." +
           pack.site.gatewayId + "." +
           pack.site.portId + "." +
           std::to_string(device.deviceId) + "." +
           pointKey;
}

void ModelConverter::appendTemperatureHumidityPoints(const TelemetryPack& pack,
                                                     const DeviceData& device,
                                                     std::vector<TelemetryPoint>& points)
{
    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "temperature";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Temperature";
        point.unit = "C";
        point.valueType = PointValueType::Number;
        point.numberValue = device.th.temperature;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "humidity";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Humidity";
        point.unit = "%";
        point.valueType = PointValueType::Number;
        point.numberValue = device.th.humidity;
        point.textValue = "";

        points.push_back(point);
    }
}

void ModelConverter::appendElectricMeterPoints(const TelemetryPack& pack,
                                               const DeviceData& device,
                                               std::vector<TelemetryPoint>& points)
{
    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "voltage";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Voltage";
        point.unit = "V";
        point.valueType = PointValueType::Number;
        point.numberValue = device.meter.voltage;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "current";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Current";
        point.unit = "A";
        point.valueType = PointValueType::Number;
        point.numberValue = device.meter.current;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "power";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Power";
        point.unit = "W";
        point.valueType = PointValueType::Number;
        point.numberValue = device.meter.power;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "energy";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Energy";
        point.unit = "kWh";
        point.valueType = PointValueType::Number;
        point.numberValue = device.meter.energy;
        point.textValue = "";

        points.push_back(point);
    }
}

void ModelConverter::appendRelayPoints(const TelemetryPack& pack,
                                       const DeviceData& device,
                                       std::vector<TelemetryPoint>& points)
{
    int channelCount = device.relay.channelCount;

    if (channelCount <= 0) {
        channelCount = 16;
    }

    if (channelCount > 16) {
        channelCount = 16;
    }

    for (int i = 0; i < channelCount; ++i) {
        const bool isOn = (device.relay.relayStates & (1u << i)) != 0;

        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "relay_" + std::to_string(i + 1);
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Relay " + std::to_string(i + 1);
        point.unit = "";
        point.valueType = PointValueType::Boolean;
        point.numberValue = isOn ? 1.0 : 0.0;
        point.textValue = isOn ? "on" : "off";

        points.push_back(point);
    }
}

void ModelConverter::appendSysInfoPoints(const TelemetryPack& pack,
                                         const DeviceData& device,
                                         std::vector<TelemetryPoint>& points)
{
    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "kernel";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Kernel Version";
        point.unit = "";
        point.valueType = PointValueType::Text;
        point.numberValue = 0.0;
        point.textValue = device.sys.kernel;

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "arch";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Architecture";
        point.unit = "";
        point.valueType = PointValueType::Text;
        point.numberValue = 0.0;
        point.textValue = device.sys.arch;

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "os";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Operating System";
        point.unit = "";
        point.valueType = PointValueType::Text;
        point.numberValue = 0.0;
        point.textValue = device.sys.os;

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "screen_width";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Screen Width";
        point.unit = "px";
        point.valueType = PointValueType::Number;
        point.numberValue = device.sys.screenWidth;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "screen_height";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Screen Height";
        point.unit = "px";
        point.valueType = PointValueType::Number;
        point.numberValue = device.sys.screenHeight;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "cpu_usage";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "CPU Usage";
        point.unit = "%";
        point.valueType = PointValueType::Number;
        point.numberValue = device.sys.cpuUsage;
        point.textValue = "";

        points.push_back(point);
    }

    {
        TelemetryPoint point = makeBasePoint(pack, device);

        point.pointKey = "memory_usage";
        point.pointId = buildPointId(pack, device, point.pointKey);
        point.pointName = "Memory Usage";
        point.unit = "%";
        point.valueType = PointValueType::Number;
        point.numberValue = device.sys.memoryUsage;
        point.textValue = "";

        points.push_back(point);
    }
}

void ModelConverter::appendUnknownDevicePoint(const TelemetryPack& pack,
                                              const DeviceData& device,
                                              std::vector<TelemetryPoint>& points)
{
    TelemetryPoint point = makeBasePoint(pack, device);

    point.pointKey = "unknown_status";
    point.pointId = buildPointId(pack, device, point.pointKey);
    point.pointName = "Unknown Device Status";
    point.unit = "";
    point.valueType = PointValueType::Text;
    point.numberValue = 0.0;
    point.textValue = device.errorMessage.empty() ? "unknown_device_type" : device.errorMessage;
    point.valid = false;
    point.errorMessage = point.textValue;

    points.push_back(point);
}
