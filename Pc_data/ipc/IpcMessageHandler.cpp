#include "IpcMessageHandler.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/document.h>

#include "common/JsonUtils.hpp"
#include "ipc/IpcServer.hpp"
#include "ipc/PcUiPublisher.hpp"
#include "model/TelemetryPoint.hpp"
#include "mqtt/MqttClient.hpp"
#include "protocol/PcDataMessages.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

IpcMessageHandler::IpcMessageHandler(PcDatabase& database,
                                     PcDataService& dataService,
                                     IpcServer& ipc,
                                     MqttClient& mqtt,
                                     MqttConfig& mqttConfig)
    : m_database(database)
    , m_dataService(dataService)
    , m_ipc(ipc)
    , m_mqtt(mqtt)
    , m_mqttConfig(mqttConfig)
{
}

namespace {

std::vector<SyncGatewaySelection> parseSyncConfigRequest(const std::string& msg)
{
    std::vector<SyncGatewaySelection> targets;
    rapidjson::Document root;
    root.Parse(msg.c_str());
    if (root.HasParseError() || !root.IsObject() ||
        getJsonString(root, "type") != "sync_config_request" ||
        !root.HasMember("targets") || !root["targets"].IsArray()) {
        return targets;
    }

    for (const rapidjson::Value& targetValue : root["targets"].GetArray()) {
        if (!targetValue.IsObject()) {
            continue;
        }

        SyncGatewaySelection target;
        target.gatewayId = getJsonString(targetValue, "gatewayId");
        if (target.gatewayId.empty() || !targetValue.HasMember("devices") ||
            !targetValue["devices"].IsArray()) {
            continue;
        }

        for (const rapidjson::Value& deviceValue : targetValue["devices"].GetArray()) {
            if (!deviceValue.IsObject()) {
                continue;
            }

            SyncSelectedDevice device;
            device.portId = getJsonString(deviceValue, "portId");
            device.deviceId = getJsonInt(deviceValue, "deviceId", 0);
            if (!device.portId.empty() && device.deviceId > 0) {
                target.devices.push_back(device);
            }
        }

        if (!target.devices.empty()) {
            targets.push_back(target);
        }
    }

    return targets;
}

bool isSyncConfigRequest(const std::string& msg)
{
    rapidjson::Document root;
    root.Parse(msg.c_str());
    return !root.HasParseError() && root.IsObject() &&
           getJsonString(root, "type") == "sync_config_request";
}

std::string buildGetConfigCommandJson(const SyncGatewayPending& pending)
{
    std::ostringstream oss;
    oss << "{";
    oss << "\"type\":\"command\",";
    oss << "\"cmd\":\"get_config\",";
    oss << "\"seq\":" << pending.seq << ",";
    oss << "\"target\":{";
    oss << "\"gatewayId\":\"" << jsonEscape(pending.gatewayId) << "\",";
    oss << "\"devices\":[";
    for (size_t i = 0; i < pending.devices.size(); ++i) {
        const SyncSelectedDevice& device = pending.devices[i];
        if (i > 0) {
            oss << ",";
        }
        oss << "{";
        oss << "\"portId\":\"" << jsonEscape(device.portId) << "\",";
        oss << "\"deviceId\":" << device.deviceId;
        oss << "}";
    }
    oss << "]";
    oss << "}";
    oss << "}";
    return oss.str();
}

} // namespace

