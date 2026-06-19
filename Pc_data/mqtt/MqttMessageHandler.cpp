#include "MqttMessageHandler.hpp"

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rapidjson/document.h>

#include "common/JsonUtils.hpp"
#include "ipc/IpcServer.hpp"
#include "ipc/PcUiPublisher.hpp"
#include "model/DeviceRecord.hpp"
#include "model/ModelConverter.hpp"
#include "mqtt/MqttClient.hpp"
#include "protocol/PcDataMessages.hpp"
#include "protocol/PointConfigPackParser.hpp"
#include "protocol/TelemetryPackParser.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

namespace {

const std::int64_t kUnknownTelemetryConfigRequestIntervalMs = 5000;
const size_t kMaxTelemetryQueueSize = 256;
const auto kLatestPublishInterval = std::chrono::milliseconds(150);

std::string portIdFromSlot(int slot)
{
    if (slot < 0) {
        slot = 0;
    }

    std::ostringstream oss;
    oss << "port_";
    if (slot + 1 < 10) {
        oss << "00";
    } else if (slot + 1 < 100) {
        oss << "0";
    }
    oss << (slot + 1);
    return oss.str();
}

std::string portNameFromSlot(int slot)
{
    if (slot < 0) {
        slot = 0;
    }

    std::ostringstream oss;
    oss << "RS485-" << (slot + 1);
    return oss.str();
}

DeviceRecord deviceRecordFromAddDeviceAck(const rapidjson::Value& root)
{
    DeviceRecord device;
    const int slot = getJsonInt(root, "slot", getJsonInt(root, "master_slot", 0));
    const std::int64_t nowMs = currentTimeMs();
    const rapidjson::Value& target = root.HasMember("target") && root["target"].IsObject()
                                         ? root["target"]
                                         : root;

    device.factoryId = getJsonStringAny(root, {"factoryId", "factory_id"});
    if (device.factoryId.empty()) device.factoryId = "factory_001";
    device.factoryName = getJsonStringAny(root, {"factoryName", "factory_name"});
    if (device.factoryName.empty()) device.factoryName = "工厂";
    device.areaId = getJsonStringAny(root, {"areaId", "area_id"});
    if (device.areaId.empty()) device.areaId = "area_001";
    device.areaName = getJsonStringAny(root, {"areaName", "area_name"});
    if (device.areaName.empty()) device.areaName = "车间";
    device.gatewayId = getJsonString(target, "gatewayId");
    if (device.gatewayId.empty()) device.gatewayId = getJsonStringAny(root, {"gatewayId", "gateway_id"});
    device.gatewayName = getJsonStringAny(root, {"gatewayName", "gateway_name"});
    if (device.gatewayName.empty()) device.gatewayName = "IMX6ULL Gateway";
    device.portId = getJsonString(target, "portId");
    if (device.portId.empty()) device.portId = getJsonStringAny(root, {"portId", "port_id"});
    if (device.portId.empty()) device.portId = portIdFromSlot(slot);
    device.portName = portNameFromSlot(slot);
    device.deviceId = getJsonIntAny(root, {"deviceId", "slave_id", "slaveAddress", "device_id", "slave_addr"}, 0);
    device.deviceName = "Device " + std::to_string(device.deviceId);
    device.deviceType = getJsonStringAny(root, {"deviceType", "device_type"});
    device.pollIntervalMs = getJsonInt(root, "pollIntervalMs", getJsonInt(root, "poll_interval_ms", 1000));
    device.expectTelemetry = device.deviceType != "relay";
    device.enabled = true;
    device.status = "online";
    device.lastSeenMs = nowMs;
    device.createTimeMs = nowMs;
    device.updateTimeMs = nowMs;
    return device;
}

DeviceRecord pendingDeviceRecordFromTarget(const PendingCommandTarget& target)
{
    DeviceRecord device;
    const std::int64_t nowMs = currentTimeMs();
    device.factoryId = "factory_001";
    device.factoryName = "工厂";
    device.areaId = "area_001";
    device.areaName = "车间";
    device.gatewayId = target.gatewayId;
    device.gatewayName = "IMX6ULL Gateway";
    device.portId = target.portId;
    device.portName = target.portId;
    device.deviceId = target.deviceId;
    device.deviceName = "Device " + std::to_string(target.deviceId);
    device.deviceType = "pending";
    device.status = "unknown";
    device.lifecycleStatus = "active";
    device.createTimeMs = nowMs;
    device.updateTimeMs = nowMs;
    return device;
}

void sendDevicesSnapshotWithPendingDevice(IpcServer& ipc,
                                          PcDatabase& database,
                                          const PendingCommandTarget& target)
{
    std::vector<DeviceRecord> devices;
    bool found = false;

    if (database.isOpen()) {
        devices = database.queryDevices();
        for (const DeviceRecord& device : devices) {
            if (device.gatewayId == target.gatewayId &&
                device.portId == target.portId &&
                device.deviceId == target.deviceId) {
                found = true;
                break;
            }
        }
    }

    if (!found && target.deviceId > 0) {
        devices.push_back(pendingDeviceRecordFromTarget(target));
    }

    const std::string json = buildDevicesSnapshotJson(devices);
    const bool ok = ipc.sendMessage(json);
    std::cout << "send devices_snapshot after add_device ack count=" << devices.size()
              << " pending=" << (!found ? "true" : "false")
              << " ok=" << (ok ? "true" : "false")
              << " jsonBytes=" << json.size() << std::endl;
}

DeviceRecord deviceRecordFromTelemetry(const TelemetryPack& pack, const DeviceData& telemetryDevice)
{
    DeviceRecord device;
    const std::int64_t nowMs = currentTimeMs();

    device.factoryId = pack.site.factoryId.empty() ? "factory_001" : pack.site.factoryId;
    device.factoryName = pack.site.factoryName.empty() ? "工厂" : pack.site.factoryName;
    device.areaId = pack.site.areaId.empty() ? "area_001" : pack.site.areaId;
    device.areaName = pack.site.areaName.empty() ? "车间" : pack.site.areaName;
    device.gatewayId = pack.site.gatewayId;
    device.gatewayName = pack.site.gatewayName.empty() ? "IMX6ULL Gateway" : pack.site.gatewayName;
    device.portId = pack.site.portId;
    device.portName = pack.site.portName;
    device.deviceId = telemetryDevice.deviceId;
    device.deviceName = telemetryDevice.deviceName.empty()
                            ? "Device " + std::to_string(telemetryDevice.deviceId)
                            : telemetryDevice.deviceName;
    device.deviceType = deviceTypeToString(telemetryDevice.type);
    device.pollIntervalMs = 1000;
    device.expectTelemetry = device.deviceType != "relay";
    device.enabled = true;
    device.status = telemetryDevice.valid ? "online" :
                        (telemetryDevice.errorMessage == "device_offline" ? "offline" : "error");
    device.lastSeenMs = nowMs;
    device.lastOfflineMs = device.status == "offline" ? nowMs : 0;
    device.statusReason = telemetryDevice.valid ? "" : telemetryDevice.errorMessage;
    device.createTimeMs = nowMs;
    device.updateTimeMs = nowMs;
    return device;
}

bool isRelayPointKey(const std::string& pointKey)
{
    if (pointKey.rfind("relay_", 0) != 0) {
        return false;
    }

    try {
        const int channel = std::stoi(pointKey.substr(6));
        return channel >= 1 && channel <= 16;
    } catch (...) {
        return false;
    }
}

bool isDefaultTelemetryPointKey(const std::string& deviceType, const std::string& pointKey)
{
    if (deviceType == "sensor_th") {
        return pointKey == "temperature" || pointKey == "humidity";
    }

    if (deviceType == "relay") {
        return pointKey == "state" || pointKey == "ch1" || isRelayPointKey(pointKey);
    }

    if (deviceType == "electric_meter" || deviceType == "meter") {
        return pointKey == "voltage" ||
               pointKey == "current" ||
               pointKey == "power" ||
               pointKey == "energy";
    }

    return false;
}

bool deviceTypesMatch(const std::string& registeredType, const std::string& telemetryType)
{
    if (registeredType.empty() || telemetryType.empty() || telemetryType == "unknown") {
        return true;
    }

    if (registeredType == telemetryType) {
        return true;
    }

    return (registeredType == "meter" && telemetryType == "electric_meter") ||
           (registeredType == "electric_meter" && telemetryType == "meter");
}

std::string normalizeTelemetryPointKey(const std::string& pointKey)
{
    if (pointKey.rfind("relay.ch", 0) == 0) {
        return "relay_" + pointKey.substr(8);
    }

    if (pointKey.rfind("relay_", 0) == 0) {
        return pointKey;
    }

    if (pointKey.rfind("ch", 0) == 0) {
        return "relay_" + pointKey.substr(2);
    }

    if (pointKey == "relay" || pointKey == "state") {
        return "relay_1";
    }

    return pointKey;
}

std::string telemetryDeviceKey(const std::string& portId, int deviceId)
{
    return portId + "/" + std::to_string(deviceId);
}

std::string buildSnapshotPointId(const DeviceRecord& device,
                                 const std::string& pointKey)
{
    return ModelConverter::buildPointId(device.factoryId,
                                        device.areaId,
                                        device.gatewayId,
                                        device.portId,
                                        device.deviceId,
                                        pointKey);
}

bool getOptionalSnapshotDouble(const rapidjson::Value& obj,
                               const char* snakeKey,
                               const char* camelKey,
                               double& value)
{
    if (!obj.IsObject()) {
        return false;
    }

    const rapidjson::Value* candidate = nullptr;
    if (obj.HasMember(snakeKey)) {
        candidate = &obj[snakeKey];
    } else if (obj.HasMember(camelKey)) {
        candidate = &obj[camelKey];
    }

    if (!candidate || candidate->IsNull() || !candidate->IsNumber()) {
        return false;
    }

    value = candidate->GetDouble();
    return true;
}

void appendThresholdPointConfig(std::vector<PointConfig>& configs,
                                const rapidjson::Value& threshold,
                                const DeviceRecord& device,
                                const std::string& pointKey,
                                const std::string& pointName,
                                const std::string& unit,
                                std::int64_t timestampMs)
{
    if (!threshold.IsObject()) {
        return;
    }

    PointConfig config;
    config.timestampMs = timestampMs;
    config.factoryId = device.factoryId;
    config.areaId = device.areaId;
    config.gatewayId = device.gatewayId;
    config.gatewayName = device.gatewayName;
    config.portId = device.portId;
    config.portName = device.portName;
    config.deviceId = device.deviceId;
    config.deviceName = device.deviceName;
    config.deviceType = device.deviceType;
    config.pointKey = pointKey;
    config.pointName = pointName;
    config.unit = unit;
    config.valueType = "number";
    config.enableAlarm = getJsonBool(threshold, "enable_alarm", getJsonBool(threshold, "enableAlarm", false));
    config.hasAlarmLow = getOptionalSnapshotDouble(threshold, "alarm_low", "alarmLow", config.alarmLow);
    config.hasAlarmHigh = getOptionalSnapshotDouble(threshold, "alarm_high", "alarmHigh", config.alarmHigh);
    config.enabled = true;

    if (config.factoryId.empty() ||
        config.areaId.empty() ||
        config.gatewayId.empty() ||
        config.portId.empty() ||
        config.deviceId <= 0 ||
        config.pointKey.empty()) {
        return;
    }

    config.pointId = buildSnapshotPointId(device, pointKey);
    configs.push_back(config);
}

std::string getJsonStringCompat(const rapidjson::Value& obj, const char* snakeKey, const char* camelKey)
{
    std::string value = getJsonString(obj, snakeKey);
    return value.empty() && camelKey ? getJsonString(obj, camelKey) : value;
}

double getJsonDoubleCompat(const rapidjson::Value& obj, const char* key, double defaultValue)
{
    if (obj.IsObject() && obj.HasMember(key) && obj[key].IsNumber()) {
        return obj[key].GetDouble();
    }
    return defaultValue;
}

std::string buildStableAlarmId(const AlarmEvent& event)
{
    std::ostringstream out;
    out << (event.gatewayId.empty() ? "unknown_gateway" : event.gatewayId) << '.'
        << (event.portId.empty() ? "unknown_port" : event.portId) << '.'
        << event.deviceId << '.'
        << (event.pointKey.empty() ? "unknown" : event.pointKey) << '.'
        << (event.alarmType.empty() ? "emergency" : event.alarmType);
    return out.str();
}

std::string buildRequestConfigSnapshotJson(const std::string& gatewayId)
{
    const std::int64_t seq = currentTimeMs();
    std::ostringstream payload;
    payload << "{\"type\":\"command\",\"cmd\":\"request_config_snapshot\",\"seq\":" << seq
            << ",\"cmdType\":\"request_config_snapshot\""
            << ",\"gatewayId\":\"" << jsonEscape(gatewayId) << "\""
            << ",\"timestampMs\":" << currentTimeMs()
            << ",\"target\":{\"gatewayId\":\"" << jsonEscape(gatewayId) << "\"}"
            << ",\"payload\":{\"gatewayId\":\"" << jsonEscape(gatewayId) << "\"}}";
    return payload.str();
}

std::string defaultUpTopic(const std::string& gatewayId)
{
    return gatewayId.empty() ? std::string() : "gateway/" + gatewayId + "/up";
}

std::string defaultCmdTopic(const std::string& gatewayId)
{
    return gatewayId.empty() ? std::string() : "cmd/" + gatewayId;
}

std::string makePortCommandTopic(const std::string& gatewayId, const std::string& portId)
{
    return gatewayId.empty() || portId.empty() ? std::string() : "cmd/" + gatewayId + "/" + portId;
}

bool parsePortUpTopic(const std::string& topic,
                      std::string& gatewayId,
                      std::string& portId)
{
    const std::string prefix = "gateway/";
    const std::string suffix = "/up";

    if (topic.rfind(prefix, 0) != 0 ||
        topic.size() <= prefix.size() + suffix.size() ||
        topic.compare(topic.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }

    const std::string body = topic.substr(prefix.size(),
                                          topic.size() - prefix.size() - suffix.size());
    const std::size_t slash = body.find('/');

    if (slash == std::string::npos ||
        slash == 0 ||
        slash + 1 >= body.size() ||
        body.find('/', slash + 1) != std::string::npos) {
        return false;
    }

    gatewayId = body.substr(0, slash);
    portId = body.substr(slash + 1);
    return !gatewayId.empty() && !portId.empty();
}

std::string extractGatewayIdCompat(const rapidjson::Value& root)
{
    std::string gatewayId = getJsonStringAny(root, {"gatewayId", "gateway_id"});
    if (gatewayId.empty() &&
        root.IsObject() &&
        root.HasMember("site") &&
        root["site"].IsObject()) {
        gatewayId = getJsonStringAny(root["site"], {"gatewayId", "gateway_id"});
    }
    return gatewayId;
}

std::string extractPortIdCompat(const rapidjson::Value& root)
{
    std::string portId = getJsonStringAny(root, {"portId", "port_id"});
    if (portId.empty() &&
        root.IsObject() &&
        root.HasMember("site") &&
        root["site"].IsObject()) {
        portId = getJsonStringAny(root["site"], {"portId", "port_id"});
    }
    return portId;
}

bool parsePortStatusDetails(const rapidjson::Value& root,
                            GatewayPort& port,
                            std::string& reason,
                            std::string& message)
{
    if (!root.IsObject()) {
        reason = "invalid_payload";
        message = "port_status payload is not object";
        return false;
    }

    const std::int64_t nowMs = currentTimeMs();

    port.gatewayId = extractGatewayIdCompat(root);
    port.portId = extractPortIdCompat(root);
    port.portName = getJsonStringAny(root, {"portName", "port_name"});
    port.slot = getJsonInt(root, "slot", getJsonInt(root, "master_slot", 0));
    port.devicePath = getJsonStringAny(root, {"devicePath", "device_path", "port", "path"});
    port.baud = getJsonInt(root, "baud", 0);

    if (root.HasMember("connected")) {
        port.status = getJsonBool(root, "connected", false) ? "connected" : "disconnected";
    } else {
        port.status = getJsonString(root, "status");
        if (port.status.empty()) {
            port.status = "connected";
        }
    }

    port.lastRegisterTimeMs = nowMs;
    port.updateTimeMs = nowMs;

    if (port.gatewayId.empty()) {
        reason = "missing_field";
        message = "port_status missing gatewayId";
        return false;
    }

    if (port.portId.empty()) {
        reason = "missing_field";
        message = "port_status missing portId";
        return false;
    }

    reason.clear();
    message.clear();
    return true;
}

bool isValidPublishTopic(const std::string& topic)
{
    return !topic.empty() &&
           topic.find('#') == std::string::npos &&
           topic.find('+') == std::string::npos;
}

bool isValidRegisterTopic(const std::string& topic)
{
    return topic == "gateway/register";
}

bool isValidGatewayUpTopic(const std::string& topic,
                           const std::string& gatewayId,
                           std::string& parsedGatewayId)
{
    const std::string prefix = "gateway/";
    const std::string suffix = "/up";

    if (topic.rfind(prefix, 0) != 0 ||
        topic.size() <= prefix.size() + suffix.size() ||
        topic.compare(topic.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }

    parsedGatewayId = topic.substr(prefix.size(),
                                   topic.size() - prefix.size() - suffix.size());
    return !parsedGatewayId.empty() &&
           parsedGatewayId.find('/') == std::string::npos &&
           parsedGatewayId == gatewayId;
}

bool publishConfigSnapshotRequest(MqttClient& mqtt,
                                  PcDatabase& database,
                                  const std::string& gatewayId,
                                  const std::string& portId,
                                  const std::string& reason)
{
    (void)database;
    (void)portId;

    if (gatewayId.empty()) {
        return false;
    }

    const std::string topic = defaultCmdTopic(gatewayId);
    if (!isValidPublishTopic(topic)) {
        std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
                  << " topic=<unregistered> cmd=request_config_snapshot seq=0 failed=gateway_not_registered"
                  << " reason=" << reason << std::endl;
        return false;
    }

    const std::string payload = buildRequestConfigSnapshotJson(gatewayId);
    const bool ok = mqtt.publish(topic, payload);

    std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
              << " topic=" << topic
              << " cmd=request_config_snapshot seq=0"
              << " reason=" << reason
              << " status=" << (ok ? "sent" : "failed") << std::endl;

    return ok;
}

std::string buildRegisterAckJson(const std::string& cmd,
                                 std::int64_t seq,
                                 bool ok,
                                 const std::string& status,
                                 const std::string& gatewayId,
                                 const std::string& portId,
                                 std::int64_t configVersion,
                                 const std::string& reason,
                                 const std::string& message,
                                 bool retryable,
                                 int registeredDeviceCount,
                                 int registeredPointCount,
                                 int deviceId)
{
    std::ostringstream out;

    const std::string actualStatus = ok && status.empty() ? "success" : status;
    const std::string actualReason = ok ? "saved" : reason;
    const std::string actualMessage = ok ? "regsit save ok" : message;

    out << "{"
        << "\"type\":\"ack\","
        << "\"cmd\":\"" << jsonEscape(cmd) << "\","
        << "\"seq\":" << seq << ","
        << "\"ok\":" << (ok ? "true" : "false") << ","
        << "\"status\":\"" << jsonEscape(actualStatus) << "\","
        << "\"gatewayId\":\"" << jsonEscape(gatewayId) << "\",";

    if (!portId.empty()) {
        out << "\"portId\":\"" << jsonEscape(portId) << "\",";
    }

    if (deviceId > 0) {
        out << "\"deviceId\":" << deviceId << ",";
    }

    if (configVersion > 0) {
        out << "\"configVersion\":" << configVersion << ",";
    }

    out << "\"reason\":\"" << jsonEscape(actualReason) << "\","
        << "\"commandType\":\"" << jsonEscape(cmd) << "\","
        << "\"cmdType\":\"" << jsonEscape(cmd) << "\","
        << "\"message\":\"" << jsonEscape(actualMessage) << "\","
        << "\"retryable\":" << (retryable ? "true" : "false");

    if (registeredDeviceCount >= 0) {
        out << ",\"registeredDeviceCount\":" << registeredDeviceCount;
    }

    if (registeredPointCount >= 0) {
        out << ",\"registeredPointCount\":" << registeredPointCount;
    }

    out << ",\"timestampMs\":" << currentTimeMs()
        << "}";

    return out.str();
}

bool publishRegisterAck(MqttClient& mqtt,
                        const std::string& cmd,
                        std::int64_t seq,
                        bool ok,
                        const std::string& gatewayId,
                        const std::string& portId,
                        std::int64_t configVersion,
                        const std::string& reason,
                        const std::string& message,
                        bool retryable,
                        int registeredDeviceCount = -1,
                        int registeredPointCount = -1,
                        int deviceId = -1)
{
    if (gatewayId.empty()) {
        std::cout << "[MQTT TX ACK] skip register ack cmd=" << cmd
                  << " seq=" << seq
                  << " reason=gateway_id_missing" << std::endl;
        return false;
    }

    const std::string status = ok ? "success" : "failed";
    const std::string actualReason = ok ? "saved" : reason;
    const std::string actualMessage = ok ? "regsit save ok" : message;
    const bool actualRetryable = ok ? false : retryable;

    const std::string topic = portId.empty()
                                  ? defaultCmdTopic(gatewayId)
                                  : makePortCommandTopic(gatewayId, portId);

    if (!isValidPublishTopic(topic)) {
        std::cout << "[MQTT TX ACK] skip register ack cmd=" << cmd
                  << " seq=" << seq
                  << " topic=" << topic
                  << " reason=invalid_publish_topic" << std::endl;
        return false;
    }

    const std::string payload = buildRegisterAckJson(cmd,
                                                     seq,
                                                     ok,
                                                     status,
                                                     gatewayId,
                                                     portId,
                                                     configVersion,
                                                     actualReason,
                                                     actualMessage,
                                                     actualRetryable,
                                                     registeredDeviceCount,
                                                     registeredPointCount,
                                                     deviceId);

    const bool publishOk = mqtt.publish(topic, payload);

    std::cout << "[MQTT TX ACK] cmd=" << cmd
              << " seq=" << seq
              << " topic=" << topic
              << " ok=" << (ok ? "true" : "false")
              << " publish=" << (publishOk ? "ok" : "failed")
              << " reason=" << actualReason
              << std::endl;

    return publishOk;
}

bool isFinalBusinessSuccess(const rapidjson::Value& root)
{
    if (!root.IsObject()) {
        return false;
    }

    return getJsonString(root, "stage") == "done" &&
           getJsonBool(root, "ok", false) &&
           getJsonString(root, "status") == "success";
}

bool parseGatewayRegisterDetails(const std::string& payload,
                                 GatewayStatus& gateway,
                                 GatewayRegistry& registry,
                                 std::vector<GatewayPort>& ports)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());

    if (root.HasParseError() ||
        !root.IsObject() ||
        getJsonString(root, "type") != "gateway_register") {
        return false;
    }

    const rapidjson::Value& site = root.HasMember("site") && root["site"].IsObject()
                                       ? root["site"]
                                       : root;

    const std::int64_t nowMs = currentTimeMs();

    gateway.gatewayId = getJsonStringAny(root, {"gatewayId", "gateway_id"});
    if (gateway.gatewayId.empty()) gateway.gatewayId = getJsonString(site, "gatewayId");

    gateway.gatewayName = getJsonStringAny(root, {"gatewayName", "gateway_name"});
    if (gateway.gatewayName.empty()) gateway.gatewayName = getJsonString(site, "gatewayName");

    gateway.factoryId = getJsonStringAny(root, {"factoryId", "factory_id"});
    if (gateway.factoryId.empty()) gateway.factoryId = getJsonString(site, "factoryId");

    gateway.areaId = getJsonStringAny(root, {"areaId", "area_id"});
    if (gateway.areaId.empty()) gateway.areaId = getJsonString(site, "areaId");

    gateway.status = getJsonString(root, "status");
    if (gateway.status.empty()) gateway.status = "online";

    gateway.lastRegisterTimeMs = nowMs;
    gateway.lastHeartbeatTimeMs = nowMs;
    gateway.updateTimeMs = nowMs;

    if (gateway.gatewayId.empty()) {
        return false;
    }

    registry.gatewayId = gateway.gatewayId;
    registry.gatewayName = gateway.gatewayName;
    registry.status = gateway.status;

    registry.upTopic = getJsonString(root, "upTopic");
    if (registry.upTopic.empty()) registry.upTopic = defaultUpTopic(gateway.gatewayId);

    registry.cmdTopic = getJsonString(root, "cmdTopic");
    if (!isValidPublishTopic(registry.cmdTopic)) registry.cmdTopic = defaultCmdTopic(gateway.gatewayId);

    registry.broadcastTopic = getJsonString(root, "broadcastTopic");
    registry.lastRegisterTimeMs = nowMs;
    registry.lastHeartbeatTimeMs = nowMs;
    registry.updateTimeMs = nowMs;

    if (root.HasMember("ports") && root["ports"].IsArray()) {
        for (const rapidjson::Value& portValue : root["ports"].GetArray()) {
            if (!portValue.IsObject()) {
                continue;
            }

            GatewayPort port;
            port.gatewayId = gateway.gatewayId;
            port.portId = getJsonString(portValue, "portId");
            port.portName = getJsonString(portValue, "portName");
            port.slot = getJsonInt(portValue, "slot", 0);
            port.devicePath = getJsonString(portValue, "port");
            if (port.devicePath.empty()) port.devicePath = getJsonString(portValue, "devicePath");
            if (port.devicePath.empty()) port.devicePath = getJsonString(portValue, "path");
            port.baud = getJsonInt(portValue, "baud", 0);

            if (portValue.HasMember("connected")) {
                port.status = getJsonBool(portValue, "connected", false) ? "connected" : "disconnected";
            } else {
                port.status = getJsonString(portValue, "status");
                if (port.status.empty()) port.status = "connected";
            }

            port.lastRegisterTimeMs = nowMs;
            port.updateTimeMs = nowMs;

            if (!port.portId.empty()) {
                ports.push_back(port);
            }
        }
    }

    return true;
}

bool fillAlarmEventFromObject(const rapidjson::Value& root, AlarmEvent& event, std::string& reason)
{
    if (!root.IsObject()) {
        reason = "payload_not_object";
        return false;
    }

    event.timestampMs = getJsonInt64(root, "timestampMs", getJsonInt64(root, "timestamp", currentTimeMs()));
    event.factoryId = getJsonStringCompat(root, "factory_id", "factoryId");
    event.factoryName = getJsonStringCompat(root, "factory_name", "factoryName");
    event.areaId = getJsonStringCompat(root, "area_id", "areaId");
    event.areaName = getJsonStringCompat(root, "area_name", "areaName");
    event.gatewayId = getJsonStringCompat(root, "gateway_id", "gatewayId");
    event.gatewayName = getJsonStringCompat(root, "gateway_name", "gatewayName");
    event.portId = getJsonStringCompat(root, "port_id", "portId");
    event.portName = getJsonStringCompat(root, "port_name", "portName");
    event.deviceId = getJsonInt(root, "deviceId", getJsonInt(root, "device_id", getJsonInt(root, "slave_addr", 0)));
    event.deviceName = getJsonStringCompat(root, "device_name", "deviceName");
    event.deviceType = getJsonStringCompat(root, "device_type", "deviceType");
    event.pointKey = getJsonStringCompat(root, "point_key", "pointKey");
    event.pointName = getJsonStringCompat(root, "point_name", "pointName");
    event.alarmType = getJsonStringCompat(root, "alarm_type", "alarmType");
    event.level = getJsonString(root, "level");
    if (event.level.empty()) event.level = getJsonString(root, "alarm_level");
    event.state = getJsonString(root, "state");
    if (event.state.empty()) event.state = getJsonString(root, "status");
    if (event.state == "acknowledged") event.state = "acked";
    if (event.state.empty()) event.state = "active";
    event.value = getJsonDoubleCompat(root, "value", getJsonDoubleCompat(root, "trigger_value", 0.0));
    event.threshold = getJsonDoubleCompat(root, "threshold", getJsonDoubleCompat(root, "threshold_value", 0.0));
    event.message = getJsonString(root, "message");
    if (event.message.empty()) event.message = getJsonString(root, "alarm_message");

    event.alarmId = getJsonString(root, "alarm_id");
    if (event.alarmId.empty()) event.alarmId = getJsonString(root, "alarmId");
    if (event.alarmType.empty()) event.alarmType = "emergency";
    if (event.pointKey.empty()) event.pointKey = "unknown";
    if (event.level.empty()) event.level = "warning";
    if (event.message.empty()) event.message = "emergency alarm";
    if (event.alarmId.empty()) event.alarmId = buildStableAlarmId(event);

    if (event.gatewayId.empty()) {
        reason = "gateway_id_missing";
        return false;
    }

    return true;
}

bool parseAlarmEventPayload(const std::string& payload, AlarmEvent& event, std::string& reason)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());

    if (root.HasParseError() || !root.IsObject()) {
        reason = "json_parse_failed";
        return false;
    }

    const std::string type = getJsonString(root, "type");
    const std::string cmd = getJsonString(root, "cmd");

    if (type == "alarm_event") {
        return fillAlarmEventFromObject(root, event, reason);
    }

    if (type != "command" || cmd != "emergency") {
        reason = "not_alarm_payload";
        return false;
    }

    if (!fillAlarmEventFromObject(root, event, reason) && reason != "gateway_id_missing") {
        return false;
    }

    const double temp = getJsonDoubleCompat(root, "temperature", 0.0);
    const double tempHigh = getJsonDoubleCompat(root, "temp_high", getJsonDoubleCompat(root, "temperatureHigh", 0.0));
    const double humi = getJsonDoubleCompat(root, "humidity", 0.0);
    const double humiHigh = getJsonDoubleCompat(root, "humi_high", getJsonDoubleCompat(root, "humidityHigh", 0.0));

    if (event.pointKey == "unknown" && tempHigh > 0.0 && temp > tempHigh) {
        event.pointKey = "temperature";
        event.alarmType = "threshold_high";
        event.value = temp;
        event.threshold = tempHigh;
        event.message = event.message == "emergency alarm" ? "temperature high" : event.message;
    } else if (event.pointKey == "unknown" && humiHigh > 0.0 && humi > humiHigh) {
        event.pointKey = "humidity";
        event.alarmType = "threshold_high";
        event.value = humi;
        event.threshold = humiHigh;
        event.message = event.message == "emergency alarm" ? "humidity high" : event.message;
    }

    event.alarmId = buildStableAlarmId(event);
    return !event.gatewayId.empty();
}

