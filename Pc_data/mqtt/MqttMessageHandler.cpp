#include "MqttMessageHandler.hpp"

#include <cstdint>
#include <iostream>
#include <set>
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

std::string buildSnapshotPointId(const std::string& gatewayId,
                                 const std::string& portId,
                                 int deviceId,
                                 const std::string& pointKey)
{
    return gatewayId + "." + portId + "." + std::to_string(deviceId) + "." + pointKey;
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
    config.pointId = buildSnapshotPointId(device.gatewayId, device.portId, device.deviceId, pointKey);
    configs.push_back(config);
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

    const std::string gatewayId = getJsonString(root, "gatewayId");
    if (gatewayId != pending.gatewayId ||
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
            device.gatewayId = gatewayId;
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
                                               "温度",
                                               "℃",
                                               timestampMs);
                }
                if (thresholds && thresholds->HasMember("humidity")) {
                    appendThresholdPointConfig(pointConfigs,
                                               (*thresholds)["humidity"],
                                               device,
                                               "humidity",
                                               "湿度",
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
        const std::string ackStatus = getJsonString(root, "status");
        const std::string logStatus = ackStatus == "ok" ? "success" : "failed";
        const std::string reason = getJsonString(root, "reason");
        const std::string message = getJsonString(root, "message");

        if (m_database.isOpen()) {
            m_database.updateCommandLogBySeq(seq, logStatus, reason, message, currentTimeMs());
        }
        if (m_ipc.hasClient()) {
            m_ipc.sendMessage(buildCommandLogUpdateJson(seq, commandType, logStatus, reason, message));
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

    std::vector<TelemetryPoint> receivedPoints = ModelConverter::toTelemetryPoints(pack);

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