void IpcMessageHandler::handle(const std::string& msg)
{
    std::cout << "Pc_data recv: " << msg << std::endl;

    std::string pointId;
    std::int64_t startMs = 0;
    std::int64_t endMs = 0;
    int limit = 1000;

    std::vector<SyncGatewaySelection> syncTargets = parseSyncConfigRequest(msg);
    if (!syncTargets.empty()) {
        const std::vector<SyncGatewayPending> pendingList =
            m_dataService.beginSyncConfigRequest(syncTargets);
        if (pendingList.empty()) {
            m_ipc.sendMessage(buildSyncConfigResultJson(false, "invalid sync target", 0, 0));
            return;
        }

        for (const SyncGatewayPending& pending : pendingList) {
            const std::string topic = "cmd/" + pending.gatewayId;
            const std::string command = buildGetConfigCommandJson(pending);
            const bool publishOk = m_mqtt.publish(topic, command);
            std::cout << "sync_config get_config publish "
                      << (publishOk ? "ok" : "failed")
                      << ", topic: " << topic
                      << ", seq: " << pending.seq
                      << std::endl;

            if (!publishOk) {
                SyncConfigResult result;
                if (m_dataService.completeSyncConfig(pending.seq,
                                                     false,
                                                     "mqtt publish failed",
                                                     0,
                                                     0,
                                                     result)) {
                    m_ipc.sendMessage(buildSyncConfigResultJson(result.success,
                                                                result.message,
                                                                result.portCount,
                                                                result.deviceCount));
                }
            }
        }
        return;
    }
    if (isSyncConfigRequest(msg)) {
        m_ipc.sendMessage(buildSyncConfigResultJson(false, "invalid sync target", 0, 0));
        return;
    }

    if (parseHistoryQuery(msg, pointId, startMs, endMs, limit)) {
        const std::vector<TelemetryPoint> points =
            m_database.queryHistoryPoints(pointId, startMs, endMs, limit);

        m_ipc.sendMessage(buildHistoryPointsJson(pointId, points));

        std::cout << "send history_points done, pointId: "
                  << pointId
                  << ", count: "
                  << points.size()
                  << std::endl;
        return;
    }

    std::string gatewayId;
    std::string portId;
    int deviceId = 0;

    if (parseDeleteDeviceRequest(msg, gatewayId, portId, deviceId)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.deleteDeviceData(gatewayId, portId, deviceId);
        } else {
            std::cout << "delete_device_data skipped: database is not open" << std::endl;
        }

        bool snapshotRemoved = false;
        if (dbOk) {
            snapshotRemoved = m_dataService.removeDeviceData(gatewayId, portId, deviceId);
        }
        const bool ok = dbOk;
        const std::string reason = ok ? "" : "delete_device_data_failed";

        m_ipc.sendMessage(buildDeleteDataAckJson("delete_device_data", ok, reason));
        sendLatestPoints(m_ipc, m_dataService, m_database);
        sendDevicesSnapshot(m_ipc, m_database);

        std::cout << "delete_device_data done, gateway: "
                  << gatewayId
                  << ", port: "
                  << portId
                  << ", device: "
                  << deviceId
                  << ", dbOk: "
                  << dbOk
                  << ", snapshotRemoved: "
                  << snapshotRemoved
                  << std::endl;
        return;
    }

    if (parseDeleteMasterRequest(msg, gatewayId, portId)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.deleteMasterData(gatewayId, portId);
        } else {
            std::cout << "delete_master_data skipped: database is not open" << std::endl;
        }

        const bool snapshotRemoved = m_dataService.removeMasterData(gatewayId, portId);
        const bool ok = dbOk || snapshotRemoved;
        const std::string reason = ok ? "" : "delete_master_data_failed";

        m_ipc.sendMessage(buildDeleteDataAckJson("delete_master_data", ok, reason));
        sendLatestPoints(m_ipc, m_dataService, m_database);
        sendDevicesSnapshot(m_ipc, m_database);

        std::cout << "delete_master_data done, gateway: "
                  << gatewayId
                  << ", port: "
                  << portId
                  << ", dbOk: "
                  << dbOk
                  << ", snapshotRemoved: "
                  << snapshotRemoved
                  << std::endl;
        return;
    }

    if (parseClearRecoveredAlarmsRequest(msg)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.clearRecoveredAlarms();
        } else {
            std::cout << "clear_recovered_alarms skipped: database is not open" << std::endl;
        }

        const std::string reason = dbOk ? "" : "clear_recovered_alarms_failed";
        m_ipc.sendMessage(buildDeleteDataAckJson("clear_recovered_alarms", dbOk, reason));

        std::cout << "clear_recovered_alarms done, dbOk: "
                  << dbOk
                  << std::endl;
        return;
    }

    if (parseClearAllDataRequest(msg)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.clearAllData();
        } else {
            std::cout << "clear_all_data skipped: database is not open" << std::endl;
        }

        if (dbOk) {
            m_dataService.clear();
        }

        const std::string reason = dbOk ? "" : "clear_all_data_failed";
        m_ipc.sendMessage(buildDeleteDataAckJson("clear_all_data", dbOk, reason));
        sendLatestPoints(m_ipc, m_dataService, m_database);
        sendDevicesSnapshot(m_ipc, m_database);
        sendGatewayStatusSnapshot(m_ipc, m_database);
        sendPortStatusSnapshot(m_ipc, m_database);

        std::cout << "clear_all_data done, dbOk: "
                  << dbOk
                  << std::endl;
        return;
    }

    if (parseGetMqttConfigRequest(msg)) {
        m_ipc.sendMessage(buildMqttConfigJson(m_mqttConfig, m_mqtt.status()));
        std::cout << "send mqtt_config done" << std::endl;
        return;
    }

    MqttConfig requestedMqttConfig = m_mqttConfig;
    std::string mqttConfigReason;
    if (parseSaveMqttConfigRequest(msg, requestedMqttConfig, mqttConfigReason)) {
        if (!mqttConfigReason.empty()) {
            m_ipc.sendMessage(buildMqttConfigAckJson(false, mqttConfigReason, m_mqttConfig, m_mqtt.status()));
            std::cout << "save_mqtt_config rejected: " << mqttConfigReason << std::endl;
            return;
        }

        if (!saveMqttConfigFile(requestedMqttConfig)) {
            m_ipc.sendMessage(buildMqttConfigAckJson(false, "config_save_failed", m_mqttConfig, m_mqtt.status()));
            std::cout << "save_mqtt_config file write failed" << std::endl;
            return;
        }

        m_mqttConfig = requestedMqttConfig;
        const bool connectOk = m_mqtt.connectToBroker(
            m_mqttConfig.host,
            m_mqttConfig.port,
            m_mqttConfig.clientId,
            m_mqttConfig.topics);

        m_ipc.sendMessage(buildMqttConfigAckJson(connectOk, connectOk ? "" : "mqtt_connect_failed", m_mqttConfig, m_mqtt.status()));

        std::cout << "save_mqtt_config done, host: "
                  << m_mqttConfig.host
                  << ", port: "
                  << m_mqttConfig.port
                  << ", connectOk: "
                  << connectOk
                  << std::endl;
        return;
    }

    /*
     * 兼容你之前 Pc_ui 可能发送的 get_snapshot。
     * 后面建议统一改成 get_latest_points。
     */
    if (msg.find("get_gateway_status") != std::string::npos) {
        sendGatewayStatusSnapshot(m_ipc, m_database);
    } else if (msg.find("get_port_status") != std::string::npos) {
        sendPortStatusSnapshot(m_ipc, m_database);
    } else if (msg.find("get_devices") != std::string::npos) {
        sendDevicesSnapshot(m_ipc, m_database);
    } else if (msg.find("get_latest_points") != std::string::npos ||
        msg.find("get_snapshot") != std::string::npos) {

        sendLatestPoints(m_ipc, m_dataService, m_database);
    } else if (msg.find("\"type\":\"command\"") != std::string::npos ||
               msg.find("\"msg_type\":\"command\"") != std::string::npos) {
        rapidjson::Document root;
        root.Parse(msg.c_str());
        const std::string cmdId = extractJsonStringValue(msg, "cmd_id");
        std::string commandType = root.IsObject() ? getJsonString(root, "commandType") : "";
        if (commandType.empty() && root.IsObject()) {
            commandType = getJsonString(root, "cmd");
        }
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

        if (commandType == "add_device" || commandType == "remove_device") {
            const std::int64_t seq = root.IsObject() ? getJsonInt64(root, "seq", 0) : 0;
            int deviceId = 0;
            if (root.IsObject() && root.HasMember("device") && root["device"].IsObject()) {
                deviceId = getJsonInt(root["device"], "deviceId", getJsonInt(root["device"], "slaveAddress", 0));
            }
            if (deviceId <= 0 && root.IsObject()) {
                deviceId = getJsonInt(root, "deviceId", 0);
            }
            if (deviceId <= 0 && root.IsObject()) {
                deviceId = getJsonInt(root, "slave_id", 0);
            }

            if (seq <= 0) {
                m_ipc.sendMessage(buildCommandAckJson(cmdId, false, "invalid_argument"));
                std::cout << commandType << " rejected, missing seq" << std::endl;
                return;
            }

            const std::string commandId = cmdId.empty()
                ? std::string("CMD") + std::to_string(seq)
                : cmdId;
            if (m_database.isOpen()) {
                m_database.createCommandLog(commandId,
                                            seq,
                                            commandType,
                                            gatewayId,
                                            portId,
                                            deviceId,
                                            currentTimeMs());
            }

            if (!m_database.isOpen() || !m_database.isGatewayPortConnected(gatewayId, portId)) {
                if (m_database.isOpen()) {
                    m_database.updateCommandLogBySeq(seq,
                                                     "failed",
                                                     "port_not_found",
                                                     "gateway port is not connected",
                                                     currentTimeMs());
                }
                m_ipc.sendMessage(buildCommandAckJson(cmdId, false, "port_not_found"));
                std::cout << commandType << " rejected, port not connected, gateway: "
                          << gatewayId << ", port: " << portId << std::endl;
                return;
            }

            const std::string topic = "cmd/" + gatewayId;
            const bool publishOk = m_mqtt.publish(topic, msg);
            if (m_database.isOpen()) {
                m_database.updateCommandLogBySeq(seq,
                                                 publishOk ? "sent" : "failed",
                                                 publishOk ? "" : "mqtt_publish_failed",
                                                 publishOk ? "command sent" : "mqtt publish failed",
                                                 currentTimeMs());
            }
            m_ipc.sendMessage(buildCommandAckJson(cmdId, publishOk, publishOk ? "sent" : "mqtt_publish_failed"));
            std::cout << commandType << " publish "
                      << (publishOk ? "ok" : "failed")
                      << ", topic: " << topic
                      << ", cmd_id: " << cmdId << std::endl;
            return;
        }

        m_ipc.sendMessage(buildCommandAckJson(cmdId, true, ""));
        std::cout << "send command_ack done, cmd_id: " << cmdId << std::endl;
    } else {
        m_ipc.sendMessage(R"({"type":"ack","cmd":"unknown","status":"ok","message":"Pc_data received"})");

        std::cout << "send unknown ack done" << std::endl;
    }
}