std::string buildAlarmEventJson(const AlarmEvent& event)
{
    std::ostringstream out;

    out << "{\"type\":\"alarm_event\""
        << ",\"alarm_id\":\"" << jsonEscape(event.alarmId) << "\""
        << ",\"alarmId\":\"" << jsonEscape(event.alarmId) << "\""
        << ",\"timestampMs\":" << event.timestampMs
        << ",\"gatewayId\":\"" << jsonEscape(event.gatewayId) << "\""
        << ",\"gateway_id\":\"" << jsonEscape(event.gatewayId) << "\""
        << ",\"portId\":\"" << jsonEscape(event.portId) << "\""
        << ",\"port_id\":\"" << jsonEscape(event.portId) << "\""
        << ",\"deviceId\":" << event.deviceId
        << ",\"device_id\":" << event.deviceId
        << ",\"deviceType\":\"" << jsonEscape(event.deviceType) << "\""
        << ",\"device_type\":\"" << jsonEscape(event.deviceType) << "\""
        << ",\"pointKey\":\"" << jsonEscape(event.pointKey) << "\""
        << ",\"point_key\":\"" << jsonEscape(event.pointKey) << "\""
        << ",\"alarmType\":\"" << jsonEscape(event.alarmType) << "\""
        << ",\"alarm_type\":\"" << jsonEscape(event.alarmType) << "\""
        << ",\"level\":\"" << jsonEscape(event.level) << "\""
        << ",\"value\":" << event.value
        << ",\"threshold\":" << event.threshold
        << ",\"state\":\"" << jsonEscape(event.state) << "\""
        << ",\"status\":\"" << jsonEscape(event.state) << "\""
        << ",\"message\":\"" << jsonEscape(event.message) << "\"}";

    return out.str();
}

