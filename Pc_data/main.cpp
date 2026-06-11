#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <exception>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "ipc/IpcServer.hpp"
#include "mqtt/MqttClient.hpp"
#include "model/DeviceRecord.hpp"
#include "model/ModelConverter.hpp"
#include "model/PointConfigPackParser.hpp"
#include "model/TelemetryPackParser.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

using namespace std;

static const char* kDeviceRegisterAckTopic = "imx6ull/device/data";

struct MqttConfig
{
    std::string host = "127.0.0.1";
    int port = 1883;
    std::string clientId = "pc_data_001";
    std::vector<std::string> topics = {"pc_data/telemetry/test"};
};

static const char* kMqttConfigPath = "config/mqtt_config.json";

static std::string jsonEscape(const std::string& input)
{
    std::string output;
    output.reserve(input.size());

    for (char ch : input) {
        switch (ch) {
        case '\"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += ch;
            break;
        }
    }

    return output;
}

static std::int64_t currentTimeMs()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
               ).count();
}

static std::string extractJsonStringValue(const std::string& json, const std::string& key)
{
    const std::string pattern = "\"" + key + "\"";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) {
        return "";
    }

    pos = json.find(':', pos + pattern.size());
    if (pos == std::string::npos) {
        return "";
    }

    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) {
        return "";
    }

    std::string result;
    bool escaped = false;
    for (size_t i = pos + 1; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaped) {
            result.push_back(ch);
            escaped = false;
            continue;
        }

        if (ch == '\\') {
            escaped = true;
            continue;
        }

        if (ch == '"') {
            break;
        }

        result.push_back(ch);
    }

    return result;
}

static std::int64_t getJsonInt64(const rapidjson::Value& obj,
                                 const char* key,
                                 std::int64_t defaultValue)
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

static int getJsonInt(const rapidjson::Value& obj, const char* key, int defaultValue)
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

static bool getJsonBool(const rapidjson::Value& obj, const char* key, bool defaultValue)
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

static std::string getJsonString(const rapidjson::Value& obj, const char* key)
{
    if (!obj.IsObject() || !obj.HasMember(key) || !obj[key].IsString()) {
        return "";
    }

    return obj[key].GetString();
}

static std::string buildCommandAckJson(const std::string& cmdId,
                                       bool ok,
                                       const std::string& reason)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"command_ack\",";
    oss << "\"cmd_id\":\"" << jsonEscape(cmdId) << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"timestamp\":" << currentTimeMs();
    oss << "}";

    return oss.str();
}

static std::string valueTypeToString(PointValueType type)
{
    switch (type) {
    case PointValueType::Number:
        return "number";
    case PointValueType::Text:
        return "text";
    case PointValueType::Boolean:
        return "boolean";
    default:
        return "unknown";
    }
}

static std::string buildLatestPointsJson(const std::vector<TelemetryPoint>& points)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"latest_points\",";
    oss << "\"count\":" << points.size() << ",";
    oss << "\"points\":[";

    for (size_t i = 0; i < points.size(); ++i) {
        const TelemetryPoint& p = points[i];

        if (i > 0) {
            oss << ",";
        }

        oss << "{";
        oss << "\"pointId\":\"" << jsonEscape(p.pointId) << "\",";
        oss << "\"timestampMs\":" << p.timestampMs << ",";

        oss << "\"factoryId\":\"" << jsonEscape(p.factoryId) << "\",";
        oss << "\"factoryName\":\"" << jsonEscape(p.factoryName) << "\",";

        oss << "\"areaId\":\"" << jsonEscape(p.areaId) << "\",";
        oss << "\"areaName\":\"" << jsonEscape(p.areaName) << "\",";

        oss << "\"gatewayId\":\"" << jsonEscape(p.gatewayId) << "\",";
        oss << "\"gatewayName\":\"" << jsonEscape(p.gatewayName) << "\",";

        oss << "\"portId\":\"" << jsonEscape(p.portId) << "\",";
        oss << "\"portName\":\"" << jsonEscape(p.portName) << "\",";

        oss << "\"deviceId\":" << p.deviceId << ",";
        oss << "\"deviceName\":\"" << jsonEscape(p.deviceName) << "\",";
        oss << "\"deviceType\":\"" << jsonEscape(p.deviceType) << "\",";

        oss << "\"pointKey\":\"" << jsonEscape(p.pointKey) << "\",";
        oss << "\"pointName\":\"" << jsonEscape(p.pointName) << "\",";
        oss << "\"unit\":\"" << jsonEscape(p.unit) << "\",";

        oss << "\"valueType\":\"" << valueTypeToString(p.valueType) << "\",";

        if (p.valueType == PointValueType::Text) {
            oss << "\"value\":\"" << jsonEscape(p.textValue) << "\",";
            oss << "\"numberValue\":0,";
            oss << "\"textValue\":\"" << jsonEscape(p.textValue) << "\",";
        } else {
            oss << "\"value\":" << p.numberValue << ",";
            oss << "\"numberValue\":" << p.numberValue << ",";
            oss << "\"textValue\":\"\",";
        }

        oss << "\"valid\":" << (p.valid ? "true" : "false") << ",";
        oss << "\"errorMessage\":\"" << jsonEscape(p.errorMessage) << "\"";

        oss << "}";
    }

    oss << "]";
    oss << "}";

    return oss.str();
}

