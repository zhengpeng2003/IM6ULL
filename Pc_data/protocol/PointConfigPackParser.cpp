#include "PointConfigPackParser.hpp"

#include "model/ModelConverter.hpp"

#include <chrono>
#include <string>
#include <utility>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "common/JsonUtils.hpp"

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

bool getOptionalDoubleAny(const rapidjson::Value& obj,
                          std::initializer_list<const char*> keys,
                          double& outValue)
{
    if (!obj.IsObject()) {
        return false;
    }
    for (const char* key : keys) {
        if (getOptionalDouble(obj, key, outValue)) {
            return true;
        }
    }
    return false;
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
    return ModelConverter::buildPointId(config.factoryId,
                                        config.areaId,
                                        config.gatewayId,
                                        config.portId,
                                        config.deviceId,
                                        config.pointKey);
}

PointConfig basePointConfig(const rapidjson::Value& site,
                            const std::string& deviceName,
                            const std::string& deviceType,
                            int deviceId,
                            std::int64_t timestampMs)
{
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
    config.valueType = "number";
    config.enabled = true;
    return config;
}

void appendLegacyThresholdConfig(std::vector<PointConfig>& configs,
                                 const rapidjson::Value& site,
                                 const std::string& deviceName,
                                 const std::string& deviceType,
                                 int deviceId,
                                 bool deviceThresholdEnabled,
                                 const rapidjson::Value& thresholds,
                                 const rapidjson::Value& thresholdConfig,
                                 std::int64_t timestampMs)
{
    const bool enableAlarm = thresholdConfig.IsObject()
        ? getJsonBoolAny(thresholdConfig, {"enable"}, deviceThresholdEnabled)
        : deviceThresholdEnabled;

    PointConfig temperature = basePointConfig(site, deviceName, deviceType, deviceId, timestampMs);
    temperature.pointKey = "temperature";
    temperature.pointName = "temperature";
    temperature.unit = "c";
    temperature.enableAlarm = enableAlarm;
    temperature.hasAlarmHigh =
        (thresholds.IsObject() && getOptionalDoubleAny(thresholds, {"temp_high", "tempHigh"}, temperature.alarmHigh)) ||
        (thresholdConfig.IsObject() && getOptionalDoubleAny(thresholdConfig, {"temp_high", "tempHigh"}, temperature.alarmHigh));

    PointConfig humidity = basePointConfig(site, deviceName, deviceType, deviceId, timestampMs);
    humidity.pointKey = "humidity";
    humidity.pointName = "humidity";
    humidity.unit = "%";
    humidity.enableAlarm = enableAlarm;
    humidity.hasAlarmHigh =
        (thresholds.IsObject() && getOptionalDoubleAny(thresholds, {"humi_high", "humiHigh"}, humidity.alarmHigh)) ||
        (thresholdConfig.IsObject() && getOptionalDoubleAny(thresholdConfig, {"humi_high", "humiHigh"}, humidity.alarmHigh));

    if (!temperature.factoryId.empty() && !temperature.areaId.empty() &&
        !temperature.gatewayId.empty() && !temperature.portId.empty() &&
        temperature.deviceId > 0 && temperature.hasAlarmHigh) {
        temperature.pointId = buildPointId(temperature);
        configs.push_back(temperature);
    }
    if (!humidity.factoryId.empty() && !humidity.areaId.empty() &&
        !humidity.gatewayId.empty() && !humidity.portId.empty() &&
        humidity.deviceId > 0 && humidity.hasAlarmHigh) {
        humidity.pointId = buildPointId(humidity);
        configs.push_back(humidity);
    }
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
        getJsonInt64Any(root, {"timestampMs", "timestamp", "time"}, currentTimeMs());
    const rapidjson::Value& site = nestedObject(root, "site");

    std::vector<PointConfig> configs;
    const rapidjson::Value& devices = root["devices"];
    for (rapidjson::SizeType i = 0; i < devices.Size(); ++i) {
        const rapidjson::Value& device = devices[i];
        if (!device.IsObject()) {
            continue;
        }

        const int deviceId = getJsonIntAny(device, {"deviceId", "slave_id", "slaveAddress"}, 0);
        const std::string deviceName = getString(device, "deviceName");
        const std::string deviceType = getJsonStringAny(device, {"deviceType", "device_type"});
        const bool deviceThresholdEnabled =
            getJsonBoolAny(device, {"threshold_enabled", "thresholdEnabled"}, false);
        const rapidjson::Value& thresholdConfig = device.HasMember("threshold_config") && device["threshold_config"].IsObject()
            ? device["threshold_config"]
            : nestedObject(device, "thresholdConfig");
        const rapidjson::Value& thresholds = device.HasMember("thresholds") && device["thresholds"].IsObject()
            ? device["thresholds"]
            : nestedObject(thresholdConfig, "thresholds");

        if (!device.HasMember("points") || !device["points"].IsArray()) {
            appendLegacyThresholdConfig(configs,
                                        site,
                                        deviceName,
                                        deviceType,
                                        deviceId,
                                        deviceThresholdEnabled,
                                        thresholds,
                                        thresholdConfig,
                                        timestampMs);
            continue;
        }

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
            config.enableAlarm = getJsonBoolAny(point, {"enableAlarm", "enable_alarm"}, deviceThresholdEnabled);
            config.hasAlarmLow = getOptionalDoubleAny(point, {"alarmLow", "alarm_low"}, config.alarmLow);
            config.hasAlarmHigh = getOptionalDoubleAny(point, {"alarmHigh", "alarm_high"}, config.alarmHigh);
            if (config.pointKey == "temperature" && thresholds.IsObject()) {
                config.hasAlarmHigh = config.hasAlarmHigh ||
                    getOptionalDoubleAny(thresholds, {"temp_high", "tempHigh"}, config.alarmHigh);
            }
            if (config.pointKey == "humidity" && thresholds.IsObject()) {
                config.hasAlarmHigh = config.hasAlarmHigh ||
                    getOptionalDoubleAny(thresholds, {"humi_high", "humiHigh"}, config.alarmHigh);
            }
            if (thresholdConfig.IsObject()) {
                config.enableAlarm = getJsonBoolAny(thresholdConfig, {"enable"}, config.enableAlarm);
                if (config.pointKey == "temperature") {
                    config.hasAlarmHigh = config.hasAlarmHigh ||
                        getOptionalDoubleAny(thresholdConfig, {"temp_high", "tempHigh"}, config.alarmHigh);
                }
                if (config.pointKey == "humidity") {
                    config.hasAlarmHigh = config.hasAlarmHigh ||
                        getOptionalDoubleAny(thresholdConfig, {"humi_high", "humiHigh"}, config.alarmHigh);
                }
            }
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
