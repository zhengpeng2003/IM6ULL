#include "MqttMessageHandler.hpp"

#include <cstdint>
#include <iostream>
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
