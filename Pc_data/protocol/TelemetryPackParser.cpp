#include "TelemetryPackParser.hpp"

#include <chrono>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

namespace {

std::string getString(const rapidjson::Value& obj,
                      const char* key,
                      const std::string& defaultValue = "")
{
    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsString()) {
        return defaultValue;
    }

    return obj[key].GetString();
}

int getInt(const rapidjson::Value& obj, const char* key, int defaultValue = 0)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsInt()) {
        return value.GetInt();
    }

    if (value.IsNumber()) {
        return static_cast<int>(value.GetDouble());
    }

    return defaultValue;
}

std::uint32_t getUInt32(const rapidjson::Value& obj,
                        const char* key,
                        std::uint32_t defaultValue = 0)
{
    const int value = getInt(obj, key, static_cast<int>(defaultValue));
    return value < 0 ? defaultValue : static_cast<std::uint32_t>(value);
}

std::int64_t getInt64(const rapidjson::Value& obj,
                      const char* key,
                      std::int64_t defaultValue = 0)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsInt64()) {
        return value.GetInt64();
    }

    if (value.IsNumber()) {
        return static_cast<std::int64_t>(value.GetDouble());
    }

    return defaultValue;
}

double getDouble(const rapidjson::Value& obj, const char* key, double defaultValue = 0.0)
{
    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsNumber()) {
        return defaultValue;
    }

    return obj[key].GetDouble();
}

bool getBool(const rapidjson::Value& obj, const char* key, bool defaultValue = false)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return defaultValue;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsBool()) {
        return value.GetBool();
    }

    if (value.IsInt()) {
        return value.GetInt() != 0;
    }

    return defaultValue;
}

DeviceType parseDeviceType(const std::string& type)
{
    if (type == "sensor_th") {
        return DeviceType::SensorTH;
    }

    if (type == "electric_meter" || type == "meter") {
        return DeviceType::ElectricMeter;
    }

    if (type == "relay") {
        return DeviceType::Relay;
    }

    if (type == "sysinfo") {
        return DeviceType::SysInfo;
    }

    return DeviceType::Unknown;
}

const rapidjson::Value& nestedObject(const rapidjson::Value& obj, const char* key)
{
    static const rapidjson::Value empty;

    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsObject()) {
        return empty;
    }

    return obj[key];
}

}

bool TelemetryPackParser::parseJson(const std::string& payload,
                                    TelemetryPack& outPack,
                                    std::string& errorMessage)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());

    if (root.HasParseError()) {
        errorMessage = std::string("json parse error: ") +
                       rapidjson::GetParseError_En(root.GetParseError()) +
                       " at offset " +
                       std::to_string(root.GetErrorOffset());
        return false;
    }

    if (!root.IsObject()) {
        errorMessage = "root is not object";
        return false;
    }

    const std::string type = getString(root, "type");
    if (type != "telemetry_pack") {
        errorMessage = "unsupported type: " + type;
        return false;
    }

    TelemetryPack pack;
    pack.version = getUInt32(root, "version", 1);
    pack.sequence = getUInt32(root, "sequence", 0);
    pack.timestampMs = getInt64(root, "timestampMs", currentTimeMs());
    pack.sourceId = getString(root, "sourceId");
    pack.targetId = getString(root, "targetId");

    const rapidjson::Value& site = nestedObject(root, "site");
    pack.site.factoryId = getString(site, "factoryId");
    pack.site.factoryName = getString(site, "factoryName");
    pack.site.areaId = getString(site, "areaId");
    pack.site.areaName = getString(site, "areaName");
    pack.site.gatewayId = getString(site, "gatewayId");
    pack.site.gatewayName = getString(site, "gatewayName");
    pack.site.portId = getString(site, "portId");
    pack.site.portName = getString(site, "portName");

    if (!root.HasMember("devices") || !root["devices"].IsArray()) {
        errorMessage = "devices is missing or not array";
        return false;
    }

    const rapidjson::Value& devices = root["devices"];
    for (rapidjson::SizeType i = 0; i < devices.Size(); ++i) {
        const rapidjson::Value& item = devices[i];
        if (!item.IsObject()) {
            continue;
        }

        DeviceData device;
        device.deviceId = getInt(item, "deviceId", 0);
        device.deviceName = getString(item, "deviceName");
        const std::string deviceTypeText = getString(item, "deviceType", getString(item, "type", "unknown"));
        device.type = parseDeviceType(deviceTypeText);
        device.valid = getBool(item, "valid", true);
        device.errorMessage = getString(item, "errorMessage");
        if (device.deviceId <= 0) {
            device.valid = false;
            device.errorMessage = device.errorMessage.empty() ? "missing_deviceId" : device.errorMessage;
        }
        if (device.type == DeviceType::Unknown) {
            device.valid = false;
            device.errorMessage = device.errorMessage.empty() ? "unknown_device_type" : device.errorMessage;
        }

        const rapidjson::Value& th = nestedObject(item, "th");
        const rapidjson::Value& meter = nestedObject(item, "meter");
        const rapidjson::Value& relay = nestedObject(item, "relay");
        const rapidjson::Value& sys = nestedObject(item, "sys");

        device.th.temperature = static_cast<float>(
            getDouble(item, "temperature", getDouble(th, "temperature", 0.0)));
        device.th.humidity = static_cast<float>(
            getDouble(item, "humidity", getDouble(th, "humidity", 0.0)));

        device.meter.voltage = static_cast<float>(
            getDouble(item, "voltage", getDouble(meter, "voltage", 0.0)));
        device.meter.current = static_cast<float>(
            getDouble(item, "current", getDouble(meter, "current", 0.0)));
        device.meter.power = static_cast<float>(
            getDouble(item, "power", getDouble(meter, "power", 0.0)));
        device.meter.energy = static_cast<float>(
            getDouble(item, "energy", getDouble(meter, "energy", 0.0)));

        device.relay.channelCount = getInt(item, "channelCount", getInt(relay, "channelCount", 16));
        device.relay.relayStates = static_cast<std::uint16_t>(
            getInt(item, "relayStates", getInt(item, "states", getInt(relay, "relayStates", 0))));

        if (device.valid && device.type == DeviceType::SensorTH &&
            (!item.HasMember("temperature") && !(th.IsObject() && th.HasMember("temperature")))) {
            device.valid = false;
            device.errorMessage = "missing_temperature";
        }
        if (device.valid && device.type == DeviceType::Relay &&
            !item.HasMember("relayStates") && !item.HasMember("states") && !(relay.IsObject() && relay.HasMember("relayStates"))) {
            device.valid = false;
            device.errorMessage = "missing_relayStates";
        }

        device.sys.kernel = getString(item, "kernel", getString(sys, "kernel"));
        device.sys.arch = getString(item, "arch", getString(sys, "arch"));
        device.sys.os = getString(item, "os", getString(sys, "os"));
        device.sys.screenWidth = getInt(item, "screenWidth", getInt(sys, "screenWidth", 0));
        device.sys.screenHeight = getInt(item, "screenHeight", getInt(sys, "screenHeight", 0));
        device.sys.cpuUsage = getDouble(item, "cpuUsage", getDouble(sys, "cpuUsage", 0.0));
        device.sys.memoryUsage = getDouble(item, "memoryUsage", getDouble(sys, "memoryUsage", 0.0));

        pack.devices.push_back(device);
    }

    outPack = std::move(pack);
    errorMessage.clear();
    return true;
}

std::int64_t TelemetryPackParser::currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