bool parseConfigSnapshot(const std::string& payload,
                         const SyncGatewayPending& pending,
                         std::vector<GatewayPort>& ports,
                         std::vector<ConfigSnapshotDevice>& devices,
                         std::vector<PointConfig>& pointConfigs,
                         int& portCount,
                         int& deviceCount)
{
    rapidjson::Document root;
    root.Parse(payload.c_str());

    const std::string snapshotType = root.HasParseError() || !root.IsObject()
                                         ? std::string()
                                         : getJsonString(root, "type");

    if (root.HasParseError() ||
        !root.IsObject() ||
        (snapshotType != "config_snapshot" && snapshotType != "device_config_snapshot")) {
        return false;
    }

    const rapidjson::Value& site =
        root.HasMember("site") && root["site"].IsObject() ? root["site"] : root;

    std::string gatewayId = getJsonString(root, "gatewayId");
    if (gatewayId.empty()) {
        gatewayId = getJsonString(site, "gatewayId");
    }

    const std::string factoryId = getJsonString(site, "factoryId");
    const std::string factoryName = getJsonString(site, "factoryName");
    const std::string areaId = getJsonString(site, "areaId");
    const std::string areaName = getJsonString(site, "areaName");
    const std::string gatewayName = getJsonString(site, "gatewayName");

    if (gatewayId != pending.gatewayId ||
        factoryId.empty() ||
        areaId.empty() ||
        !root.HasMember("ports") ||
        !root["ports"].IsArray()) {
        return false;
    }

    std::set<std::pair<std::string, int> > selected;
    for (const SyncSelectedDevice& device : pending.devices) {
        selected.insert(std::make_pair(device.portId, device.deviceId));
    }

    const std::int64_t timestampMs = getJsonInt64(root, "timestampMs", currentTimeMs());
    const std::int64_t receiveTimeMs = currentTimeMs();

    for (const rapidjson::Value& portValue : root["ports"].GetArray()) {
        if (!portValue.IsObject()) {
            continue;
        }

        GatewayPort port;
        port.gatewayId = gatewayId;
        port.portId = getJsonString(portValue, "portId");
        port.portName = getJsonString(portValue, "portName");
        port.slot = getJsonInt(portValue, "slot", 0);
        port.devicePath = getJsonString(portValue, "port");
        port.baud = getJsonInt(portValue, "baud", 0);
        port.status = getJsonBool(portValue, "connected", false) ? "connected" : "disconnected";
        port.lastRegisterTimeMs = receiveTimeMs;
        port.updateTimeMs = receiveTimeMs;

        if (!port.portId.empty()) {
            ports.push_back(port);
        }

        if (!portValue.HasMember("devices") || !portValue["devices"].IsArray()) {
            continue;
        }

        for (const rapidjson::Value& deviceValue : portValue["devices"].GetArray()) {
            if (!deviceValue.IsObject()) {
                continue;
            }

            const int deviceId =
                getJsonIntAny(deviceValue, {"deviceId", "slave_id", "slaveAddress"}, 0);

            if (!selected.empty() &&
                selected.find(std::make_pair(port.portId, deviceId)) == selected.end()) {
                continue;
            }

            ConfigSnapshotDevice snapshotDevice;
            DeviceRecord& device = snapshotDevice.device;

            device.factoryId = factoryId;
            device.factoryName = factoryName;
            device.areaId = areaId;
            device.areaName = areaName;
            device.gatewayId = gatewayId;
            device.gatewayName = gatewayName;
            device.portId = port.portId;
            device.portName = port.portName;
            device.deviceId = deviceId;
            device.deviceType = getJsonStringAny(deviceValue, {"deviceType", "device_type"});
            device.pollIntervalMs = getJsonInt(deviceValue, "pollIntervalMs", 1000);
            device.expectTelemetry = device.deviceType != "relay";
            device.enabled = true;
            device.status = port.status == "connected" ? "online" : "unknown";
            device.lastSeenMs = port.status == "connected" ? receiveTimeMs : 0;
            device.createTimeMs = receiveTimeMs;
            device.updateTimeMs = receiveTimeMs;
            device.deviceName = "Device " + std::to_string(deviceId);
            snapshotDevice.thresholdEnabled =
                getJsonBoolAny(deviceValue, {"threshold_enabled", "thresholdEnabled"}, false);

            if (device.deviceType == "sensor_th" &&
                ((deviceValue.HasMember("threshold_config") && deviceValue["threshold_config"].IsObject()) ||
                 (deviceValue.HasMember("thresholdConfig") && deviceValue["thresholdConfig"].IsObject()))) {
                const rapidjson::Value& thresholdConfig =
                    deviceValue.HasMember("threshold_config") && deviceValue["threshold_config"].IsObject()
                        ? deviceValue["threshold_config"]
                        : deviceValue["thresholdConfig"];

                const rapidjson::Value* thresholds = nullptr;
                if (thresholdConfig.HasMember("thresholds") && thresholdConfig["thresholds"].IsObject()) {
                    thresholds = &thresholdConfig["thresholds"];
                }

                if (thresholds && thresholds->HasMember("temperature")) {
                    appendThresholdPointConfig(pointConfigs,
                                               (*thresholds)["temperature"],
                                               device,
                                               "temperature",
                                               "temperature",
                                               "c",
                                               timestampMs);
                }

                if (thresholds && thresholds->HasMember("humidity")) {
                    appendThresholdPointConfig(pointConfigs,
                                               (*thresholds)["humidity"],
                                               device,
                                               "humidity",
                                               "humidity",
                                               "%",
                                               timestampMs);
                }
            }

            devices.push_back(snapshotDevice);
        }
    }

    portCount = static_cast<int>(ports.size());
    deviceCount = static_cast<int>(devices.size());
    return true;
}

} // namespace

