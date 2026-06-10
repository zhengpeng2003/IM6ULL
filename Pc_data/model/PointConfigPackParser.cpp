#include "PointConfigPackParser.hpp"

#include <chrono>
#include <string>
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

bool getOptionalDouble(const rapidjson::Value& obj,
                       const char* key,
                       double& outValue)
{
    if (!obj.IsObject() || !obj.HasMember(key)) {
        return false;
    }

    const rapidjson::Value& value = obj[key];
    if (value.IsNull()) {
        return false;
    }
    if (!value.IsNumber()) {
        return false;
    }

    outValue = value.GetDouble();
    return true;
}

const rapidjson::Value& nestedObject(const rapidjson::Value& obj, const char* key)
{
    static const rapidjson::Value empty;

    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsObject()) {
        return empty;
    }
    return obj[key];
}

std::string buildPointId(const PointConfig& config)
{
    return config.factoryId + "." +
           config.areaId + "." +
           config.gatewayId + "." +
           config.portId + "." +
           std::to_string(config.deviceId) + "." +
           config.pointKey;
}

}

bool PointConfigPackParser::parseJson(const std::string& payload,
                                      std::vector<PointConfig>& outConfigs,
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

    if (getString(root, "type") != "threshold_config") {
        errorMessage = "unsupported type";
        return false;
    }

    if (!root.HasMember("devices") || !root["devices"].IsArray()) {
        errorMessage = "devices is missing or not array";
        return false;
    }

    const std::int64_t timestampMs =
        getInt64(root, "timestampMs", currentTimeMs());
    const rapidjson::Value& site = nestedObject(root, "site");

    std::vector<PointConfig> configs;
    const rapidjson::Value& devices = root["devices"];
    for (rapidjson::SizeType i = 0; i < devices.Size(); ++i) {
        const rapidjson::Value& device = devices[i];
        if (!device.IsObject() || !device.HasMember("points") || !device["points"].IsArray()) {
            continue;
        }

        const int deviceId = getInt(device, "deviceId", getInt(device, "slave_id", 0));
        const std::string deviceName = getString(device, "deviceName");
        const std::string deviceType = getString(device, "deviceType", getString(device, "device_type"));

        const rapidjson::Value& points = device["points"];
        for (rapidjson::SizeType j = 0; j < points.Size(); ++j) {
            const rapidjson::Value& point = points[j];
            if (!point.IsObject()) {
                continue;
            }

            PointConfig config;
            config.timestampMs = timestampMs;
            config.factoryId = getString(site, "factoryId");
            config.factoryName = getString(site, "factoryName");
            config.areaId = getString(site, "areaId");
            config.areaName = getString(site, "areaName");
            config.gatewayId = getString(site, "gatewayId");
            config.gatewayName = getString(site, "gatewayName");
            config.portId = getString(site, "portId");
            config.portName = getString(site, "portName");
            config.deviceId = deviceId;
            config.deviceName = deviceName;
            config.deviceType = deviceType;
            config.pointKey = getString(point, "pointKey");
            config.pointName = getString(point, "pointName");
            config.unit = getString(point, "unit");
            config.valueType = getString(point, "valueType", "number");
            config.enableAlarm = getBool(point, "enableAlarm", getBool(point, "enable_alarm", false));
            config.hasAlarmLow = getOptionalDouble(point, "alarmLow", config.alarmLow) ||
                                 getOptionalDouble(point, "alarm_low", config.alarmLow);
            config.hasAlarmHigh = getOptionalDouble(point, "alarmHigh", config.alarmHigh) ||
                                  getOptionalDouble(point, "alarm_high", config.alarmHigh);
            config.enabled = true;

            if (config.pointKey.empty() || config.factoryId.empty() ||
                config.areaId.empty() || config.gatewayId.empty() ||
                config.portId.empty() || config.deviceId <= 0) {
                continue;
            }

            config.pointId = buildPointId(config);
            configs.push_back(config);
        }
    }

    outConfigs = std::move(configs);
    errorMessage.clear();
    return true;
}

std::int64_t PointConfigPackParser::currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}