static std::string buildCommandLogUpdateJson(std::int64_t seq,
                                             const std::string& commandType,
                                             const std::string& status,
                                             const std::string& reason,
                                             const std::string& message)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"command_log_update\",";
    oss << "\"seq\":" << seq << ",";
    oss << "\"commandType\":\"" << jsonEscape(commandType) << "\",";
    oss << "\"status\":\"" << jsonEscape(status) << "\",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"message\":\"" << jsonEscape(message) << "\",";
    oss << "\"timestampMs\":" << currentTimeMs();
    oss << "}";
    return oss.str();
}

static std::string buildDevicesSnapshotJson(const std::vector<DeviceRecord>& devices)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"devices_snapshot\",";
    oss << "\"count\":" << devices.size() << ",";
    oss << "\"devices\":[";

    for (size_t i = 0; i < devices.size(); ++i) {
        const DeviceRecord& d = devices[i];
        if (i > 0) {
            oss << ",";
        }

        oss << "{";
        oss << "\"factoryId\":\"" << jsonEscape(d.factoryId) << "\",";
        oss << "\"factoryName\":\"" << jsonEscape(d.factoryName) << "\",";
        oss << "\"areaId\":\"" << jsonEscape(d.areaId) << "\",";
        oss << "\"areaName\":\"" << jsonEscape(d.areaName) << "\",";
        oss << "\"gatewayId\":\"" << jsonEscape(d.gatewayId) << "\",";
        oss << "\"gatewayName\":\"" << jsonEscape(d.gatewayName) << "\",";
        oss << "\"portId\":\"" << jsonEscape(d.portId) << "\",";
        oss << "\"portName\":\"" << jsonEscape(d.portName) << "\",";
        oss << "\"deviceId\":" << d.deviceId << ",";
        oss << "\"deviceName\":\"" << jsonEscape(d.deviceName) << "\",";
        oss << "\"deviceType\":\"" << jsonEscape(d.deviceType) << "\",";
        oss << "\"pollIntervalMs\":" << d.pollIntervalMs << ",";
        oss << "\"expectTelemetry\":" << (d.expectTelemetry ? "true" : "false") << ",";
        oss << "\"enabled\":" << (d.enabled ? "true" : "false") << ",";
        oss << "\"status\":\"" << jsonEscape(d.status) << "\",";
        oss << "\"lastSeenMs\":" << d.lastSeenMs << ",";
        oss << "\"lastOfflineMs\":" << d.lastOfflineMs << ",";
        oss << "\"statusReason\":\"" << jsonEscape(d.statusReason) << "\",";
        oss << "\"createTimeMs\":" << d.createTimeMs << ",";
        oss << "\"updateTimeMs\":" << d.updateTimeMs;
        oss << "}";
    }

    oss << "]";
    oss << "}";

    return oss.str();
}

static std::string buildGatewayStatusSnapshotJson(const std::vector<GatewayStatus>& gateways)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"gateway_status_snapshot\",";
    oss << "\"count\":" << gateways.size() << ",";
    oss << "\"gateways\":[";

    for (size_t i = 0; i < gateways.size(); ++i) {
        const GatewayStatus& g = gateways[i];
        if (i > 0) {
            oss << ",";
        }

        oss << "{";
        oss << "\"gatewayId\":\"" << jsonEscape(g.gatewayId) << "\",";
        oss << "\"gatewayName\":\"" << jsonEscape(g.gatewayName) << "\",";
        oss << "\"factoryId\":\"" << jsonEscape(g.factoryId) << "\",";
        oss << "\"areaId\":\"" << jsonEscape(g.areaId) << "\",";
        oss << "\"status\":\"" << jsonEscape(g.status) << "\",";
        oss << "\"lastRegisterTimeMs\":" << g.lastRegisterTimeMs << ",";
        oss << "\"lastHeartbeatTimeMs\":" << g.lastHeartbeatTimeMs << ",";
        oss << "\"updateTimeMs\":" << g.updateTimeMs;
        oss << "}";
    }

    oss << "]}";
    return oss.str();
}

static std::string buildPortStatusSnapshotJson(const std::vector<GatewayPort>& ports)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"port_status_snapshot\",";
    oss << "\"count\":" << ports.size() << ",";
    oss << "\"ports\":[";

    for (size_t i = 0; i < ports.size(); ++i) {
        const GatewayPort& p = ports[i];
        if (i > 0) {
            oss << ",";
        }

        oss << "{";
        oss << "\"gatewayId\":\"" << jsonEscape(p.gatewayId) << "\",";
        oss << "\"portId\":\"" << jsonEscape(p.portId) << "\",";
        oss << "\"portName\":\"" << jsonEscape(p.portName) << "\",";
        oss << "\"slot\":" << p.slot << ",";
        oss << "\"devicePath\":\"" << jsonEscape(p.devicePath) << "\",";
        oss << "\"baud\":" << p.baud << ",";
        oss << "\"status\":\"" << jsonEscape(p.status) << "\",";
        oss << "\"lastRegisterTimeMs\":" << p.lastRegisterTimeMs << ",";
        oss << "\"updateTimeMs\":" << p.updateTimeMs;
        oss << "}";
    }

    oss << "]}";
    return oss.str();
}