MqttMessageHandler::MqttMessageHandler(PcDatabase& database,
                                       PcDataService& dataService,
                                       IpcServer& ipc,
                                       MqttClient& mqtt)
    : m_database(database)
    , m_dataService(dataService)
    , m_ipc(ipc)
    , m_mqtt(mqtt)
{
    m_telemetryWorker = std::thread(&MqttMessageHandler::telemetryWorkerLoop, this);
    m_dbWriter = std::thread(&MqttMessageHandler::dbWriterLoop, this);
    m_latestPublisher = std::thread(&MqttMessageHandler::latestPublisherLoop, this);
}

MqttMessageHandler::~MqttMessageHandler()
{
    {
        std::lock_guard<std::mutex> lock(m_telemetryMutex);
        m_stopTelemetryWorker = true;
    }
    m_telemetryCv.notify_one();

    if (m_telemetryWorker.joinable()) {
        m_telemetryWorker.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_dbWriterMutex);
        m_stopDbWriter = true;
    }
    m_dbWriterCv.notify_one();
    if (m_dbWriter.joinable()) {
        m_dbWriter.join();
    }

    {
        std::lock_guard<std::mutex> lock(m_latestPublisherMutex);
        m_stopLatestPublisher = true;
    }
    m_latestPublisherCv.notify_one();
    if (m_latestPublisher.joinable()) {
        m_latestPublisher.join();
    }
}

