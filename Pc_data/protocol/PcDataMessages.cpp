#include "PcDataMessages.hpp"

#include <sstream>

#include <rapidjson/document.h>

#include "common/JsonUtils.hpp"

namespace {

std::string valueTypeToString(PointValueType type)
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

} // namespace

std::string buildCommandAckJson(const std::string& cmdId,
                                bool ok,
                                const std::string& reason,
                                std::int64_t seq,
                                const std::string& commandType,
                                const std::string& stage,
                                const std::string& message)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"command_ack\",";
    oss << "\"cmd_id\":\"" << jsonEscape(cmdId) << "\",";
    oss << "\"seq\":" << seq << ",";
    oss << "\"cmd\":\"" << jsonEscape(commandType) << "\",";
    oss << "\"command\":\"" << jsonEscape(commandType) << "\",";
    oss << "\"commandType\":\"" << jsonEscape(commandType) << "\",";
    oss << "\"stage\":\"" << jsonEscape(stage) << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"message\":\"" << jsonEscape(message) << "\",";
    oss << "\"timestamp\":" << currentTimeMs();
    oss << "}";

    return oss.str();
}

std::string buildLatestPointsJson(const std::vector<TelemetryPoint>& points)
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

std::string buildCommandLogUpdateJson(std::int64_t seq,
                                      const std::string& commandType,
                                      const std::string& status,
                                      const std::string& reason,
                                      const std::string& message,
                                      const CommandLogTarget* target)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"command_log_update\",";
    if (target) {
        oss << "\"cmd_id\":\"" << jsonEscape(target->commandId) << "\",";
    }
    oss << "\"seq\":" << seq << ",";
    oss << "\"commandType\":\"" << jsonEscape(commandType) << "\",";
    oss << "\"stage\":\"done\",";
    oss << "\"status\":\"" << jsonEscape(status) << "\",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"message\":\"" << jsonEscape(message) << "\",";
    if (target) {
        oss << "\"gatewayId\":\"" << jsonEscape(target->gatewayId) << "\",";
        oss << "\"portId\":\"" << jsonEscape(target->portId) << "\",";
        oss << "\"deviceId\":" << target->deviceId << ",";
    }
    oss << "\"timestampMs\":" << currentTimeMs();
    oss << "}";
    return oss.str();
}

std::string buildDevicesSnapshotJson(const std::vector<DeviceRecord>& devices)
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

std::string buildGatewayStatusSnapshotJson(const std::vector<GatewayStatus>& gateways)
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

std::string buildPortStatusSnapshotJson(const std::vector<GatewayPort>& ports)
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

bool parseGatewayRegister(const std::string& payload, GatewayStatus& gateway)
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

bool parseGatewayHeartbeat(const std::string& payload,
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

bool parsePortRegister(const std::string& payload, GatewayPort& port)
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

bool parseDeviceRegister(const std::string& payload,
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

std::string buildDeviceRegisterAckJson(std::uint32_t sequence,
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

std::string buildHistoryPointsJson(const std::string& pointId,
                                   const std::vector<TelemetryPoint>& points,
                                   bool ok,
                                   const std::string& reason,
                                   const std::string& message)
{
    const std::string actualReason = reason.empty() && ok && points.empty() ? "no_data" : reason;
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"history_points\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(actualReason) << "\",";
    if (!message.empty()) {
        oss << "\"message\":\"" << jsonEscape(message) << "\",";
    }
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

std::string buildDeleteDataAckJson(const std::string& action,
                                   bool ok,
                                   const std::string& reason)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"delete_data_ack\",";
    oss << "\"action\":\"" << jsonEscape(action) << "\",";
    oss << "\"ok\":" << (ok ? "true" : "false") << ",";
    oss << "\"reason\":\"" << jsonEscape(reason) << "\",";
    oss << "\"message\":\"" << jsonEscape(message) << "\",";
    oss << "\"timestamp\":" << currentTimeMs();
    oss << "}";

    return oss.str();
}

std::string buildMqttConfigJson(const MqttConfig& config,
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

std::string buildMqttConfigAckJson(bool ok,
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

std::string buildSyncConfigResultJson(bool success,
                                      const std::string& message,
                                      int portCount,
                                      int deviceCount)
{
    std::ostringstream oss;

    oss << "{";
    oss << "\"type\":\"sync_config_result\",";
    oss << "\"success\":" << (success ? "true" : "false") << ",";
    oss << "\"message\":\"" << jsonEscape(message) << "\",";
    oss << "\"portCount\":" << portCount << ",";
    oss << "\"deviceCount\":" << deviceCount;
    oss << "}";

    return oss.str();
}

bool parseSaveMqttConfigRequest(const std::string& msg,
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

bool parseGetMqttConfigRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    return getJsonString(root, "type") == "get_mqtt_config";
}

bool parseHistoryQuery(const std::string& msg,
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
    startMs = getJsonInt64(root, "startMs", getJsonInt64(root, "from", 0));
    endMs = getJsonInt64(root, "endMs", getJsonInt64(root, "to", 0));
    limit = getJsonInt(root, "limit", 1000);

    return !pointId.empty();
}

bool parseDeleteDeviceRequest(const std::string& msg,
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

bool parseDeleteMasterRequest(const std::string& msg,
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

bool parseClearRecoveredAlarmsRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    return getJsonString(root, "type") == "clear_recovered_alarms";
}

bool parseClearAllDataRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        return false;
    }

    return getJsonString(root, "type") == "clear_all_data";
}