static bool parseGatewayRegister(const std::string& payload, GatewayStatus& gateway)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());
    if (root.HasParseError() || !root.IsObject() || getJsonString(root, "type") != "gateway_register") {
        return false;
    }

    const std::int64_t nowMs = getJsonInt64(root, "timestampMs", currentTimeMs());
    gateway.gatewayId = getJsonString(root, "gatewayId");
    gateway.gatewayName = getJsonString(root, "gatewayName");
    gateway.factoryId = getJsonString(root, "factoryId");
    gateway.areaId = getJsonString(root, "areaId");
    gateway.status = "online";
    gateway.lastRegisterTimeMs = nowMs;
    gateway.lastHeartbeatTimeMs = nowMs;
    gateway.updateTimeMs = nowMs;

    return !gateway.gatewayId.empty();
}

static bool parseGatewayHeartbeat(const std::string& payload,
                                  std::string& gatewayId,
                                  std::int64_t& timestampMs,
                                  std::string& status)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());
    if (root.HasParseError() || !root.IsObject() || getJsonString(root, "type") != "gateway_heartbeat") {
        return false;
    }

    gatewayId = getJsonString(root, "gatewayId");
    timestampMs = getJsonInt64(root, "timestampMs", currentTimeMs());
    status = getJsonString(root, "status");
    if (status.empty()) {
        status = "online";
    }

    return !gatewayId.empty();
}

static bool parsePortRegister(const std::string& payload, GatewayPort& port)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());
    if (root.HasParseError() || !root.IsObject() || getJsonString(root, "type") != "port_register") {
        return false;
    }

    const std::int64_t nowMs = getJsonInt64(root, "timestampMs", currentTimeMs());
    port.gatewayId = getJsonString(root, "gatewayId");
    port.portId = getJsonString(root, "portId");
    port.portName = getJsonString(root, "portName");
    port.slot = getJsonInt(root, "slot", 0);
    port.devicePath = getJsonString(root, "devicePath");
    port.baud = getJsonInt(root, "baud", 0);
    port.status = getJsonString(root, "status");
    if (port.status.empty()) {
        port.status = "connected";
    }
    port.lastRegisterTimeMs = nowMs;
    port.updateTimeMs = nowMs;

    return !port.gatewayId.empty() && !port.portId.empty();
}

static bool parseDeviceRegister(const std::string& payload,
                                DeviceRecord& device,
                                std::uint32_t& sequence,
                                std::string& reason)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    if (getJsonString(root, "type") != "device_register") {
        return false;
    }

    sequence = static_cast<std::uint32_t>(getJsonInt(root, "sequence", getJsonInt(root, "seq", 0)));
    const std::int64_t timestampMs = getJsonInt64(root, "timestampMs", currentTimeMs());
    const rapidjson::Value& site = root.HasMember("site") && root["site"].IsObject()
        ? root["site"]
        : root;

    device.factoryId = getJsonString(site, "factoryId");
    device.factoryName = getJsonString(site, "factoryName");
    device.areaId = getJsonString(site, "areaId");
    device.areaName = getJsonString(site, "areaName");
    device.gatewayId = getJsonString(site, "gatewayId");
    device.gatewayName = getJsonString(site, "gatewayName");
    device.portId = getJsonString(site, "portId");
    device.portName = getJsonString(site, "portName");
    device.deviceId = getJsonInt(root, "deviceId", getJsonInt(root, "slave_id", 0));
    device.deviceName = getJsonString(root, "deviceName");
    device.deviceType = getJsonString(root, "deviceType");
    device.pollIntervalMs = getJsonInt(root, "pollIntervalMs", 1000);
    device.expectTelemetry = getJsonBool(root, "expectTelemetry", device.deviceType != "relay");
    device.enabled = true;
    device.status = "online";
    device.lastSeenMs = timestampMs;
    device.statusReason.clear();
    device.createTimeMs = timestampMs;
    device.updateTimeMs = timestampMs;

    if (device.gatewayId.empty() || device.portId.empty() || device.deviceId <= 0 ||
        device.deviceType.empty()) {
        reason = "invalid_device_register";
        return true;
    }

    if (device.deviceName.empty()) {
        device.deviceName = "Device " + std::to_string(device.deviceId);
    }

    reason.clear();
    return true;
}

static std::string buildDeviceRegisterAckJson(std::uint32_t sequence,
                                              const DeviceRecord& device,
                                              bool ok,
                                              const std::string& reason)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"device_register_ack\",";
    oss << "\"sequence\":" << sequence << ",";
    oss << "\"seq\":" << sequence << ",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"gatewayId\":\"" << jsonEscape(device.gatewayId) << "\",";
    oss << "\"portId\":\"" << jsonEscape(device.portId) << "\",";
    oss << "\"deviceId\":" << device.deviceId << ",";
    oss << "\"timestampMs\":" << currentTimeMs();
    oss << "}";

    return oss.str();
}