void MqttMessageHandler::handle(const std::string& topic, const std::string& payload)
{
    const std::string messageType = extractJsonStringValue(payload, "type");

    if (messageType == "telemetry_pack") {
        enqueueTelemetry(topic, payload);
        return;
    }

    std::cout << "[MQTT RX] topic: " << topic << std::endl;
    std::cout << "[MQTT RX] payload bytes: " << payload.size() << std::endl;

    if (messageType == "alarm_event" ||
        (messageType == "command" && extractJsonStringValue(payload, "cmd") == "emergency")) {
        std::string topicGatewayId;
        std::string topicPortId;
        const bool hasPortTopic = parsePortUpTopic(topic, topicGatewayId, topicPortId);

        if (!hasPortTopic) {
            rapidjson::Document topicRoot;
            topicRoot.Parse(payload.c_str());
            const std::string payloadGatewayId =
                (!topicRoot.HasParseError() && topicRoot.IsObject())
                    ? extractGatewayIdCompat(topicRoot)
                    : extractJsonStringValue(payload, "gatewayId");

            if (payloadGatewayId.empty() ||
                !isValidGatewayUpTopic(topic, payloadGatewayId, topicGatewayId)) {
                std::cout << "[MQTT RX] alarm_event ignored: invalid topic, topic="
                          << topic << std::endl;
                return;
            }
        }

        if (topicGatewayId.empty()) {
            std::cout << "[MQTT RX] alarm_event ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        AlarmEvent event;
        std::string reason;

        if (!parseAlarmEventPayload(payload, event, reason)) {
            std::cout << "[MQTT RX] alarm_event parse failed: " << reason << std::endl;
            return;
        }

        if ((!event.gatewayId.empty() && event.gatewayId != topicGatewayId) ||
            (hasPortTopic && !event.portId.empty() && event.portId != topicPortId)) {
            std::cout << "[MQTT RX] alarm_event ignored: topic payload mismatch, topicGateway="
                      << topicGatewayId
                      << ", topicPort=" << topicPortId
                      << ", payloadGateway=" << event.gatewayId
                      << ", payloadPort=" << event.portId << std::endl;
            return;
        }

        const bool dbOk = m_database.isOpen() && m_database.saveAlarmEvent(event);

        std::cout << "[MQTT RX] alarm_event " << (dbOk ? "ok" : "failed")
                  << ", alarmId: " << event.alarmId
                  << ", legacy: " << (messageType == "command") << std::endl;

        if (dbOk && m_ipc.hasClient()) {
            m_ipc.sendMessage(buildAlarmEventJson(event));
        } else if (!m_ipc.hasClient()) {
            std::cout << "[MQTT RX] Pc_ui not connected, alarm_event stored only" << std::endl;
        }

        return;
    }

    if (messageType == "gateway_register") {
        if (!isValidRegisterTopic(topic)) {
            std::cout << "[MQTT RX] gateway_register ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        rapidjson::Document root;
        root.Parse(payload.c_str());

        const std::int64_t seq = root.HasParseError() || !root.IsObject()
                                     ? 0
                                     : getJsonInt64Any(root, {"seq", "sequence"}, 0);

        const std::int64_t configVersion = root.HasParseError() || !root.IsObject()
                                               ? 0
                                               : getJsonInt64Any(root, {"configVersion", "config_version"}, 0);

        const std::string fallbackGatewayId = root.HasParseError() || !root.IsObject()
                                                  ? extractJsonStringValue(payload, "gatewayId")
                                                  : extractGatewayIdCompat(root);

        GatewayStatus gateway;
        GatewayRegistry registry;
        std::vector<GatewayPort> ports;

        if (!parseGatewayRegisterDetails(payload, gateway, registry, ports)) {
            std::cout << "[MQTT RX] gateway_register parse failed" << std::endl;
            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               false,
                               fallbackGatewayId,
                               std::string(),
                               configVersion,
                               fallbackGatewayId.empty() ? "missing_field" : "parse_error",
                               fallbackGatewayId.empty() ? "gateway_register missing gatewayId" : "gateway_register parse failed",
                               false);
            return;
        }

        bool ok = false;
        std::string reason;
        std::string ackMessage;

        if (!m_database.isOpen()) {
            reason = "db_error";
            ackMessage = "database is not open";
        } else {
            ok = m_database.upsertGatewayRegistry(registry) &&
                 m_database.upsertGatewayStatus(gateway);
            if (!ok) {
                reason = "db_error";
                ackMessage = "gateway_register database save failed";
            }
        }

        if (ok) {
            for (const GatewayPort& port : ports) {
                ok = m_database.upsertGatewayPort(port) && ok;
            }

            if (!ok) {
                reason = "db_error";
                ackMessage = "gateway_register port save failed";
            }
        }

        std::cout << "[GATEWAY REGISTER] gatewayId=" << gateway.gatewayId
                  << " upTopic=" << registry.upTopic
                  << " cmdTopic=" << registry.cmdTopic
                  << " saved=" << (ok ? "ok" : "failed") << std::endl;

        if (ok) {
            m_dataService.rememberGatewayRegistry(registry);
            m_dataService.rememberGatewayPorts(ports);
            m_dataService.updateGatewayStatus(gateway);
        }

        if (ok && m_ipc.hasClient()) {
            sendGatewayStatusSnapshot(m_ipc, m_dataService);
            sendPortStatusSnapshot(m_ipc, m_dataService);
            sendDevicesSnapshot(m_ipc, m_dataService);
        }

        publishRegisterAck(m_mqtt,
                           messageType,
                           seq,
                           ok,
                           gateway.gatewayId,
                           std::string(),
                           configVersion,
                           reason,
                           ackMessage,
                           true);
        return;
    }

    if (messageType == "gateway_heartbeat" || messageType == "gateway_status") {
        std::string gatewayId;
        std::int64_t timestampMs = 0;
        std::string status;

        if (messageType == "gateway_heartbeat") {
            if (!parseGatewayHeartbeat(payload, gatewayId, timestampMs, status)) {
                std::cout << "[MQTT RX] gateway_heartbeat parse failed" << std::endl;
                return;
            }
        } else {
            rapidjson::Document root;
            root.Parse(payload.c_str());
            if (root.HasParseError() || !root.IsObject()) {
                std::cout << "[MQTT RX] gateway_status parse failed" << std::endl;
                return;
            }

            gatewayId = extractGatewayIdCompat(root);
            timestampMs = getJsonInt64(root, "timestampMs", currentTimeMs());
            status = getJsonString(root, "status");
            if (status.empty()) {
                status = "online";
            }
            if (gatewayId.empty()) {
                std::cout << "[MQTT RX] gateway_status parse failed: missing gatewayId" << std::endl;
                return;
            }
        }

        std::string topicGatewayId;
        if (!isValidGatewayUpTopic(topic, gatewayId, topicGatewayId)) {
            std::cout << "[MQTT RX] " << messageType << " ignored: invalid topic, topic="
                      << topic
                      << ", payloadGateway=" << gatewayId << std::endl;
            return;
        }

        const bool ok = m_database.isOpen() &&
                        m_database.updateGatewayRegistryHeartbeat(gatewayId, timestampMs, status) &&
                        m_database.updateGatewayHeartbeat(gatewayId, timestampMs, status);

        if (ok) {
            GatewayStatus gateway;
            gateway.gatewayId = gatewayId;
            gateway.status = status;
            gateway.lastHeartbeatTimeMs = timestampMs;
            gateway.updateTimeMs = timestampMs;
            m_dataService.updateGatewayStatus(gateway);
        }

        std::cout << "[MQTT RX] " << messageType << " "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << gatewayId << std::endl;

        if (ok && m_ipc.hasClient()) {
            sendGatewayStatusSnapshot(m_ipc, m_dataService);
        }

        return;
    }

    if (messageType == "port_register") {
        if (!isValidRegisterTopic(topic)) {
            std::cout << "[MQTT RX] port_register ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        rapidjson::Document root;
        root.Parse(payload.c_str());

        const std::int64_t seq = root.HasParseError() || !root.IsObject()
                                     ? 0
                                     : getJsonInt64Any(root, {"seq", "sequence"}, 0);

        const std::int64_t configVersion = root.HasParseError() || !root.IsObject()
                                               ? 0
                                               : getJsonInt64Any(root, {"configVersion", "config_version"}, 0);

        const std::string fallbackGatewayId = root.HasParseError() || !root.IsObject()
                                                  ? extractJsonStringValue(payload, "gatewayId")
                                                  : extractGatewayIdCompat(root);

        const std::string fallbackPortId = root.HasParseError() || !root.IsObject()
                                               ? extractJsonStringValue(payload, "portId")
                                               : extractPortIdCompat(root);

        GatewayPort port;

        if (!parsePortRegister(payload, port)) {
            std::cout << "[MQTT RX] port_register parse failed" << std::endl;
            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               false,
                               fallbackGatewayId,
                               fallbackPortId,
                               configVersion,
                               fallbackGatewayId.empty() || fallbackPortId.empty() ? "missing_field" : "parse_error",
                               fallbackGatewayId.empty()
                                   ? "port_register missing gatewayId"
                                   : (fallbackPortId.empty() ? "port_register missing portId" : "port_register parse failed"),
                               false);
            return;
        }

        bool ok = false;
        std::string reason;
        std::string ackMessage;

        if (!m_database.isOpen()) {
            reason = "db_error";
            ackMessage = "database is not open";
        } else {
            ok = m_database.upsertGatewayPort(port);
            if (!ok) {
                reason = "db_error";
                ackMessage = "port_register database save failed";
            }
        }

        std::cout << "[MQTT RX] port_register "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << port.gatewayId
                  << ", port: " << port.portId
                  << ", status: " << port.status << std::endl;

        if (ok) {
            m_dataService.updateGatewayPort(port);
            if (m_ipc.hasClient()) {
                sendPortStatusSnapshot(m_ipc, m_dataService);
            }
        }

        publishRegisterAck(m_mqtt,
                           messageType,
                           seq,
                           ok,
                           port.gatewayId,
                           port.portId,
                           configVersion,
                           reason,
                           ackMessage,
                           true);
        return;
    }

    if (messageType == "port_status") {
        std::string topicGatewayId;
        std::string topicPortId;
        if (!parsePortUpTopic(topic, topicGatewayId, topicPortId)) {
            std::cout << "[MQTT RX] port_status ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        rapidjson::Document root;
        root.Parse(payload.c_str());

        if (root.HasParseError() || !root.IsObject()) {
            std::cout << "[MQTT RX] port_status parse failed" << std::endl;

            const std::string fallbackGatewayId = extractJsonStringValue(payload, "gatewayId");
            const std::string fallbackPortId = extractJsonStringValue(payload, "portId");

            publishRegisterAck(m_mqtt,
                               messageType,
                               0,
                               false,
                               fallbackGatewayId,
                               fallbackPortId,
                               0,
                               fallbackGatewayId.empty() ? "invalid_payload" : "parse_error",
                               "port_status parse failed",
                               false);
            return;
        }

        GatewayPort port;
        std::string reason;
        std::string ackMessage;

        bool ok = parsePortStatusDetails(root, port, reason, ackMessage);

        if (ok &&
            (port.gatewayId != topicGatewayId || port.portId != topicPortId)) {
            ok = false;
            reason = "topic_payload_mismatch";
            ackMessage = "port_status topic and payload mismatch";
        }

        if (ok) {
            if (!m_database.isOpen()) {
                ok = false;
                reason = "db_error";
                ackMessage = "database is not open";
            } else {
                ok = m_database.upsertGatewayPort(port);
                if (!ok) {
                    reason = "db_error";
                    ackMessage = "port_status database save failed";
                }
            }
        }

        std::cout << "[MQTT RX] port_status "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << port.gatewayId
                  << ", port: " << port.portId
                  << ", status: " << port.status
                  << ", reason: " << reason << std::endl;

        if (ok) {
            m_dataService.updateGatewayPort(port);
            if (m_ipc.hasClient()) {
                sendPortStatusSnapshot(m_ipc, m_dataService);
            }
        }

        publishRegisterAck(m_mqtt,
                           messageType,
                           getJsonInt64Any(root, {"seq", "sequence"}, 0),
                           ok,
                           port.gatewayId,
                           port.portId,
                           getJsonInt64Any(root, {"configVersion", "config_version"}, 0),
                           reason,
                           ackMessage,
                           true);
        return;
    }

    if (messageType == "ack" ||
        messageType == "command_ack" ||
        messageType == "command_result") {
        std::string topicGatewayId;
        std::string topicPortId;
        if (!parsePortUpTopic(topic, topicGatewayId, topicPortId)) {
            std::cout << "[MQTT RX ACK] ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        rapidjson::Document root;
        root.Parse(payload.c_str());

        if (root.HasParseError() || !root.IsObject()) {
            std::cout << "[MQTT RX] ack parse failed" << std::endl;
            return;
        }

        const std::int64_t seq = getJsonInt64Any(root, {"seq", "sequence"}, 0);
        const std::string ackCmdId = getJsonStringAny(root, {"cmd_id", "cmdId", "commandId"});
        std::string commandType = getJsonStringAny(root, {"cmd", "commandType", "command"});
        const std::string ackStage = getJsonString(root, "stage");
        const bool finalAck = ackStage == "done";
        const bool finalSuccess = isFinalBusinessSuccess(root);
        const std::string payloadGatewayId = extractGatewayIdCompat(root);
        const std::string payloadPortId = extractPortIdCompat(root);

        if ((!payloadGatewayId.empty() && payloadGatewayId != topicGatewayId) ||
            (!payloadPortId.empty() && payloadPortId != topicPortId)) {
            std::cout << "[MQTT RX ACK] ignored: topic payload mismatch, topicGateway="
                      << topicGatewayId
                      << ", topicPort=" << topicPortId
                      << ", payloadGateway=" << payloadGatewayId
                      << ", payloadPort=" << payloadPortId << std::endl;
            return;
        }

        std::string logStatus = finalSuccess
                                    ? "success"
                                    : (ackStage == "done" ? "failed" : "sent");

        std::string reason = getJsonString(root, "reason");
        std::string message = getJsonString(root, "message");

        if (!finalSuccess && ackStage != "done" && reason.empty()) {
            reason = "waiting_linux_data_ack";
        }

        if (ackCmdId.empty()) {
            std::cout << "[MQTT RX ACK][protocol_error] ignored: missing cmd_id, seq="
                      << seq << " cmd=" << commandType << std::endl;
            return;
        }

        PendingCommandTarget pendingTarget;
        const bool hasPendingTarget = finalAck
                                          ? m_dataService.takePendingCommand(ackCmdId, pendingTarget)
                                          : m_dataService.findPendingCommand(ackCmdId, pendingTarget);
        if (!hasPendingTarget) {
            std::cout << "[MQTT RX ACK] ignored: unmatched cmd_id=" << ackCmdId
                      << " seq=" << seq << std::endl;
            return;
        }

        std::cout << "[MQTT RX ACK] pendingHit=true"
                  << " timedOut=" << (pendingTarget.hardTimeoutNotified ? "true" : "false")
                  << " finalAckLate=" << (finalAck && pendingTarget.hardTimeoutNotified ? "true" : "false")
                  << " cmd_id=" << ackCmdId
                  << std::endl;

        std::cout << "[MQTT RX ACK] pending matched cmd_id=" << ackCmdId
                  << " command=" << (commandType.empty() ? pendingTarget.commandType : commandType)
                  << " stage=" << ackStage
                  << " status=" << logStatus
                  << " uiSeq=" << pendingTarget.uiSeq
                  << " boardSeq=" << pendingTarget.boardSeq
                  << std::endl;

        CommandLogTarget target;
        bool hasTarget = true;
        target.commandId = pendingTarget.commandId;
        target.seq = pendingTarget.uiSeq;
        target.commandType = pendingTarget.commandType;
        target.gatewayId = pendingTarget.gatewayId;
        target.portId = pendingTarget.portId;
        target.deviceId = pendingTarget.deviceId;

        if (commandType.empty() && hasTarget) {
            commandType = target.commandType;
        }

        const std::int64_t uiSeq = hasPendingTarget && pendingTarget.uiSeq > 0
                                       ? pendingTarget.uiSeq
                                       : (target.seq > 0 ? target.seq : seq);

        std::cout << "[MQTT RX ACK] gatewayId="
                  << (hasTarget ? target.gatewayId : getJsonStringAny(root, {"gatewayId", "gateway_id"}))
                  << " cmd=" << commandType
                  << " seq=" << seq
                  << " stage=" << ackStage
                  << " status=" << logStatus
                  << " reason=" << (reason.empty() ? std::string("ok") : reason)
                  << std::endl;

        std::cout << "[MQTT RX] ack seq=" << seq
                  << " cmd=" << commandType
                  << " stage=" << ackStage
                  << " status=" << logStatus
                  << " hasTarget=" << hasTarget
                  << " hasPendingTarget=" << hasPendingTarget
                  << " gatewayId=" << (hasTarget ? target.gatewayId : std::string())
                  << " portId=" << (hasTarget ? target.portId : std::string())
                  << " deviceId=" << (hasTarget ? target.deviceId : 0)
                  << std::endl;

        if (logStatus == "success" &&
            commandType == "remove_device" &&
            (!hasTarget || target.commandType != "remove_device")) {
            logStatus = "failed";
            reason = "device_not_found";
            if (message.empty()) {
                message = "remove_device ack target not found in command_log";
            }

            std::cout << "[MQTT RX] remove_device ack rejected: command_log target missing, seq="
                      << seq << std::endl;
        }

        if (m_ipc.hasClient() && finalAck && hasTarget) {
            const std::int64_t commandAckBoardSeq = pendingTarget.boardSeq > 0
                                                        ? pendingTarget.boardSeq
                                                        : uiSeq;
            const std::string commandAckJson =
                buildCommandAckJson(target.commandId,
                                    logStatus == "success",
                                    reason,
                                    uiSeq,
                                    commandType,
                                    "done",
                                    message,
                                    commandAckBoardSeq,
                                    target.gatewayId,
                                    target.portId,
                                    target.deviceId);
            const bool commandAckSendOk = m_ipc.sendMessage(commandAckJson);
            std::cout << "[IPC TX command_ack]"
                      << " cmd_id=" << target.commandId
                      << " stage=done"
                      << " status=" << (logStatus == "success" ? "success" : "failed")
                      << " ok=" << (logStatus == "success" ? "true" : "false")
                      << " bytes=" << commandAckJson.size()
                      << " cmd=" << commandType
                      << " seq=" << uiSeq
                      << " uiSeq=" << uiSeq
                      << " boardSeq=" << commandAckBoardSeq
                      << " gatewayId=" << target.gatewayId
                      << " portId=" << target.portId
                      << " deviceId=" << target.deviceId
                      << " sendOk=" << (commandAckSendOk ? "true" : "false")
                      << std::endl;

            if (logStatus == "success" &&
                commandType == "add_device" &&
                target.commandType == "add_device") {
                sendDevicesSnapshotWithPendingDevice(m_ipc, m_database, pendingTarget);
            }
        }

        if (logStatus == "success" &&
            commandType == "remove_device" &&
            hasTarget &&
            target.commandType == "remove_device") {
            const bool deleteOk = m_database.deleteDeviceData(target.gatewayId,
                                                              target.portId,
                                                              target.deviceId);

            bool snapshotRemoved = false;
            if (deleteOk) {
                snapshotRemoved = m_dataService.removeDeviceData(target.gatewayId,
                                                                 target.portId,
                                                                 target.deviceId);
            }

            if (deleteOk && m_ipc.hasClient()) {
                sendLatestPoints(m_ipc, m_dataService);
                sendDevicesSnapshot(m_ipc, m_dataService);
                sendPortStatusSnapshot(m_ipc, m_dataService);
                sendGatewayStatusSnapshot(m_ipc, m_dataService);
            } else if (!deleteOk) {
                logStatus = "failed";
                reason = "delete_device_data_failed";
                if (message.empty()) {
                    message = "gateway removed device, but Pc_data database delete failed";
                }
            }

            std::cout << "[MQTT RX] remove_device db sync "
                      << (deleteOk ? "ok" : "failed")
                      << ", gateway: " << target.gatewayId
                      << ", port: " << target.portId
                      << ", device: " << target.deviceId
                      << ", dbOk: " << deleteOk
                      << ", snapshotRemoved: " << snapshotRemoved
                      << std::endl;
        }

        if (logStatus == "success" &&
            commandType == "add_device" &&
            !hasTarget &&
            m_database.isOpen()) {
            DeviceRecord device = deviceRecordFromAddDeviceAck(root);

            if (device.deviceId > 0 && !device.deviceType.empty()) {
                publishConfigSnapshotRequest(m_mqtt,
                                             m_database,
                                             device.gatewayId,
                                             device.portId,
                                             "add_device_ack_without_command_log");

                std::cout << "[MQTT RX] add_device ack without command log, wait config_snapshot"
                          << ", gateway: " << device.gatewayId
                          << ", port: " << device.portId
                          << ", device: " << device.deviceId
                          << std::endl;
            }
        }

        if (m_database.isOpen()) {
            m_database.updateCommandLogByCommandId(pendingTarget.commandId,
                                                   logStatus,
                                                   reason,
                                                   message,
                                                   currentTimeMs());
        }

        if (logStatus == "success" &&
            commandType == "add_device" &&
            hasTarget &&
            target.commandType == "add_device") {
            if (m_database.isOpen()) {
                m_database.reactivateDeletedDevice(target.gatewayId,
                                                   target.portId,
                                                   target.deviceId);
            }

            m_dataService.forgetRemovedDevice(target.gatewayId,
                                              target.portId,
                                              target.deviceId);

            publishConfigSnapshotRequest(m_mqtt,
                                         m_database,
                                         target.gatewayId,
                                         target.portId,
                                         "add_device_success");

            if (m_database.isOpen() && m_ipc.hasClient()) {
                sendDevicesSnapshot(m_ipc, m_dataService);
                sendLatestPoints(m_ipc, m_dataService);
                sendPortStatusSnapshot(m_ipc, m_dataService);
            }
        }

        if (m_ipc.hasClient()) {
            m_ipc.sendMessage(buildCommandLogUpdateJson(uiSeq,
                                                        commandType,
                                                        logStatus,
                                                        reason,
                                                        message,
                                                        hasTarget ? &target : nullptr,
                                                        pendingTarget.boardSeq));
        }

        std::cout << "[MQTT RX] ack seq: " << seq
                  << ", command: " << commandType
                  << ", status: " << logStatus << std::endl;

        return;
    }

    if (messageType == "config_snapshot" || messageType == "device_config_snapshot") {
        if (!isValidRegisterTopic(topic)) {
            std::cout << "[MQTT RX] config_snapshot ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        rapidjson::Document root;
        root.Parse(payload.c_str());

        if (root.HasParseError() || !root.IsObject()) {
            std::cout << "[MQTT RX] config_snapshot parse failed" << std::endl;

            const std::string fallbackGatewayId = extractJsonStringValue(payload, "gatewayId");

            publishRegisterAck(m_mqtt,
                               messageType,
                               0,
                               false,
                               fallbackGatewayId,
                               std::string(),
                               0,
                               fallbackGatewayId.empty() ? "invalid_payload" : "parse_error",
                               "config_snapshot parse failed",
                               false);
            return;
        }

        const std::int64_t seq = getJsonInt64(root, "seq", 0);
        const std::int64_t configVersion = getJsonInt64Any(root, {"configVersion", "config_version"}, 0);
        const bool hasSnapshotMode = root.HasMember("fullSnapshot") || root.HasMember("full_snapshot");

        if (configVersion <= 0 && !hasSnapshotMode) {
            std::string gatewayId = extractGatewayIdCompat(root);

            std::cout << "[MQTT RX] config_snapshot ignored: missing configVersion and fullSnapshot marker, seq="
                      << seq << ", gateway=" << gatewayId << std::endl;

            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               false,
                               gatewayId,
                               std::string(),
                               configVersion,
                               "missing_snapshot_version",
                               "config snapshot missing configVersion/fullSnapshot",
                               false);
            return;
        }

        SyncGatewayPending pending;
        const bool hasPending = m_dataService.findSyncPending(seq, pending);

        if (!hasPending) {
            std::string gatewayId = extractGatewayIdCompat(root);
            if (seq > 0) {
                std::cout << "[MQTT RX] config_snapshot accepted without pending sync request, seq="
                          << seq << ", gateway=" << gatewayId << std::endl;
            }
            pending.gatewayId = gatewayId;
        }

        if (pending.gatewayId.empty()) {
            std::cout << "[MQTT RX] config_snapshot ignored: empty gatewayId, seq=" << seq << std::endl;

            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               false,
                               pending.gatewayId,
                               std::string(),
                               configVersion,
                               "missing_field",
                               "config snapshot missing gatewayId",
                               false);
            return;
        }

        if (hasPending && pending.devices.empty()) {
            std::cout << "[MQTT RX] config_snapshot ignored: pending devices empty, seq="
                      << seq << ", gateway=" << pending.gatewayId << std::endl;

            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               false,
                               pending.gatewayId,
                               std::string(),
                               configVersion,
                               "invalid_payload",
                               "config snapshot pending devices empty",
                               false);
            return;
        }

        std::vector<GatewayPort> ports;
        std::vector<ConfigSnapshotDevice> devices;
        std::vector<PointConfig> pointConfigs;
        int portCount = 0;
        int deviceCount = 0;

        bool ok = parseConfigSnapshot(payload,
                                      pending,
                                      ports,
                                      devices,
                                      pointConfigs,
                                      portCount,
                                      deviceCount);

        const bool fullSnapshot = !hasPending &&
                                  getJsonBool(root, "fullSnapshot", getJsonBool(root, "full_snapshot", false));

        std::vector<DeviceRecord> devicesBeforeFullSnapshot;

        if (ok && fullSnapshot && m_database.isOpen()) {
            devicesBeforeFullSnapshot = m_database.queryDevices();
        }

        std::string reason;
        std::string ackMessage;

        if (!ok) {
            reason = "parse_error";
            ackMessage = "config_snapshot parse failed";
        }

        if (ok && m_database.isOpen()) {
            if (hasPending) {
                std::vector<DbSelectedDevice> selectedDevices;

                for (const SyncSelectedDevice& selected : pending.devices) {
                    DbSelectedDevice dbSelected;
                    dbSelected.portId = selected.portId;
                    dbSelected.deviceId = selected.deviceId;
                    selectedDevices.push_back(dbSelected);
                }

                ok = m_database.replaceSelectedDeviceConfig(pending.gatewayId,
                                                            selectedDevices,
                                                            ports,
                                                            devices,
                                                            pointConfigs);
            } else {
                ok = m_database.upsertGatewayConfigSnapshot(pending.gatewayId,
                                                            ports,
                                                            devices,
                                                            pointConfigs,
                                                            fullSnapshot);
            }

            if (ok) {
                if (fullSnapshot) {
                    std::set<std::pair<std::string, int> > snapshotDevices;

                    for (const ConfigSnapshotDevice& snapshotDevice : devices) {
                        const DeviceRecord& device = snapshotDevice.device;
                        if (device.gatewayId == pending.gatewayId &&
                            !device.portId.empty() &&
                            device.deviceId > 0) {
                            snapshotDevices.insert(std::make_pair(device.portId, device.deviceId));
                        }
                    }

                    for (const DeviceRecord& oldDevice : devicesBeforeFullSnapshot) {
                        if (oldDevice.gatewayId != pending.gatewayId ||
                            oldDevice.portId.empty() ||
                            oldDevice.deviceId <= 0 ||
                            snapshotDevices.find(std::make_pair(oldDevice.portId, oldDevice.deviceId)) != snapshotDevices.end()) {
                            continue;
                        }

                        m_dataService.removeDeviceData(oldDevice.gatewayId,
                                                       oldDevice.portId,
                                                       oldDevice.deviceId);
                    }
                }

                if (hasPending) {
                    for (const ConfigSnapshotDevice& snapshotDevice : devices) {
                        m_dataService.forgetRemovedDevice(snapshotDevice.device.gatewayId,
                                                          snapshotDevice.device.portId,
                                                          snapshotDevice.device.deviceId);
                    }
                }
            }
        } else if (ok) {
            ok = false;
            reason = "db_error";
            ackMessage = "database is not open";

            std::cout << "[MQTT RX] config_snapshot save failed: database not open, seq="
                      << seq << ", gateway=" << pending.gatewayId << std::endl;
        }

        if (!ok && reason.empty()) {
            reason = "db_error";
            ackMessage = "config_snapshot database save failed";
        }

        if (ok) {
            std::vector<DeviceRecord> snapshotDeviceRecords;
            snapshotDeviceRecords.reserve(devices.size());
            for (const ConfigSnapshotDevice& snapshotDevice : devices) {
                snapshotDeviceRecords.push_back(snapshotDevice.device);
            }
            m_dataService.replaceGatewayRegistry(pending.gatewayId,
                                                 ports,
                                                 snapshotDeviceRecords,
                                                 pointConfigs,
                                                 fullSnapshot);
        }

        if (!hasPending) {
            std::cout << "[MQTT RX SNAPSHOT] gatewayId=" << pending.gatewayId
                      << " ports=" << portCount
                      << " devices=" << deviceCount
                      << " fullSnapshot="
                      << (getJsonBool(root, "fullSnapshot", getJsonBool(root, "full_snapshot", false)) ? "true" : "false")
                      << std::endl;

            std::cout << "[MQTT RX] unsolicited config_snapshot "
                      << (ok ? "saved" : "save failed")
                      << ", gateway: " << pending.gatewayId
                      << ", ports: " << portCount
                      << ", devices: " << deviceCount << std::endl;

            if (ok) {
                const bool hasClient = m_ipc.hasClient();
                const int dbDevices = m_database.isOpen()
                                          ? static_cast<int>(m_database.queryDevices().size())
                                          : 0;
                std::cout << "unsolicited config_snapshot saved gateway=" << pending.gatewayId
                          << " snapshotDevices=" << deviceCount
                          << " dbDevices=" << dbDevices
                          << " ipcClient=" << (hasClient ? "true" : "false")
                          << std::endl;

                if (hasClient) {
                    sendGatewayStatusSnapshot(m_ipc, m_dataService);
                    sendPortStatusSnapshot(m_ipc, m_dataService);
                    sendDevicesSnapshot(m_ipc, m_dataService);
                    sendLatestPoints(m_ipc, m_dataService);
                } else {
                    std::cout << "skip snapshot publish: no Pc_ui client"
                              << ", gateway=" << pending.gatewayId
                              << ", snapshotDevices=" << deviceCount
                              << ", dbDevices=" << dbDevices
                              << std::endl;
                }
            }

            publishRegisterAck(m_mqtt,
                               messageType,
                               seq,
                               ok,
                               pending.gatewayId,
                               std::string(),
                               configVersion,
                               reason,
                               ackMessage,
                               true,
                               ok ? deviceCount : -1,
                               ok ? static_cast<int>(pointConfigs.size()) : -1);
            return;
        }

        SyncConfigResult result;

        if (m_dataService.completeSyncConfig(seq,
                                             ok,
                                             ok ? "config sync success" : "config snapshot save failed",
                                             ok ? portCount : 0,
                                             ok ? deviceCount : 0,
                                             result)) {
            m_ipc.sendMessage(buildSyncConfigResultJson(result.success,
                                                        result.message,
                                                        result.portCount,
                                                        result.deviceCount));

            if (ok && m_ipc.hasClient()) {
                sendGatewayStatusSnapshot(m_ipc, m_dataService);
                sendPortStatusSnapshot(m_ipc, m_dataService);
                sendDevicesSnapshot(m_ipc, m_dataService);
                sendLatestPoints(m_ipc, m_dataService);
            }
        }

        std::cout << "[MQTT RX] config_snapshot seq: " << seq
                  << ", ok: " << ok
                  << ", ports: " << portCount
                  << ", devices: " << deviceCount
                  << std::endl;

        std::cout << "[MQTT RX SNAPSHOT] gatewayId=" << pending.gatewayId
                  << " ports=" << portCount
                  << " devices=" << deviceCount
                  << " fullSnapshot="
                  << (getJsonBool(root, "fullSnapshot", getJsonBool(root, "full_snapshot", false)) ? "true" : "false")
                  << std::endl;

        publishRegisterAck(m_mqtt,
                           messageType,
                           seq,
                           ok,
                           pending.gatewayId,
                           std::string(),
                           configVersion,
                           reason,
                           ackMessage,
                           true,
                           ok ? deviceCount : -1,
                           ok ? static_cast<int>(pointConfigs.size()) : -1);
        return;
    }

    if (messageType == "device_register") {
        if (!isValidRegisterTopic(topic)) {
            std::cout << "[MQTT RX] device_register ignored: invalid topic, topic="
                      << topic << std::endl;
            return;
        }

        DeviceRecord device;
        std::uint32_t sequence = 0;
        std::string reason;

        rapidjson::Document root;
        root.Parse(payload.c_str());

        const bool rootOk = !root.HasParseError() && root.IsObject();

        const std::int64_t fallbackSeq = rootOk
                                             ? getJsonInt64Any(root, {"seq", "sequence"}, 0)
                                             : 0;

        const std::string fallbackGatewayId = rootOk
                                                  ? extractGatewayIdCompat(root)
                                                  : extractJsonStringValue(payload, "gatewayId");

        const std::string fallbackPortId = rootOk
                                               ? extractPortIdCompat(root)
                                               : extractJsonStringValue(payload, "portId");

        const int fallbackDeviceId = rootOk
                                         ? getJsonIntAny(root, {"deviceId", "device_id", "slave_id", "slaveAddress"}, 0)
                                         : 0;

        if (!parseDeviceRegister(payload, device, sequence, reason)) {
            std::cout << "[MQTT RX] device_register parse failed" << std::endl;

            publishRegisterAck(m_mqtt,
                               messageType,
                               fallbackSeq,
                               false,
                               fallbackGatewayId,
                               fallbackPortId,
                               0,
                               fallbackGatewayId.empty() || fallbackPortId.empty() ? "missing_field" : "parse_error",
                               fallbackGatewayId.empty()
                                   ? "device_register missing gatewayId"
                                   : (fallbackPortId.empty() ? "device_register missing portId" : "device_register parse failed"),
                               false,
                               -1,
                               -1,
                               fallbackDeviceId);
            return;
        }

        bool ok = reason.empty();

        if (ok) {
            if (m_database.isOpen()) {
                if (m_dataService.isRemovedDevice(device.gatewayId, device.portId, device.deviceId) ||
                    m_database.deviceDeleted(device.gatewayId, device.portId, device.deviceId)) {
                    ok = false;
                    reason = "device_removed_wait_config_snapshot";

                    std::cout << "[MQTT RX] device_register rejected for removed device, wait config_snapshot, gateway: "
                              << device.gatewayId
                              << ", port: " << device.portId
                              << ", device: " << device.deviceId << std::endl;
                } else {
                    ok = m_database.upsertDevice(device);
                    if (!ok) {
                        reason = "device_db_save_failed";
                    }
                }
            } else {
                ok = false;
                reason = "database_not_open";
            }
        }

        const std::string ackMessage = ok ? std::string() :
                                           (reason.empty() ? std::string("device_register failed") : reason);

        const bool retryable = reason != "device_removed_wait_config_snapshot";

        const bool publishOk = publishRegisterAck(m_mqtt,
                                                  messageType,
                                                  sequence,
                                                  ok,
                                                  device.gatewayId,
                                                  device.portId,
                                                  0,
                                                  reason,
                                                  ackMessage,
                                                  retryable,
                                                  ok ? 1 : -1,
                                                  -1,
                                                  device.deviceId);

        std::cout << "[MQTT RX] device_register "
                  << (ok ? "ok" : "failed")
                  << ", ack publish: " << (publishOk ? "ok" : "failed")
                  << ", gateway: " << device.gatewayId
                  << ", port: " << device.portId
                  << ", device: " << device.deviceId
                  << std::endl;

        if (ok) {
            m_dataService.updateDeviceRegistry(device);
        }

        if (m_ipc.hasClient()) {
            sendDevicesSnapshot(m_ipc, m_dataService);
            sendPortStatusSnapshot(m_ipc, m_dataService);
            sendGatewayStatusSnapshot(m_ipc, m_dataService);
            sendLatestPoints(m_ipc, m_dataService);
        }

        return;
    }

    if (messageType == "threshold_config") {
        std::vector<PointConfig> configs;
        std::string errorMessage;

        if (!PointConfigPackParser::parseJson(payload, configs, errorMessage)) {
            std::cout << "[MQTT RX] threshold_config parse failed: "
                      << errorMessage
                      << std::endl;
            return;
        }

        std::cout << "[MQTT RX] threshold_config parse ok, config count: "
                  << configs.size()
                  << std::endl;

        if (m_database.isOpen()) {
            std::vector<PointConfig> acceptedConfigs;
            acceptedConfigs.reserve(configs.size());

            for (const PointConfig& config : configs) {
                if (!m_database.deviceExists(config.gatewayId,
                                             config.portId,
                                             config.deviceId)) {
                    std::cout << "[MQTT RX] threshold_config ignored for unknown device, gateway: "
                              << config.gatewayId
                              << ", port: " << config.portId
                              << ", device: " << config.deviceId
                              << ", pointKey: " << config.pointKey << std::endl;
                    continue;
                }

                const std::string deviceType =
                    m_database.queryDeviceType(config.gatewayId,
                                               config.portId,
                                               config.deviceId);

                PointConfig acceptedConfig = config;
                acceptedConfig.pointKey = normalizeTelemetryPointKey(config.pointKey);

                if (acceptedConfig.pointKey != config.pointKey &&
                    !acceptedConfig.pointId.empty()) {
                    acceptedConfig.pointId =
                        ModelConverter::buildPointId(acceptedConfig.factoryId,
                                                     acceptedConfig.areaId,
                                                     acceptedConfig.gatewayId,
                                                     acceptedConfig.portId,
                                                     acceptedConfig.deviceId,
                                                     acceptedConfig.pointKey);
                }

                if (!m_database.pointConfigExists(config.gatewayId,
                                                  config.portId,
                                                  config.deviceId,
                                                  acceptedConfig.pointKey) &&
                    !isDefaultTelemetryPointKey(deviceType.empty() ? config.deviceType : deviceType,
                                                acceptedConfig.pointKey)) {
                    std::cout << "[MQTT RX] threshold_config ignored for unknown point, gateway: "
                              << config.gatewayId
                              << ", port: " << config.portId
                              << ", device: " << config.deviceId
                              << ", pointKey: " << config.pointKey << std::endl;
                    continue;
                }

                acceptedConfigs.push_back(acceptedConfig);
            }

            if (m_database.savePointConfigs(acceptedConfigs)) {
                for (const PointConfig& config : acceptedConfigs) {
                    m_dataService.updatePointConfig(config);
                }
            }

            std::cout << "[MQTT RX] threshold_config accepted config count: "
                      << acceptedConfigs.size()
                      << std::endl;
        } else {
            std::cout << "[MQTT RX] database is not open, skip point_config storage" << std::endl;
        }

        return;
    }

    std::cout << "[MQTT RX] skip unsupported message type: "
              << messageType
              << std::endl;
}

void MqttMessageHandler::enqueueTelemetry(const std::string& topic, const std::string& payload)
{
    {
        std::lock_guard<std::mutex> lock(m_telemetryMutex);
        if (m_telemetryQueue.size() >= kMaxTelemetryQueueSize) {
            m_telemetryQueue.pop_front();
            std::cout << "[DBG_TELEMETRY] telemetry queue full, drop oldest pending packet"
                      << " queueLimit=" << kMaxTelemetryQueueSize << std::endl;
        }
        m_telemetryQueue.push_back(TelemetryMessage{topic, payload});
    }
    m_telemetryCv.notify_one();
}

void MqttMessageHandler::telemetryWorkerLoop()
{
    while (true) {
        TelemetryMessage message;
        {
            std::unique_lock<std::mutex> lock(m_telemetryMutex);
            m_telemetryCv.wait(lock, [this]() {
                return m_stopTelemetryWorker || !m_telemetryQueue.empty();
            });

            if (m_stopTelemetryWorker && m_telemetryQueue.empty()) {
                break;
            }

            message = std::move(m_telemetryQueue.front());
            m_telemetryQueue.pop_front();
        }

        handleTelemetryMessage(message.topic, message.payload);
    }
}

void MqttMessageHandler::enqueueTelemetryStorage(const std::vector<TelemetryPoint>& points)
{
    if (points.empty()) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_dbWriterMutex);
        for (const TelemetryPoint& point : points) {
            if (point.pointId.empty()) {
                continue;
            }
            m_pendingDbPoints[point.pointId] = point;
        }
    }
    m_dbWriterCv.notify_one();
}

