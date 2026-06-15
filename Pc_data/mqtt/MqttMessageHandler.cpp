#include "MqttMessageHandler.hpp"

#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
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
const char* kDeviceRegisterAckTopic = "imx6ull/device/data";

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
    if (config.factoryId.empty() || config.areaId.empty() || config.gatewayId.empty() ||
        config.portId.empty() || config.deviceId <= 0 || config.pointKey.empty()) {
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
        << ",\"message\":\"" << jsonEscape(event.message) << "\"}"
        ;
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
    if (root.HasParseError() || !root.IsObject() ||
        getJsonString(root, "type") != "config_snapshot") {
        return false;
    }

    const rapidjson::Value& site = root.HasMember("site") && root["site"].IsObject() ? root["site"] : root;
    std::string gatewayId = getJsonString(root, "gatewayId");
    if (gatewayId.empty()) {
        gatewayId = getJsonString(site, "gatewayId");
    }
    const std::string factoryId = getJsonString(site, "factoryId");
    const std::string factoryName = getJsonString(site, "factoryName");
    const std::string areaId = getJsonString(site, "areaId");
    const std::string areaName = getJsonString(site, "areaName");
    const std::string gatewayName = getJsonString(site, "gatewayName");
    if (gatewayId != pending.gatewayId || factoryId.empty() || areaId.empty() ||
        !root.HasMember("ports") || !root["ports"].IsArray()) {
        return false;
    }

    std::set<std::pair<std::string, int> > selected;
    for (const SyncSelectedDevice& device : pending.devices) {
        selected.insert(std::make_pair(device.portId, device.deviceId));
    }

    const std::int64_t timestampMs = getJsonInt64(root, "timestampMs", currentTimeMs());
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
        port.lastRegisterTimeMs = timestampMs;
        port.updateTimeMs = timestampMs;

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

            const int deviceId = getJsonInt(deviceValue, "deviceId", 0);
            if (selected.find(std::make_pair(port.portId, deviceId)) == selected.end()) {
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
            device.deviceType = getJsonString(deviceValue, "deviceType");
            device.pollIntervalMs = getJsonInt(deviceValue, "pollIntervalMs", 1000);
            device.expectTelemetry = device.deviceType != "relay";
            device.enabled = true;
            device.status = port.status == "connected" ? "online" : "unknown";
            device.lastSeenMs = port.status == "connected" ? timestampMs : 0;
            device.createTimeMs = timestampMs;
            device.updateTimeMs = timestampMs;
            device.deviceName = "Device " + std::to_string(deviceId);
            snapshotDevice.thresholdEnabled = getJsonBool(deviceValue, "thresholdEnabled", false);

            if (device.deviceType == "sensor_th" &&
                deviceValue.HasMember("thresholdConfig") &&
                deviceValue["thresholdConfig"].IsObject()) {
                const rapidjson::Value& thresholdConfig = deviceValue["thresholdConfig"];
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
}

MqttMessageHandler::MqttMessageHandler(PcDatabase& database,
                                       PcDataService& dataService,
                                       IpcServer& ipc,
                                       MqttClient& mqtt)
    : m_database(database)
    , m_dataService(dataService)
    , m_ipc(ipc)
    , m_mqtt(mqtt)
{
}

void MqttMessageHandler::handle(const std::string& topic, const std::string& payload)
{
    std::cout << "[MQTT RX] topic: " << topic << std::endl;
    std::cout << "[MQTT RX] payload: " << payload << std::endl;

    const std::string messageType = extractJsonStringValue(payload, "type");

    if (messageType == "alarm_event" ||
        (messageType == "command" && extractJsonStringValue(payload, "cmd") == "emergency")) {
        AlarmEvent event;
        std::string reason;
        if (!parseAlarmEventPayload(payload, event, reason)) {
            std::cout << "[MQTT RX] alarm_event parse failed: " << reason << std::endl;
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
        GatewayStatus gateway;
        if (!parseGatewayRegister(payload, gateway)) {
            std::cout << "[MQTT RX] gateway_register parse failed" << std::endl;
            return;
        }

        const bool ok = m_database.isOpen() && m_database.upsertGatewayStatus(gateway);
        std::cout << "[MQTT RX] gateway_register "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << gateway.gatewayId << std::endl;

        if (ok && m_ipc.hasClient()) {
            sendGatewayStatusSnapshot(m_ipc, m_database);
        }
        return;
    }

    if (messageType == "gateway_heartbeat") {
        std::string gatewayId;
        std::int64_t timestampMs = 0;
        std::string status;
        if (!parseGatewayHeartbeat(payload, gatewayId, timestampMs, status)) {
            std::cout << "[MQTT RX] gateway_heartbeat parse failed" << std::endl;
            return;
        }

        const bool ok = m_database.isOpen() &&
            m_database.updateGatewayHeartbeat(gatewayId, timestampMs, status);
        std::cout << "[MQTT RX] gateway_heartbeat "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << gatewayId << std::endl;

        if (ok && m_ipc.hasClient()) {
            sendGatewayStatusSnapshot(m_ipc, m_database);
        }
        return;
    }

    if (messageType == "port_register") {
        GatewayPort port;
        if (!parsePortRegister(payload, port)) {
            std::cout << "[MQTT RX] port_register parse failed" << std::endl;
            return;
        }

        const bool ok = m_database.isOpen() && m_database.upsertGatewayPort(port);
        std::cout << "[MQTT RX] port_register "
                  << (ok ? "ok" : "failed")
                  << ", gateway: " << port.gatewayId
                  << ", port: " << port.portId
                  << ", status: " << port.status << std::endl;

        if (ok && m_ipc.hasClient()) {
            sendPortStatusSnapshot(m_ipc, m_database);
        }
        return;
    }

    if (messageType == "ack") {
        rapidjson::Document root;
        root.Parse(payload.c_str());
        if (root.HasParseError() || !root.IsObject()) {
            std::cout << "[MQTT RX] ack parse failed" << std::endl;
            return;
        }

        const std::int64_t seq = getJsonInt64(root, "seq", 0);
        std::string commandType = getJsonString(root, "commandType");
        if (commandType.empty()) {
            commandType = getJsonString(root, "cmd");
        }
        if (commandType.empty()) {
            commandType = getJsonString(root, "command");
        }
        const std::string ackStatus = getJsonString(root, "status");
        std::string logStatus = (ackStatus == "ok" || ackStatus == "success") ? "success" : "failed";
        std::string reason = getJsonString(root, "reason");
        std::string message = getJsonString(root, "message");

        CommandLogTarget target;
        const bool hasTarget = m_database.isOpen() &&
                               m_database.queryCommandTargetBySeq(seq, target);
        if (commandType.empty() && hasTarget) {
            commandType = target.commandType;
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
                sendLatestPoints(m_ipc, m_dataService, m_database);
                sendDevicesSnapshot(m_ipc, m_database);
                sendPortStatusSnapshot(m_ipc, m_database);
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
        if (m_database.isOpen()) {
            m_database.updateCommandLogBySeq(seq, logStatus, reason, message, currentTimeMs());
        }
        if (logStatus == "success" &&
            commandType == "add_device" &&
            hasTarget &&
            target.commandType == "add_device") {
            m_dataService.forgetRemovedDevice(target.gatewayId,
                                              target.portId,
                                              target.deviceId);
        }
        if (m_ipc.hasClient()) {
            m_ipc.sendMessage(buildCommandLogUpdateJson(seq,
                                                        commandType,
                                                        logStatus,
                                                        reason,
                                                        message,
                                                        hasTarget ? &target : nullptr));
        }
        std::cout << "[MQTT RX] ack seq: " << seq
                  << ", command: " << commandType
                  << ", status: " << logStatus << std::endl;
        return;
    }

    if (messageType == "config_snapshot") {
        rapidjson::Document root;
        root.Parse(payload.c_str());
        if (root.HasParseError() || !root.IsObject()) {
            std::cout << "[MQTT RX] config_snapshot parse failed" << std::endl;
            return;
        }

        const std::int64_t seq = getJsonInt64(root, "seq", 0);
        SyncGatewayPending pending;
        if (!m_dataService.findSyncPending(seq, pending)) {
            std::cout << "[MQTT RX] config_snapshot skip unknown seq: " << seq << std::endl;
            return;
        }

        std::vector<GatewayPort> ports;
        std::vector<ConfigSnapshotDevice> devices;
        std::vector<PointConfig> pointConfigs;
        int portCount = 0;
        int deviceCount = 0;
        bool ok = parseConfigSnapshot(payload, pending, ports, devices, pointConfigs, portCount, deviceCount);

        if (ok && m_database.isOpen()) {
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
            if (ok) {
                for (const ConfigSnapshotDevice& snapshotDevice : devices) {
                    m_dataService.forgetRemovedDevice(snapshotDevice.device.gatewayId,
                                                      snapshotDevice.device.portId,
                                                      snapshotDevice.device.deviceId);
                }
            }
        } else if (ok) {
            ok = false;
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
            if ((result.portCount > 0 || result.deviceCount > 0) && m_ipc.hasClient()) {
                sendPortStatusSnapshot(m_ipc, m_database);
                sendDevicesSnapshot(m_ipc, m_database);
            }
        }

        std::cout << "[MQTT RX] config_snapshot seq: " << seq
                  << ", ok: " << ok
                  << ", ports: " << portCount
                  << ", devices: " << deviceCount
                  << std::endl;
        return;
    }

    if (messageType == "device_register") {
        DeviceRecord device;
        std::uint32_t sequence = 0;
        std::string reason;

        if (!parseDeviceRegister(payload, device, sequence, reason)) {
            std::cout << "[MQTT RX] device_register parse failed" << std::endl;
            return;
        }

        bool ok = reason.empty();
        if (ok) {
            if (m_database.isOpen()) {
                ok = m_database.upsertDevice(device);
                if (!ok) {
                    reason = "device_db_save_failed";
                } else {
                    m_dataService.forgetRemovedDevice(device.gatewayId,
                                                      device.portId,
                                                      device.deviceId);
                }
            } else {
                ok = false;
                reason = "database_not_open";
            }
        }

        const std::string ack = buildDeviceRegisterAckJson(sequence, device, ok, reason);
        const bool publishOk = m_mqtt.publish(kDeviceRegisterAckTopic, ack);
        std::cout << "[MQTT RX] device_register "
                  << (ok ? "ok" : "failed")
                  << ", ack publish: "
                  << (publishOk ? "ok" : "failed")
                  << ", gateway: "
                  << device.gatewayId
                  << ", port: "
                  << device.portId
                  << ", device: "
                  << device.deviceId
                  << std::endl;

        if (m_ipc.hasClient()) {
            sendDevicesSnapshot(m_ipc, m_database);
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
            m_database.savePointConfigs(configs);
        } else {
            std::cout << "[MQTT RX] database is not open, skip point_config storage" << std::endl;
        }
        return;
    }

    if (messageType != "telemetry_pack") {
        std::cout << "[MQTT RX] skip unsupported message type: "
                  << messageType
                  << std::endl;
        return;
    }

    TelemetryPack pack;
    std::string errorMessage;

    if (!TelemetryPackParser::parseJson(payload, pack, errorMessage)) {
        std::cout << "[MQTT RX] parse failed: " << errorMessage << std::endl;
        return;
    }

    std::cout << "[MQTT RX] parse ok, sequence: "
              << pack.sequence
              << ", device count: "
              << pack.devices.size()
              << std::endl;

    std::vector<TelemetryPoint> receivedPoints =
        m_dataService.filterRemovedPoints(ModelConverter::toTelemetryPoints(pack));

    std::cout << "[MQTT RX] received point count: "
              << receivedPoints.size()
              << std::endl;

    m_dataService.handleTelemetryPack(pack);

    std::vector<TelemetryPoint> points = m_dataService.getLatestPoints();

    std::cout << "[MQTT RX] snapshot point count: "
              << points.size()
              << std::endl;

    if (m_database.isOpen()) {
        m_database.saveTelemetryPoints(receivedPoints);
    } else {
        std::cout << "[MQTT RX] database is not open, skip storage" << std::endl;
    }

    if (m_ipc.hasClient()) {
        std::string json = buildLatestPointsJson(points);
        std::cout << "[MQTT RX] send latest_points to Pc_ui, size: "
                  << json.size()
                  << std::endl;
        m_ipc.sendMessage(json);
    } else {
        std::cout << "[MQTT RX] Pc_ui not connected, latest_points not sent" << std::endl;
    }
}