static std::string buildHistoryPointsJson(const std::string& pointId,
                                          const std::vector<TelemetryPoint>& points)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"history_points\",";
    oss << "\"pointId\":\"" << jsonEscape(pointId) << "\",";
    oss << "\"count\":" << points.size() << ",";
    oss << "\"points\":[";

    for (size_t i = 0; i < points.size(); ++i) {
        const TelemetryPoint& p = points[i];

        if (i > 0) {
            oss << ",";
        }

        oss << "{";
        oss << "\"timestampMs\":" << p.timestampMs << ",";
        oss << "\"numberValue\":" << p.numberValue << ",";
        oss << "\"textValue\":\"" << jsonEscape(p.textValue) << "\",";
        oss << "\"valid\":" << (p.valid ? "true" : "false");
        oss << "}";
    }

    oss << "]";
    oss << "}";

    return oss.str();
}

static std::string buildDeleteDataAckJson(const std::string& action,
                                          bool ok,
                                          const std::string& reason)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"delete_data_ack\",";
    oss << "\"action\":\"" << jsonEscape(action) << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"timestamp\":" << currentTimeMs();
    oss << "}";

    return oss.str();
}

static std::string buildMqttConfigJson(const MqttConfig& config,
                                       const std::string& status)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"mqtt_config\",";
    oss << "\"host\":\"" << jsonEscape(config.host) << "\",";
    oss << "\"port\":" << config.port << ",";
    oss << "\"status\":\"" << jsonEscape(status) << "\"";
    oss << "}";

    return oss.str();
}

static std::string buildMqttConfigAckJson(bool ok,
                                          const std::string& reason,
                                          const MqttConfig& config,
                                          const std::string& status)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"mqtt_config_ack\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"host\":\"" << jsonEscape(config.host) << "\",";
    oss << "\"port\":" << config.port << ",";
    oss << "\"status\":\"" << jsonEscape(status) << "\"";
    oss << "}";

    return oss.str();
}

static MqttConfig loadMqttConfig()
{
    MqttConfig config;
    std::ifstream file(kMqttConfigPath, std::ios::binary);
    if (!file.is_open()) {
        return config;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    rapidjson::Document root;
    root.Parse(buffer.str().c_str());

    if (root.HasParseError() || !root.IsObject()) {
        cout << "MQTT config parse failed, use defaults" << endl;
        return config;
    }

    const std::string host = getJsonString(root, "host");
    const int port = getJsonInt(root, "port", config.port);

    if (!host.empty()) {
        config.host = host;
    }
    if (port >= 1 && port <= 65535) {
        config.port = port;
    }

    return config;
}

static bool saveMqttConfigFile(const MqttConfig& config)
{
    try {
        std::filesystem::path path(kMqttConfigPath);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
    } catch (const std::exception& e) {
        cout << "MQTT config directory create failed: " << e.what() << endl;
        return false;
    }

    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    writer.StartObject();
    writer.Key("host");
    writer.String(config.host.c_str());
    writer.Key("port");
    writer.Int(config.port);
    writer.EndObject();

    std::ofstream file(kMqttConfigPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    file << buffer.GetString();
    return file.good();
}

static bool parseSaveMqttConfigRequest(const std::string& msg,
                                       MqttConfig& config,
                                       std::string& reason)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    if (getJsonString(root, "type") != "save_mqtt_config") {
        return false;
    }

    const std::string host = getJsonString(root, "host");
    const int port = getJsonInt(root, "port", 0);

    if (host.empty()) {
        reason = "host_empty";
        return true;
    }

    if (port < 1 || port > 65535) {
        reason = "port_invalid";
        return true;
    }

    config.host = host;
    config.port = port;
    reason.clear();
    return true;
}

static bool parseGetMqttConfigRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    return getJsonString(root, "type") == "get_mqtt_config";
}

static bool parseHistoryQuery(const std::string& msg,
                              std::string& pointId,
                              std::int64_t& startMs,
                              std::int64_t& endMs,
                              int& limit)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    if (getJsonString(root, "type") != "query_history") {
        return false;
    }

    pointId = getJsonString(root, "pointId");
    startMs = getJsonInt64(root, "startMs", 0);
    endMs = getJsonInt64(root, "endMs", 0);
    limit = getJsonInt(root, "limit", 1000);

    return !pointId.empty();
}

static bool parseDeleteDeviceRequest(const std::string& msg,
                                     std::string& gatewayId,
                                     std::string& portId,
                                     int& deviceId)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    if (getJsonString(root, "type") != "delete_device_data") {
        return false;
    }

    gatewayId = getJsonString(root, "gatewayId");
    if (gatewayId.empty()) {
        gatewayId = getJsonString(root, "gateway_id");
    }

    portId = getJsonString(root, "portId");
    if (portId.empty()) {
        portId = getJsonString(root, "port_id");
    }

    deviceId = getJsonInt(root, "deviceId", getJsonInt(root, "device_id", 0));

    return !gatewayId.empty() && !portId.empty() && deviceId > 0;
}