void MqttMessageHandler::dbWriterLoop()
{
    while (true) {
        std::vector<TelemetryPoint> points;
        {
            std::unique_lock<std::mutex> lock(m_dbWriterMutex);
            m_dbWriterCv.wait(lock, [this]() {
                return m_stopDbWriter || !m_pendingDbPoints.empty();
            });

            if (m_stopDbWriter && m_pendingDbPoints.empty()) {
                break;
            }

            points.reserve(m_pendingDbPoints.size());
            for (const auto& item : m_pendingDbPoints) {
                points.push_back(item.second);
            }
            m_pendingDbPoints.clear();
        }

        if (!points.empty() && m_database.isOpen()) {
            const std::int64_t beginMs = currentTimeMs();
            const bool ok = m_database.saveTelemetryPoints(points);
            const std::int64_t costMs = currentTimeMs() - beginMs;
            std::cout << "[DBG_TELEMETRY_DB] async save ok="
                      << (ok ? "true" : "false")
                      << " pointCount=" << points.size()
                      << " costMs=" << costMs << std::endl;
        }
    }
}

void MqttMessageHandler::markLatestPointsDirty()
{
    {
        std::lock_guard<std::mutex> lock(m_latestPublisherMutex);
        m_latestPointsDirty = true;
    }
    m_latestPublisherCv.notify_one();
}