static bool parseDeleteMasterRequest(const std::string& msg,
                                     std::string& gatewayId,
                                     std::string& portId)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    if (getJsonString(root, "type") != "delete_master_data") {
        return false;
    }

    gatewayId = getJsonString(root, "gatewayId");
    if (gatewayId.empty()) {
        gatewayId = getJsonString(root, "gateway_id");
    }

    portId = getJsonString(root, "portId");
    if (portId.empty()) {
        portId = getJsonString(root, "port_id");
    }

    return !gatewayId.empty() && !portId.empty();
}

static bool parseClearRecoveredAlarmsRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    return getJsonString(root, "type") == "clear_recovered_alarms";
}

static void sendLatestPoints(IpcServer& ipc, PcDataService& dataService, PcDatabase& database)
{
    std::vector<TelemetryPoint> points = dataService.getLatestPoints();
    if (points.empty() && database.isOpen()) {
        points = database.queryLatestPoints();
    }

    cout << "latest point count: " << points.size() << endl;

    std::string json = buildLatestPointsJson(points);

    cout << "json build ok, size: " << json.size() << endl;

    ipc.sendMessage(json);

    cout << "send latest_points done" << endl;
}

static void sendDevicesSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<DeviceRecord> devices;
    if (database.isOpen()) {
        devices = database.queryDevices();
    }

    ipc.sendMessage(buildDevicesSnapshotJson(devices));
    cout << "send devices_snapshot done, count: " << devices.size() << endl;
}

static void sendGatewayStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<GatewayStatus> gateways;
    if (database.isOpen()) {
        gateways = database.queryGatewayStatuses();
    }

    ipc.sendMessage(buildGatewayStatusSnapshotJson(gateways));
    cout << "send gateway_status_snapshot done, count: " << gateways.size() << endl;
}

static void sendPortStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<GatewayPort> ports;
    if (database.isOpen()) {
        ports = database.queryGatewayPorts();
    }

    ipc.sendMessage(buildPortStatusSnapshotJson(ports));
    cout << "send port_status_snapshot done, count: " << ports.size() << endl;
}