void MqttMessageHandler::latestPublisherLoop()
{
    while (true) {
        {
            std::unique_lock<std::mutex> lock(m_latestPublisherMutex);
            m_latestPublisherCv.wait(lock, [this]() {
                return m_stopLatestPublisher || m_latestPointsDirty;
            });

            if (m_stopLatestPublisher) {
                break;
            }

            lock.unlock();
            std::this_thread::sleep_for(kLatestPublishInterval);
            lock.lock();

            if (m_stopLatestPublisher) {
                break;
            }
            if (!m_latestPointsDirty) {
                continue;
            }
            m_latestPointsDirty = false;
        }

        if (!m_ipc.hasClient()) {
            std::cout << "[MQTT RX] Pc_ui not connected, latest_points publish coalesced but skipped"
                      << std::endl;
            continue;
        }

        const std::vector<TelemetryPoint> points = m_dataService.getLatestPoints();
        const std::string json = buildLatestPointsJson(points);
        const std::int64_t beginMs = currentTimeMs();
        const bool sendOk = m_ipc.sendMessage(json);
        const std::int64_t costMs = currentTimeMs() - beginMs;
        std::cout << "[DBG_IPC_SEND] latest_points coalesced send ok="
                  << (sendOk ? "true" : "false")
                  << " pointCount=" << points.size()
                  << " jsonBytes=" << json.size()
                  << " costMs=" << costMs << std::endl;
        if (!sendOk) {
            markLatestPointsDirty();
        }
    }
}