int main()
{
    try {
        cout << "Pc_data start" << endl;

        PcDataService dataService;
        cout << "PcDataService created" << endl;

        PcDatabase database;
        if (!database.openDatabase("db/pc_data.db")) {
            cout << "Database open failed, continue without storage" << endl;
        } else if (!database.initTables()) {
            cout << "Database init failed, continue without storage" << endl;
        }

        cout << "Before create IpcServer" << endl;

        IpcServer ipc(R"(\\.\pipe\PcDataIpcPipe)");

        cout << "IpcServer created" << endl;

        MqttClient mqtt;
        MqttConfig mqttConfig = loadMqttConfig();

        mqtt.setMessageCallback([&](const std::string& topic,
                                    const std::string& payload) {
            cout << "[MQTT RX] topic: " << topic << endl;
            cout << "[MQTT RX] payload: " << payload << endl;

            const std::string messageType = extractJsonStringValue(payload, "type");
            if (messageType == "gateway_register") {
                GatewayStatus gateway;
                if (!parseGatewayRegister(payload, gateway)) {
                    cout << "[MQTT RX] gateway_register parse failed" << endl;
                    return;
                }

                const bool ok = database.isOpen() && database.upsertGatewayStatus(gateway);
                cout << "[MQTT RX] gateway_register "
                     << (ok ? "ok" : "failed")
                     << ", gateway: " << gateway.gatewayId << endl;

                if (ok && ipc.hasClient()) {
                    sendGatewayStatusSnapshot(ipc, database);
                }
                return;
            }

            if (messageType == "gateway_heartbeat") {
                std::string gatewayId;
                std::int64_t timestampMs = 0;
                std::string status;
                if (!parseGatewayHeartbeat(payload, gatewayId, timestampMs, status)) {
                    cout << "[MQTT RX] gateway_heartbeat parse failed" << endl;
                    return;
                }

                const bool ok = database.isOpen() &&
                    database.updateGatewayHeartbeat(gatewayId, timestampMs, status);
                cout << "[MQTT RX] gateway_heartbeat "
                     << (ok ? "ok" : "failed")
                     << ", gateway: " << gatewayId << endl;

                if (ok && ipc.hasClient()) {
                    sendGatewayStatusSnapshot(ipc, database);
                }
                return;
            }

            if (messageType == "port_register") {
                GatewayPort port;
                if (!parsePortRegister(payload, port)) {
                    cout << "[MQTT RX] port_register parse failed" << endl;
                    return;
                }

                const bool ok = database.isOpen() && database.upsertGatewayPort(port);
                cout << "[MQTT RX] port_register "
                     << (ok ? "ok" : "failed")
                     << ", gateway: " << port.gatewayId
                     << ", port: " << port.portId
                     << ", status: " << port.status << endl;

                if (ok && ipc.hasClient()) {
                    sendPortStatusSnapshot(ipc, database);
                }
                return;
            }

            if (messageType == "ack") {
                rapidjson::Document root;
                root.Parse(payload.c_str());
                if (root.HasParseError() || !root.IsObject()) {
                    cout << "[MQTT RX] ack parse failed" << endl;
                    return;
                }

                const std::int64_t seq = getJsonInt64(root, "seq", 0);
                std::string commandType = getJsonString(root, "commandType");
                if (commandType.empty()) {
                    commandType = getJsonString(root, "cmd");
                }
                const std::string ackStatus = getJsonString(root, "status");
                const std::string logStatus = ackStatus == "ok" ? "success" : "failed";
                const std::string reason = getJsonString(root, "reason");
                const std::string message = getJsonString(root, "message");

                if (database.isOpen()) {
                    database.updateCommandLogBySeq(seq, logStatus, reason, message, currentTimeMs());
                }
                if (ipc.hasClient()) {
                    ipc.sendMessage(buildCommandLogUpdateJson(seq, commandType, logStatus, reason, message));
                }
                cout << "[MQTT RX] ack seq: " << seq
                     << ", command: " << commandType
                     << ", status: " << logStatus << endl;
                return;
            }

            if (messageType == "device_register") {
                DeviceRecord device;
                std::uint32_t sequence = 0;
                std::string reason;

                if (!parseDeviceRegister(payload, device, sequence, reason)) {
                    cout << "[MQTT RX] device_register parse failed" << endl;
                    return;
                }

                bool ok = reason.empty();
                if (ok) {
                    if (database.isOpen()) {
                        ok = database.upsertDevice(device);
                        if (!ok) {
                            reason = "device_db_save_failed";
                        }
                    } else {
                        ok = false;
                        reason = "database_not_open";
                    }
                }

                const std::string ack = buildDeviceRegisterAckJson(sequence, device, ok, reason);
                const bool publishOk = mqtt.publish(kDeviceRegisterAckTopic, ack);
                cout << "[MQTT RX] device_register "
                     << (ok ? "ok" : "failed")
                     << ", ack publish: "
                     << (publishOk ? "ok" : "failed")
                     << ", gateway: "
                     << device.gatewayId
                     << ", port: "
                     << device.portId
                     << ", device: "
                     << device.deviceId
                     << endl;

                if (ipc.hasClient()) {
                    sendDevicesSnapshot(ipc, database);
                }
                return;
            }

            if (messageType == "threshold_config") {
                std::vector<PointConfig> configs;
                std::string errorMessage;

                if (!PointConfigPackParser::parseJson(payload, configs, errorMessage)) {
                    cout << "[MQTT RX] threshold_config parse failed: "
                         << errorMessage
                         << endl;
                    return;
                }

                cout << "[MQTT RX] threshold_config parse ok, config count: "
                     << configs.size()
                     << endl;

                if (database.isOpen()) {
                    database.savePointConfigs(configs);
                } else {
                    cout << "[MQTT RX] database is not open, skip point_config storage" << endl;
                }
                return;
            }

            if (messageType != "telemetry_pack") {
                cout << "[MQTT RX] skip unsupported message type: "
                     << messageType
                     << endl;
                return;
            }

            TelemetryPack pack;
            std::string errorMessage;

            if (!TelemetryPackParser::parseJson(payload, pack, errorMessage)) {
                cout << "[MQTT RX] parse failed: " << errorMessage << endl;
                return;
            }

            cout << "[MQTT RX] parse ok, sequence: "
                 << pack.sequence
                 << ", device count: "
                 << pack.devices.size()
                 << endl;

            std::vector<TelemetryPoint> receivedPoints = ModelConverter::toTelemetryPoints(pack);

            cout << "[MQTT RX] received point count: "
                 << receivedPoints.size()
                 << endl;

            dataService.handleTelemetryPack(pack);

            std::vector<TelemetryPoint> points = dataService.getLatestPoints();

            cout << "[MQTT RX] snapshot point count: "
                 << points.size()
                 << endl;

            if (database.isOpen()) {
                database.saveTelemetryPoints(receivedPoints);
            } else {
                cout << "[MQTT RX] database is not open, skip storage" << endl;
            }

            if (ipc.hasClient()) {
                std::string json = buildLatestPointsJson(points);
                cout << "[MQTT RX] send latest_points to Pc_ui, size: "
                     << json.size()
                     << endl;
                ipc.sendMessage(json);
            } else {
                cout << "[MQTT RX] Pc_ui not connected, latest_points not sent" << endl;
            }
        });

        ipc.setClientConnectedCallback([&]() {
            cout << "Pc_ui connected" << endl;

            /*
             * 注意：
             * 你的 IpcServer::sendMessage() 返回值是 void，
             * 所以这里只能直接调用，不能写 bool ok = ...
             */
            ipc.sendMessage(R"({"type":"hello","message":"hello pc_ui"})");

            cout << "send hello done" << endl;

            sendLatestPoints(ipc, dataService, database);
            sendGatewayStatusSnapshot(ipc, database);
            sendPortStatusSnapshot(ipc, database);
            sendDevicesSnapshot(ipc, database);
        });

        ipc.setClientDisconnectedCallback([]() {
            cout << "Pc_ui disconnected" << endl;
        });

        ipc.setMessageCallback([&](const std::string& msg) {
            cout << "Pc_data recv: " << msg << endl;

            std::string pointId;
            std::int64_t startMs = 0;
            std::int64_t endMs = 0;
            int limit = 1000;

            if (parseHistoryQuery(msg, pointId, startMs, endMs, limit)) {
                const std::vector<TelemetryPoint> points =
                    database.queryHistoryPoints(pointId, startMs, endMs, limit);

                ipc.sendMessage(buildHistoryPointsJson(pointId, points));

                cout << "send history_points done, pointId: "
                     << pointId
                     << ", count: "
                     << points.size()
                     << endl;
                return;
            }

            std::string gatewayId;
            std::string portId;
            int deviceId = 0;

            if (parseDeleteDeviceRequest(msg, gatewayId, portId, deviceId)) {
                bool dbOk = false;
                if (database.isOpen()) {
                    dbOk = database.deleteDeviceData(gatewayId, portId, deviceId);
                } else {
                    cout << "delete_device_data skipped: database is not open" << endl;
                }

                const bool snapshotRemoved = dataService.removeDeviceData(gatewayId, portId, deviceId);
                const bool ok = dbOk || snapshotRemoved;
                const std::string reason = ok ? "" : "delete_device_data_failed";

                ipc.sendMessage(buildDeleteDataAckJson("delete_device_data", ok, reason));
                sendLatestPoints(ipc, dataService, database);
                sendDevicesSnapshot(ipc, database);

                cout << "delete_device_data done, gateway: "
                     << gatewayId
                     << ", port: "
                     << portId
                     << ", device: "
                     << deviceId
                     << ", dbOk: "
                     << dbOk
                     << ", snapshotRemoved: "
                     << snapshotRemoved
                     << endl;
                return;
            }

            if (parseDeleteMasterRequest(msg, gatewayId, portId)) {
                bool dbOk = false;
                if (database.isOpen()) {
                    dbOk = database.deleteMasterData(gatewayId, portId);
                } else {
                    cout << "delete_master_data skipped: database is not open" << endl;
                }

                const bool snapshotRemoved = dataService.removeMasterData(gatewayId, portId);
                const bool ok = dbOk || snapshotRemoved;
                const std::string reason = ok ? "" : "delete_master_data_failed";

                ipc.sendMessage(buildDeleteDataAckJson("delete_master_data", ok, reason));
                sendLatestPoints(ipc, dataService, database);
                sendDevicesSnapshot(ipc, database);

                cout << "delete_master_data done, gateway: "
                     << gatewayId
                     << ", port: "
                     << portId
                     << ", dbOk: "
                     << dbOk
                     << ", snapshotRemoved: "
                     << snapshotRemoved
                     << endl;
                return;
            }

            if (parseClearRecoveredAlarmsRequest(msg)) {
                bool dbOk = false;
                if (database.isOpen()) {
                    dbOk = database.clearRecoveredAlarms();
                } else {
                    cout << "clear_recovered_alarms skipped: database is not open" << endl;
                }

                const std::string reason = dbOk ? "" : "clear_recovered_alarms_failed";
                ipc.sendMessage(buildDeleteDataAckJson("clear_recovered_alarms", dbOk, reason));

                cout << "clear_recovered_alarms done, dbOk: "
                     << dbOk
                     << endl;
                return;
            }

            if (parseGetMqttConfigRequest(msg)) {
                ipc.sendMessage(buildMqttConfigJson(mqttConfig, mqtt.status()));
                cout << "send mqtt_config done" << endl;
                return;
            }

            MqttConfig requestedMqttConfig = mqttConfig;
            std::string mqttConfigReason;
            if (parseSaveMqttConfigRequest(msg, requestedMqttConfig, mqttConfigReason)) {
                if (!mqttConfigReason.empty()) {
                    ipc.sendMessage(buildMqttConfigAckJson(false, mqttConfigReason, mqttConfig, mqtt.status()));
                    cout << "save_mqtt_config rejected: " << mqttConfigReason << endl;
                    return;
                }

                if (!saveMqttConfigFile(requestedMqttConfig)) {
                    ipc.sendMessage(buildMqttConfigAckJson(false, "config_save_failed", mqttConfig, mqtt.status()));
                    cout << "save_mqtt_config file write failed" << endl;
                    return;
                }

                mqttConfig = requestedMqttConfig;
                const bool connectOk = mqtt.connectToBroker(
                    mqttConfig.host,
                    mqttConfig.port,
                    mqttConfig.clientId,
                    mqttConfig.topics);

                ipc.sendMessage(buildMqttConfigAckJson(connectOk, connectOk ? "" : "mqtt_connect_failed", mqttConfig, mqtt.status()));

                cout << "save_mqtt_config done, host: "
                     << mqttConfig.host
                     << ", port: "
                     << mqttConfig.port
                     << ", connectOk: "
                     << connectOk
                     << endl;
                return;
            }

            /*
             * 兼容你之前 Pc_ui 可能发送的 get_snapshot。
             * 后面建议统一改成 get_latest_points。
             */
            if (msg.find("get_gateway_status") != std::string::npos) {
                sendGatewayStatusSnapshot(ipc, database);
            } else if (msg.find("get_port_status") != std::string::npos) {
                sendPortStatusSnapshot(ipc, database);
            } else if (msg.find("get_devices") != std::string::npos) {
                sendDevicesSnapshot(ipc, database);
            } else if (msg.find("get_latest_points") != std::string::npos ||
                msg.find("get_snapshot") != std::string::npos) {

                sendLatestPoints(ipc, dataService, database);
            } else if (msg.find("\"type\":\"command\"") != std::string::npos ||
                       msg.find("\"msg_type\":\"command\"") != std::string::npos) {
                rapidjson::Document root;
                root.Parse(msg.c_str());
                const std::string cmdId = extractJsonStringValue(msg, "cmd_id");
                const std::string commandType = root.IsObject() ? getJsonString(root, "commandType") : "";
                std::string gatewayId;
                std::string portId;
                if (root.IsObject() && root.HasMember("target") && root["target"].IsObject()) {
                    gatewayId = getJsonString(root["target"], "gatewayId");
                    portId = getJsonString(root["target"], "portId");
                }
                if (gatewayId.empty() && root.IsObject()) {
                    gatewayId = getJsonString(root, "gatewayId");
                }
                if (portId.empty() && root.IsObject()) {
                    portId = getJsonString(root, "portId");
                }

                if (commandType == "add_device") {
                    const std::int64_t seq = root.IsObject() ? getJsonInt64(root, "seq", 0) : 0;
                    int deviceId = 0;
                    if (root.IsObject() && root.HasMember("device") && root["device"].IsObject()) {
                        deviceId = getJsonInt(root["device"], "deviceId", getJsonInt(root["device"], "slaveAddress", 0));
                    }
                    if (deviceId <= 0 && root.IsObject()) {
                        deviceId = getJsonInt(root, "deviceId", 0);
                    }

                    if (seq <= 0) {
                        ipc.sendMessage(buildCommandAckJson(cmdId, false, "invalid_argument"));
                        cout << "add_device rejected, missing seq" << endl;
                        return;
                    }

                    const std::string commandId = cmdId.empty()
                        ? std::string("CMD") + std::to_string(seq)
                        : cmdId;
                    if (database.isOpen()) {
                        database.createCommandLog(commandId,
                                                  seq,
                                                  commandType,
                                                  gatewayId,
                                                  portId,
                                                  deviceId,
                                                  currentTimeMs());
                    }

                    if (!database.isOpen() || !database.isGatewayPortConnected(gatewayId, portId)) {
                        if (database.isOpen()) {
                            database.updateCommandLogBySeq(seq,
                                                           "failed",
                                                           "port_not_found",
                                                           "gateway port is not connected",
                                                           currentTimeMs());
                        }
                        ipc.sendMessage(buildCommandAckJson(cmdId, false, "port_not_found"));
                        cout << "add_device rejected, port not connected, gateway: "
                             << gatewayId << ", port: " << portId << endl;
                        return;
                    }

                    const std::string topic = "cmd/" + gatewayId;
                    const bool publishOk = mqtt.publish(topic, msg);
                    if (database.isOpen()) {
                        database.updateCommandLogBySeq(seq,
                                                       publishOk ? "sent" : "failed",
                                                       publishOk ? "" : "mqtt_publish_failed",
                                                       publishOk ? "command sent" : "mqtt publish failed",
                                                       currentTimeMs());
                    }
                    ipc.sendMessage(buildCommandAckJson(cmdId, publishOk, publishOk ? "sent" : "mqtt_publish_failed"));
                    cout << "add_device publish "
                         << (publishOk ? "ok" : "failed")
                         << ", topic: " << topic
                         << ", cmd_id: " << cmdId << endl;
                    return;
                }

                ipc.sendMessage(buildCommandAckJson(cmdId, true, ""));
                cout << "send command_ack done, cmd_id: " << cmdId << endl;
            } else {
                ipc.sendMessage(R"({"type":"ack","cmd":"unknown","status":"ok","message":"Pc_data received"})");

                cout << "send unknown ack done" << endl;
            }
        });

        cout << "Before ipc.start" << endl;

        if (!ipc.start()) {
            cout << "IpcServer start failed" << endl;
            return -1;
        }

        cout << "Pc_data IPC server running..." << endl;

        cout << "MQTT broker: " << mqttConfig.host << ":" << mqttConfig.port << endl;
        cout << "MQTT subscribe topic: pc_data/telemetry/test" << endl;

        if (!mqtt.connectToBroker(mqttConfig.host, mqttConfig.port, mqttConfig.clientId, mqttConfig.topics)) {
            cout << "MQTT connectToBroker call failed" << endl;
        }

        std::int64_t lastOfflineScanMs = 0;
        while (true) {
            const std::int64_t nowMs = currentTimeMs();
            if (database.isOpen() && nowMs - lastOfflineScanMs >= 1000) {
                lastOfflineScanMs = nowMs;
                const int offlineChanged = database.markOfflineDevices(nowMs, 30000);
                if (offlineChanged > 0 && ipc.hasClient()) {
                    sendDevicesSnapshot(ipc, database);
                }
                const int staleGatewayChanged = database.markStaleGateways(nowMs, 30000);
                if (staleGatewayChanged > 0 && ipc.hasClient()) {
                    sendGatewayStatusSnapshot(ipc, database);
                }
            }
            this_thread::sleep_for(chrono::seconds(1));
        }

        return 0;
    }
    catch (const std::exception& e) {
        cout << "std exception: " << e.what() << endl;
        return -1;
    }
    catch (...) {
        cout << "unknown exception" << endl;
        return -1;
    }
}