void MqttMessageHandler::handleTelemetryMessage(const std::string& topic, const std::string& payload)
{
    std::cout << "[DBG_TELEMETRY] telemetry branch entry type="
              << "telemetry_pack topic=" << topic
              << " payloadBytes=" << payload.size()
              << std::endl;

    TelemetryPack pack;
    std::string errorMessage;

    if (!TelemetryPackParser::parseJson(payload, pack, errorMessage)) {
        std::cout << "[DBG_TELEMETRY] parseOk=false reason="
                  << errorMessage
                  << std::endl;
        std::cout << "[MQTT RX] parse failed: " << errorMessage << std::endl;
        return;
    }

    std::cout << "[DBG_TELEMETRY] parseOk=true seq=" << pack.sequence
              << " gatewayId=" << pack.site.gatewayId
              << " portId=" << pack.site.portId
              << " rawDeviceCount=" << pack.devices.size()
              << std::endl;

    std::string topicGatewayId;
    std::string topicPortId;

    if (!parsePortUpTopic(topic, topicGatewayId, topicPortId)) {
        std::cout << "[MQTT RX] telemetry_pack ignored: invalid topic, topic="
                  << topic << std::endl;
        return;
    }

    if (pack.site.gatewayId.empty() ||
        pack.site.portId.empty() ||
        pack.site.gatewayId != topicGatewayId ||
        pack.site.portId != topicPortId) {
        std::cout << "[MQTT RX] telemetry_pack ignored: topic payload mismatch, topicGateway="
                  << topicGatewayId
                  << ", topicPort=" << topicPortId
                  << ", payloadGateway=" << pack.site.gatewayId
                  << ", payloadPort=" << pack.site.portId << std::endl;
        return;
    }

    if (!m_dataService.gatewayRegistered(pack.site.gatewayId)) {
        std::cout << "[MQTT RX] telemetry_pack ignored: gateway not registered, gateway="
                  << pack.site.gatewayId << std::endl;
        return;
    }

    if (!m_dataService.portRegistered(pack.site.gatewayId, pack.site.portId)) {
        std::cout << "[MQTT RX] telemetry_pack ignored: port not registered, gateway="
                  << pack.site.gatewayId
                  << ", port=" << pack.site.portId << std::endl;
        return;
    }

    TelemetryPack filteredPack = pack;
    filteredPack.devices.clear();

    std::unordered_map<std::string, std::string> registeredDeviceTypes;
    std::unordered_map<std::string, std::set<std::string> > telemetryPointKeys;

    for (const DeviceData& device : pack.devices) {
        if (pack.site.gatewayId.empty() ||
            pack.site.portId.empty() ||
            device.deviceId <= 0) {
            std::cout << "[MQTT RX] unknown telemetry ignored, gateway: "
                      << pack.site.gatewayId
                      << ", port: " << pack.site.portId
                      << ", device: " << device.deviceId
                      << ", reason=missing_identity" << std::endl;
            continue;
        }

        if (m_dataService.isRemovedDevice(pack.site.gatewayId,
                                          pack.site.portId,
                                          device.deviceId)) {
            std::cout << "[MQTT RX] deleted device telemetry ignored, gateway: "
                      << pack.site.gatewayId
                      << ", port: " << pack.site.portId
                      << ", device: " << device.deviceId << std::endl;
            continue;
        }

        if (!m_dataService.deviceRegistered(pack.site.gatewayId,
                                            pack.site.portId,
                                            device.deviceId)) {
            std::cout << "[MQTT RX] unknown telemetry ignored, gateway: "
                      << pack.site.gatewayId
                      << ", port: " << pack.site.portId
                      << ", device: " << device.deviceId << std::endl;

            const std::string requestKey = pack.site.gatewayId + "/" + pack.site.portId;
            const std::int64_t nowMs = currentTimeMs();
            const auto lastIt = m_lastConfigRequestMs.find(requestKey);

            if (lastIt == m_lastConfigRequestMs.end() ||
                nowMs - lastIt->second >= kUnknownTelemetryConfigRequestIntervalMs) {
                m_lastConfigRequestMs[requestKey] = nowMs;
                publishConfigSnapshotRequest(m_mqtt,
                                             m_database,
                                             pack.site.gatewayId,
                                             pack.site.portId,
                                             "unknown_telemetry");

                std::cout << "[MQTT RX] unknown telemetry dropped after config request, gateway: "
                          << pack.site.gatewayId
                          << ", port: " << pack.site.portId
                          << ", device: " << device.deviceId << std::endl;
            } else {
                std::cout << "[MQTT RX] unknown telemetry drop log throttled, gateway: "
                          << pack.site.gatewayId
                          << ", port: " << pack.site.portId
                          << ", remain_ms: "
                          << (kUnknownTelemetryConfigRequestIntervalMs - (nowMs - lastIt->second))
                          << std::endl;
            }

            continue;
        }

        const std::string registeredType =
            m_dataService.registeredDeviceType(pack.site.gatewayId,
                                               pack.site.portId,
                                               device.deviceId);

        const std::string telemetryType = deviceTypeToString(device.type);

        if (!deviceTypesMatch(registeredType, telemetryType)) {
            std::cout << "[MQTT RX] telemetry device type mismatch ignored, gateway: "
                      << pack.site.gatewayId
                      << ", port: " << pack.site.portId
                      << ", device: " << device.deviceId
                      << ", registeredType: " << registeredType
                      << ", telemetryType: " << telemetryType << std::endl;
            continue;
        }

        const std::string deviceKey =
            telemetryDeviceKey(pack.site.portId, device.deviceId);

        registeredDeviceTypes[deviceKey] =
            registeredType.empty() ? telemetryType : registeredType;

        TelemetryPack singleDevicePack = pack;
        singleDevicePack.devices.clear();
        singleDevicePack.devices.push_back(device);

        const std::vector<TelemetryPoint> devicePoints =
            ModelConverter::toTelemetryPoints(singleDevicePack);

        for (const TelemetryPoint& point : devicePoints) {
            const std::string normalizedPointKey =
                normalizeTelemetryPointKey(point.pointKey);

            telemetryPointKeys[deviceKey].insert(normalizedPointKey);

            if (!m_dataService.pointConfigRegistered(pack.site.gatewayId,
                                                     pack.site.portId,
                                                     device.deviceId,
                                                     normalizedPointKey) &&
                !isDefaultTelemetryPointKey(registeredDeviceTypes[deviceKey],
                                            normalizedPointKey)) {
                std::cout << "[MQTT RX] unknown telemetry point ignored, gateway: "
                          << pack.site.gatewayId
                          << ", port: " << pack.site.portId
                          << ", device: " << device.deviceId
                          << ", pointKey: " << point.pointKey << std::endl;
            }
        }

        filteredPack.devices.push_back(device);
    }

    std::vector<TelemetryPoint> receivedPoints =
        ModelConverter::toTelemetryPoints(filteredPack);

    std::cout << "[DBG_TELEMETRY] convertedPoints count="
              << receivedPoints.size()
              << " seq=" << pack.sequence
              << std::endl;

    const size_t receivedBeforeFilter = receivedPoints.size();

    receivedPoints.erase(
        std::remove_if(receivedPoints.begin(),
                       receivedPoints.end(),
                       [this,
                        &registeredDeviceTypes,
                        &telemetryPointKeys](const TelemetryPoint& point) {
                           const std::string deviceKey =
                               telemetryDeviceKey(point.portId, point.deviceId);

                           const auto typeIt = registeredDeviceTypes.find(deviceKey);

                           const std::string deviceType =
                               typeIt == registeredDeviceTypes.end()
                                   ? point.deviceType
                                   : typeIt->second;

                           const std::string normalizedPointKey =
                               normalizeTelemetryPointKey(point.pointKey);

                           const auto rawKeysIt = telemetryPointKeys.find(deviceKey);

                           if (rawKeysIt != telemetryPointKeys.end() &&
                               !rawKeysIt->second.empty() &&
                               rawKeysIt->second.find(normalizedPointKey) == rawKeysIt->second.end()) {
                               return true;
                           }

                           if (m_dataService.pointConfigRegistered(point.gatewayId,
                                                                   point.portId,
                                                                   point.deviceId,
                                                                   normalizedPointKey)) {
                               return false;
                           }

                           if (isDefaultTelemetryPointKey(deviceType, normalizedPointKey)) {
                               return false;
                           }

                           std::cout << "[MQTT RX] unknown telemetry point ignored, gateway: "
                                     << point.gatewayId
                                     << ", port: " << point.portId
                                     << ", device: " << point.deviceId
                                     << ", pointKey: " << point.pointKey << std::endl;

                           return true;
                       }),
        receivedPoints.end());

    std::cout << "[DBG_TELEMETRY] filteredPoints before="
              << receivedBeforeFilter
              << " after=" << receivedPoints.size()
              << " filtered=" << (receivedBeforeFilter - receivedPoints.size())
              << " seq=" << pack.sequence
              << std::endl;

    std::cout << "[MQTT RX TELEMETRY] gatewayId=" << pack.site.gatewayId
              << " portId=" << pack.site.portId
              << " devices=" << pack.devices.size()
              << " points=" << receivedPoints.size()
              << " seq=" << pack.sequence << std::endl;

    m_dataService.handleTelemetryPoints(receivedPoints);

    std::vector<TelemetryPoint> points = m_dataService.getLatestPoints();

    enqueueTelemetryStorage(receivedPoints);
    markLatestPointsDirty();

    std::cout << "[DBG_TELEMETRY] latest_points dirty pointCount="
              << points.size()
              << " seq=" << pack.sequence
              << std::endl;
}
