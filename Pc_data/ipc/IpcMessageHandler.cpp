#include "IpcMessageHandler.hpp"

#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

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


bool isRemoteCommandAllowed(const std::string& commandType)
{
    return commandType == "add_device" ||
           commandType == "remove_device" ||
           commandType == "set_relay" ||
           commandType == "set_device_threshold" ||
           commandType == "set_threshold" ||
           commandType == "get_config" ||
           commandType == "request_config_snapshot" ||
           commandType == "discover";
}

std::string commandNameFrom(const rapidjson::Value& root)
{
    return getJsonStringAny(root, {"cmdType", "cmd", "commandType", "command"});
}

const rapidjson::Value& payloadObjectOrRoot(const rapidjson::Value& root)
{
    if (root.IsObject() && root.HasMember("payload") && root["payload"].IsObject()) {
        return root["payload"];
    }
    return root;
}

std::int64_t sequenceFrom(const rapidjson::Value& root)
{
    return getJsonInt64Any(root, {"seq", "sequence"}, 0);
}

std::int64_t linuxDataSeqFrom(std::int64_t seq)
{
    constexpr std::int64_t maxLinuxDataSeq = 0x7fffffff;
    seq &= maxLinuxDataSeq;
    return seq > 0 ? seq : 1;
}

void rememberPendingCommand(PcDataService& dataService,
                            const std::string& commandId,
                            std::int64_t uiSeq,
                            std::int64_t boardSeq,
                            const std::string& commandType,
                            const std::string& gatewayId,
                            const std::string& portId,
                            int deviceId)
{
    PendingCommandTarget target;
    target.commandId = commandId;
    target.uiSeq = uiSeq;
    target.boardSeq = boardSeq;
    target.commandType = commandType;
    target.gatewayId = gatewayId;
    target.portId = portId;
    target.deviceId = deviceId;
    target.requestTimeMs = currentTimeMs();
    dataService.rememberPendingCommand(target);
}

int deviceIdFrom(const rapidjson::Value& root)
{
    return getJsonIntAny(root, {"deviceId", "slave_id", "slaveAddress", "device_id", "slave_addr"}, 0);
}

int slotFromPortId(const std::string& portId, int fallback)
{
    if (fallback >= 0) {
        return fallback;
    }
    std::string digits;
    for (char ch : portId) {
        if (ch >= '0' && ch <= '9') {
            digits.push_back(ch);
        }
    }
    if (digits.empty()) {
        return 0;
    }
    int value = std::stoi(digits);
    return value > 0 ? value - 1 : 0;
}

std::string jsonValueToString(const rapidjson::Value& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    value.Accept(writer);
    return buffer.GetString();
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

std::string makeGatewayCommandTopic(const std::string& gatewayId)
{
    return gatewayId.empty() ? std::string() : "cmd/" + gatewayId;
}

std::string makePortCommandTopic(const std::string& gatewayId, const std::string& portId)
{
    return gatewayId.empty() || portId.empty() ? std::string() : "cmd/" + gatewayId + "/" + portId;
}

bool isGatewayCommand(const std::string& commandType)
{
    return commandType == "delete_gateway" ||
           commandType == "disconnect_gateway" ||
           commandType == "request_gateway_status" ||
           commandType == "get_config" ||
           commandType == "request_config_snapshot" ||
           commandType == "discover";
}

std::string commandTopicFor(const std::string& commandType,
                            const std::string& gatewayId,
                            const std::string& portId)
{
    return isGatewayCommand(commandType)
        ? makeGatewayCommandTopic(gatewayId)
        : makePortCommandTopic(gatewayId, portId);
}

void failGatewayNotRegistered(IpcServer& ipc,
                              PcDatabase& database,
                              const std::string& commandId,
                              std::int64_t uiSeq,
                              std::int64_t boardSeq,
                              const std::string& commandType,
                              const std::string& gatewayId,
                              const std::string& portId,
                              int deviceId)
{
    const std::int64_t nowMs = currentTimeMs();
    if (database.isOpen()) {
        if (!commandId.empty() && boardSeq > 0) {
            database.createCommandLog(commandId,
                                      boardSeq,
                                      commandType,
                                      gatewayId,
                                      portId,
                                      deviceId,
                                      nowMs);
            database.updateCommandLogByCommandId(commandId,
                                                 "failed",
                                                 "gateway_not_registered",
                                                 "gateway is not registered",
                                                 nowMs);
        }
    }
    ipc.sendMessage(buildCommandAckJson(commandId,
                                        false,
                                        "gateway_not_registered",
                                        uiSeq,
                                        commandType,
                                        "done",
                                        "gateway is not registered",
                                        boardSeq,
                                        gatewayId,
                                        portId,
                                        deviceId));
    std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
              << " topic=<unregistered> cmd=" << commandType
              << " seq=" << boardSeq
              << " failed=gateway_not_registered" << std::endl;
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
            const std::string topic = makeGatewayCommandTopic(pending.gatewayId);
            if (topic.empty()) {
                SyncConfigResult result;
                if (m_dataService.completeSyncConfig(pending.seq,
                                                     false,
                                                     "gateway_not_registered",
                                                     0,
                                                     0,
                                                     result)) {
                    m_ipc.sendMessage(buildSyncConfigResultJson(result.success,
                                                                result.message,
                                                                result.portCount,
                                                                result.deviceCount));
                }
                std::cout << "[MQTT TX CMD] gatewayId=" << pending.gatewayId
                          << " topic=<unregistered> cmd=get_config seq=" << pending.seq
                          << " failed=gateway_not_registered" << std::endl;
                continue;
            }
            const std::string command = buildGetConfigCommandJson(pending);
            const bool publishOk = m_mqtt.publish(topic, command);
            std::cout << "sync_config get_config publish "
                      << (publishOk ? "ok" : "failed")
                      << ", topic: " << topic
                      << ", seq: " << pending.seq
                      << std::endl;
            std::cout << "[MQTT TX CMD] gatewayId=" << pending.gatewayId
                      << " topic=" << topic
                      << " cmd=get_config seq=" << pending.seq << std::endl;

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
        bool queryOk = false;
        std::string reason;
        const std::vector<TelemetryPoint> points =
            m_database.queryHistoryPoints(pointId, startMs, endMs, limit, &queryOk, &reason);

        const std::string message = queryOk ? std::string() : std::string("history query failed");
        m_ipc.sendMessage(buildHistoryPointsJson(pointId, points, queryOk, reason, message));

        std::cout << "send history_points done, pointId: "
                  << pointId
                  << ", ok: " << (queryOk ? "true" : "false")
                  << ", reason: " << reason
                  << ", count: "
                  << points.size()
                  << std::endl;
        return;
    }

    {
        rapidjson::Document root;
        root.Parse(msg.c_str());
        if (!root.HasParseError() && root.IsObject() &&
            getJsonString(root, "type") == "query_history") {
            const std::string invalidPointId = getJsonString(root, "pointId");
            m_ipc.sendMessage(buildHistoryPointsJson(invalidPointId,
                                                     std::vector<TelemetryPoint>(),
                                                     false,
                                                     "invalid_point_id",
                                                     "pointId is required"));
            return;
        }
    }

    std::string gatewayId;
    std::string portId;
    int deviceId = 0;
    std::string alarmId;

    if (parseGetAlarmEventsRequest(msg)) {
        if (m_database.isOpen()) {
            m_ipc.sendMessage(buildAlarmEventsSnapshotJson(m_database.queryAlarmEvents()));
        } else {
            m_ipc.sendMessage(buildAlarmEventsSnapshotJson(std::vector<AlarmEvent>()));
        }
        std::cout << "send alarm_events_snapshot done" << std::endl;
        return;
    }

    if (parseAckAlarmRequest(msg, alarmId)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.acknowledgeAlarm(alarmId, currentTimeMs());
        } else {
            std::cout << "ack_alarm skipped: database is not open" << std::endl;
        }

        const std::string reason = dbOk ? "" : "ack_alarm_failed";
        m_ipc.sendMessage(buildAlarmActionAckJson("ack_alarm", dbOk, reason, alarmId));
        if (m_database.isOpen()) {
            m_ipc.sendMessage(buildAlarmEventsSnapshotJson(m_database.queryAlarmEvents()));
        }

        std::cout << "ack_alarm done, alarmId: "
                  << alarmId
                  << ", dbOk: "
                  << dbOk
                  << std::endl;
        return;
    }

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
        sendLatestPoints(m_ipc, m_dataService);
        sendDevicesSnapshot(m_ipc, m_dataService);

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
        sendLatestPoints(m_ipc, m_dataService);
        sendDevicesSnapshot(m_ipc, m_dataService);

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
        m_ipc.sendMessage(buildAlarmActionAckJson("clear_recovered_alarms", dbOk, reason));
        if (m_database.isOpen()) {
            m_ipc.sendMessage(buildAlarmEventsSnapshotJson(m_database.queryAlarmEvents()));
        }

        std::cout << "clear_recovered_alarms done, dbOk: "
                  << dbOk
                  << std::endl;
        return;
    }

    if (parseClearAcknowledgedAlarmsRequest(msg)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.clearAcknowledgedAlarms();
        } else {
            std::cout << "clear_acknowledged_alarms skipped: database is not open" << std::endl;
        }

        const std::string reason = dbOk ? "" : "clear_acknowledged_alarms_failed";
        m_ipc.sendMessage(buildAlarmActionAckJson("clear_acknowledged_alarms", dbOk, reason));
        if (m_database.isOpen()) {
            m_ipc.sendMessage(buildAlarmEventsSnapshotJson(m_database.queryAlarmEvents()));
        }

        std::cout << "clear_acknowledged_alarms done, dbOk: "
                  << dbOk
                  << std::endl;
        return;
    }

    if (parseClearAllDataRequest(msg)) {
        bool dbOk = false;
        if (m_database.isOpen()) {
            dbOk = m_database.clearRuntimeData();
        } else {
            std::cout << "clear_all_data skipped: database is not open" << std::endl;
        }

        if (dbOk) {
            m_dataService.clear();
        }

        const std::string reason = dbOk ? "" : "clear_runtime_data_failed";
        m_ipc.sendMessage(buildDeleteDataAckJson("clear_all_data", dbOk, reason));
        sendLatestPoints(m_ipc, m_dataService);
        sendDevicesSnapshot(m_ipc, m_dataService);
        sendGatewayStatusSnapshot(m_ipc, m_dataService);
        sendPortStatusSnapshot(m_ipc, m_dataService);

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
        sendGatewayStatusSnapshot(m_ipc, m_dataService);
    } else if (msg.find("get_port_status") != std::string::npos) {
        sendPortStatusSnapshot(m_ipc, m_dataService);
    } else if (msg.find("get_devices") != std::string::npos) {
        sendDevicesSnapshot(m_ipc, m_dataService);
    } else if (msg.find("get_latest_points") != std::string::npos ||
        msg.find("get_snapshot") != std::string::npos) {

        sendLatestPoints(m_ipc, m_dataService);
    } else if (msg.find("\"type\":\"command\"") != std::string::npos ||
               msg.find("\"msg_type\":\"command\"") != std::string::npos) {
        rapidjson::Document root;
        root.Parse(msg.c_str());
        const std::string cmdId = extractJsonStringValue(msg, "cmd_id");
        std::string commandType = root.IsObject() ? commandNameFrom(root) : "";
        std::string gatewayId;
        std::string portId;
        if (root.IsObject() && root.HasMember("target") && root["target"].IsObject()) {
            gatewayId = getJsonString(root["target"], "gatewayId");
            portId = getJsonString(root["target"], "portId");
        }
        if (gatewayId.empty() && root.IsObject()) {
            gatewayId = getJsonString(root, "gatewayId");
        }
        if (gatewayId.empty() && root.IsObject()) {
            gatewayId = getJsonString(root, "gateway_id");
        }
        if (portId.empty() && root.IsObject()) {
            portId = getJsonString(root, "portId");
        }

        std::int64_t seqForAck = root.IsObject() ? sequenceFrom(root) : 0;
        const std::string commandIdForAck = cmdId;
        if (!isRemoteCommandAllowed(commandType)) {
            m_ipc.sendMessage(buildCommandAckJson(commandIdForAck, false, "unsupported_command", seqForAck, commandType, "done", "unsupported command", seqForAck, gatewayId, portId, 0));
            std::cout << "command rejected, unsupported command: " << commandType << std::endl;
            return;
        }
        if (commandIdForAck.empty() || gatewayId.empty() ||
            (!isGatewayCommand(commandType) && portId.empty())) {
            m_ipc.sendMessage(buildCommandAckJson(commandIdForAck, false, "invalid_request", seqForAck, commandType, "done", "gatewayId, seq, cmd_id and portId for port commands are required", seqForAck, gatewayId, portId, 0));
            std::cout << "command rejected, invalid target, command: " << commandType << std::endl;
            return;
        }

        if (commandType == "set_relay") {
            const std::int64_t uiSeq = root.IsObject() ? sequenceFrom(root) : 0;
            const std::int64_t boardSeq = linuxDataSeqFrom(uiSeq);
            const std::string commandId = cmdId;
            const rapidjson::Value& requestPayload = root.IsObject() ? payloadObjectOrRoot(root) : root;
            const int slot = root.IsObject() ? getJsonInt(requestPayload, "slot", getJsonInt(root, "slot", getJsonInt(root, "master_slot", -1))) : -1;
            const int slaveId = root.IsObject() ? deviceIdFrom(requestPayload) : 0;
            if (uiSeq <= 0 || gatewayId.empty() || portId.empty() || slot < 0 || slaveId <= 0 ||
                !requestPayload.IsObject() || !requestPayload.HasMember("states") || !requestPayload["states"].IsArray() || requestPayload["states"].Empty()) {
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "invalid_request", uiSeq, commandType, "done", "invalid relay command", boardSeq, gatewayId, portId, slaveId));
                std::cout << "set_relay rejected, invalid relay command" << std::endl;
                return;
            }

            const std::string topic = makePortCommandTopic(gatewayId, portId);
            if (topic.empty()) {
                failGatewayNotRegistered(m_ipc, m_database, commandId, uiSeq, boardSeq,
                                         commandType, gatewayId, portId, slaveId);
                return;
            }
            if (m_database.isOpen()) {
                m_database.createCommandLog(commandId, boardSeq, commandType, gatewayId, portId, slaveId, currentTimeMs());
            }
            if (m_database.isOpen() && !m_database.isGatewayPortConnected(gatewayId, portId)) {
                m_database.updateCommandLogByCommandId(commandId, "failed", "port_not_connected", "gateway port is not connected", currentTimeMs());
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "port_not_connected", uiSeq, commandType, "done", "gateway port is not connected", boardSeq, gatewayId, portId, slaveId));
                return;
            }

            std::ostringstream payload;
            payload << "{\"type\":\"command\",\"cmd\":\"set_relay\",\"cmd_id\":\"" << jsonEscape(commandId)
                    << "\",\"seq\":" << boardSeq
                    << ",\"cmdType\":\"set_relay\",\"gatewayId\":\"" << jsonEscape(gatewayId)
                    << "\",\"portId\":\"" << jsonEscape(portId) << "\",\"deviceId\":" << slaveId
                    << ",\"timestampMs\":" << currentTimeMs()
                    << ",\"slot\":" << slot << ",\"slave_id\":" << slaveId << ",\"device_id\":" << slaveId << ",\"states\":[";
            const rapidjson::Value& states = requestPayload["states"];
            for (rapidjson::SizeType i = 0; i < states.Size(); ++i) {
                if (i > 0) payload << ",";
                payload << (states[i].IsBool() ? (states[i].GetBool() ? "true" : "false") : (states[i].GetInt() != 0 ? "true" : "false"));
            }
            payload << "],\"payload\":{\"slot\":" << slot << ",\"slave_id\":" << slaveId
                    << ",\"deviceId\":" << slaveId << ",\"states\":" << jsonValueToString(states) << "}}";

            const bool publishOk = m_mqtt.publish(topic, payload.str());
            if (m_database.isOpen()) {
                m_database.updateCommandLogByCommandId(commandId, publishOk ? "sent" : "failed", publishOk ? "" : "mqtt_publish_failed", publishOk ? "command sent" : "mqtt publish failed", currentTimeMs());
            }
            if (publishOk) {
                rememberPendingCommand(m_dataService, commandId, uiSeq, boardSeq, commandType, gatewayId, portId, slaveId);
            }
            m_ipc.sendMessage(buildCommandAckJson(commandId, publishOk, publishOk ? "" : "mqtt_publish_failed", uiSeq, commandType, publishOk ? "sent" : "done", publishOk ? "command published to gateway" : "MQTT publish failed", boardSeq, gatewayId, portId, slaveId));
            std::cout << "set_relay publish " << (publishOk ? "ok" : "failed") << ", topic: " << topic << ", cmd_id: " << commandId << std::endl;
            std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
                      << " topic=" << topic
                      << " cmd=" << commandType
                      << " seq=" << boardSeq << std::endl;
            return;
        }

        if (commandType == "set_device_threshold" || commandType == "set_threshold") {
            const std::int64_t uiSeq = root.IsObject() ? sequenceFrom(root) : 0;
            const std::int64_t boardSeq = linuxDataSeqFrom(uiSeq);
            const std::string commandId = cmdId;
            const rapidjson::Value& requestPayload = root.IsObject() ? payloadObjectOrRoot(root) : root;
            const int slot = slotFromPortId(portId, root.IsObject() ? getJsonInt(requestPayload, "slot", getJsonInt(root, "slot", getJsonInt(root, "master_slot", -1))) : -1);
            int slaveId = root.IsObject() ? deviceIdFrom(requestPayload) : 0;
            if (slaveId <= 0 && root.IsObject() && root.HasMember("device") && root["device"].IsObject()) {
                slaveId = deviceIdFrom(root["device"]);
            }
            const bool hasThresholds = requestPayload.IsObject() &&
                ((requestPayload.HasMember("thresholds") && requestPayload["thresholds"].IsObject()) ||
                 (requestPayload.HasMember("threshold_config") && requestPayload["threshold_config"].IsObject()) ||
                 (requestPayload.HasMember("thresholdConfig") && requestPayload["thresholdConfig"].IsObject()));
            if (uiSeq <= 0 || slot < 0 || slaveId <= 0 || !hasThresholds) {
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "invalid_request", uiSeq, commandType, "done", "slot, slave_id and thresholds are required", boardSeq, gatewayId, portId, slaveId));
                std::cout << "set_device_threshold rejected, invalid request" << std::endl;
                return;
            }
            const std::string topic = makePortCommandTopic(gatewayId, portId);
            if (topic.empty()) {
                failGatewayNotRegistered(m_ipc, m_database, commandId, uiSeq, boardSeq,
                                         commandType, gatewayId, portId, slaveId);
                return;
            }
            if (m_database.isOpen()) {
                m_database.createCommandLog(commandId, boardSeq, commandType, gatewayId, portId, slaveId, currentTimeMs());
            }
            if (m_database.isOpen() && !m_database.isGatewayPortConnected(gatewayId, portId)) {
                m_database.updateCommandLogByCommandId(commandId, "failed", "port_not_connected", "gateway port is not connected", currentTimeMs());
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "port_not_connected", uiSeq, commandType, "done", "gateway port is not connected", boardSeq, gatewayId, portId, slaveId));
                return;
            }
            std::ostringstream payload;
            payload << "{\"type\":\"command\",\"cmd\":\"set_device_threshold\",\"cmd_id\":\"" << jsonEscape(commandId)
                    << "\",\"seq\":" << boardSeq
                    << ",\"cmdType\":\"set_device_threshold\",\"gatewayId\":\"" << jsonEscape(gatewayId)
                    << "\",\"portId\":\"" << jsonEscape(portId) << "\",\"deviceId\":" << slaveId
                    << ",\"timestampMs\":" << currentTimeMs()
                    << ",\"slot\":" << slot << ",\"slave_id\":" << slaveId
                    << ",\"threshold_enabled\":" << (getJsonBoolAny(requestPayload, {"threshold_enabled", "thresholdEnabled"}, true) ? "true" : "false");
            if (requestPayload.HasMember("thresholds") && requestPayload["thresholds"].IsObject()) {
                payload << ",\"thresholds\":" << jsonValueToString(requestPayload["thresholds"]);
            } else if (requestPayload.HasMember("threshold_config") && requestPayload["threshold_config"].IsObject()) {
                payload << ",\"threshold_config\":" << jsonValueToString(requestPayload["threshold_config"]);
            } else {
                payload << ",\"thresholdConfig\":" << jsonValueToString(requestPayload["thresholdConfig"]);
            }
            payload << ",\"payload\":{\"slot\":" << slot << ",\"slave_id\":" << slaveId
                    << ",\"deviceId\":" << slaveId << ",\"threshold_enabled\":"
                    << (getJsonBoolAny(requestPayload, {"threshold_enabled", "thresholdEnabled"}, true) ? "true" : "false");
            if (requestPayload.HasMember("thresholds") && requestPayload["thresholds"].IsObject()) {
                payload << ",\"thresholds\":" << jsonValueToString(requestPayload["thresholds"]);
            } else if (requestPayload.HasMember("threshold_config") && requestPayload["threshold_config"].IsObject()) {
                payload << ",\"threshold_config\":" << jsonValueToString(requestPayload["threshold_config"]);
            } else {
                payload << ",\"thresholdConfig\":" << jsonValueToString(requestPayload["thresholdConfig"]);
            }
            payload << "}}";
            const bool publishOk = m_mqtt.publish(topic, payload.str());
            if (m_database.isOpen()) {
                m_database.updateCommandLogByCommandId(commandId, publishOk ? "sent" : "failed", publishOk ? "" : "mqtt_publish_failed", publishOk ? "command sent" : "mqtt publish failed", currentTimeMs());
            }
            if (publishOk) {
                rememberPendingCommand(m_dataService, commandId, uiSeq, boardSeq, commandType, gatewayId, portId, slaveId);
            }
            m_ipc.sendMessage(buildCommandAckJson(commandId, publishOk, publishOk ? "" : "mqtt_publish_failed", uiSeq, commandType, publishOk ? "sent" : "done", publishOk ? "command published to gateway" : "MQTT publish failed", boardSeq, gatewayId, portId, slaveId));
            std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
                      << " topic=" << topic
                      << " cmd=" << commandType
                      << " seq=" << boardSeq << std::endl;
            return;
        }

        if (commandType == "add_device" || commandType == "remove_device") {
            const std::int64_t uiSeq = root.IsObject() ? sequenceFrom(root) : 0;
            const std::int64_t boardSeq = linuxDataSeqFrom(uiSeq);
            int deviceId = 0;
            const rapidjson::Value& requestPayload = root.IsObject() ? payloadObjectOrRoot(root) : root;
            if (root.IsObject() && root.HasMember("device") && root["device"].IsObject()) {
                deviceId = deviceIdFrom(root["device"]);
            }
            if (deviceId <= 0 && root.IsObject()) {
                deviceId = deviceIdFrom(requestPayload);
            }

            const std::string commandId = cmdId;
            if (uiSeq <= 0 || deviceId <= 0) {
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "invalid_request", uiSeq, commandType, "done", "deviceId or slave_id is required", boardSeq, gatewayId, portId, deviceId));
                std::cout << commandType << " rejected, missing device id" << std::endl;
                return;
            }
            std::string deviceType;
            int pollIntervalMs = 1000;
            const rapidjson::Value* deviceOptions = nullptr;
            if (commandType == "add_device") {
                if (root.IsObject() && root.HasMember("device") && root["device"].IsObject()) {
                    deviceType = getJsonStringAny(root["device"], {"deviceType", "device_type"});
                    pollIntervalMs = getJsonIntAny(root["device"], {"pollIntervalMs", "poll_interval_ms"}, pollIntervalMs);
                    if (root["device"].HasMember("device_options") &&
                        root["device"]["device_options"].IsObject()) {
                        deviceOptions = &root["device"]["device_options"];
                    }
                }
                if (deviceType.empty() && root.IsObject()) {
                    deviceType = getJsonStringAny(requestPayload, {"deviceType", "device_type"});
                }
                if (deviceType.empty()) {
                    deviceType = "sensor_th";
                }
            }
            const std::string topic = makePortCommandTopic(gatewayId, portId);
            if (topic.empty()) {
                failGatewayNotRegistered(m_ipc, m_database, commandId, uiSeq, boardSeq,
                                         commandType, gatewayId, portId, deviceId);
                return;
            }
            if (m_database.isOpen()) {
                m_database.createCommandLog(commandId,
                                            boardSeq,
                                            commandType,
                                            gatewayId,
                                            portId,
                                            deviceId,
                                            currentTimeMs());
            }

            if (m_database.isOpen() && !m_database.isGatewayPortConnected(gatewayId, portId)) {
                m_database.updateCommandLogByCommandId(commandId,
                                                       "failed",
                                                       "port_not_connected",
                                                       "gateway port is not connected",
                                                       currentTimeMs());
                m_ipc.sendMessage(buildCommandAckJson(commandId, false, "port_not_connected", uiSeq, commandType, "done", "gateway port is not connected", boardSeq, gatewayId, portId, deviceId));
                std::cout << commandType << " rejected, port not connected, gateway: "
                          << gatewayId << ", port: " << portId << std::endl;
                return;
            }

            const int slot = slotFromPortId(portId, root.IsObject() ? getJsonInt(root, "slot", getJsonInt(root, "master_slot", -1)) : -1);
            std::ostringstream payload;
            payload << "{\"type\":\"command\",\"cmd\":\"" << commandType << "\",\"cmd_id\":\"" << jsonEscape(commandId)
                    << "\",\"seq\":" << boardSeq
                    << ",\"cmdType\":\"" << commandType << "\",\"gatewayId\":\"" << jsonEscape(gatewayId)
                    << "\",\"portId\":\"" << jsonEscape(portId) << "\",\"deviceId\":" << deviceId
                    << ",\"timestampMs\":" << currentTimeMs()
                    << ",\"slot\":" << slot << ",\"slave_id\":" << deviceId;
            if (commandType == "add_device") {
                payload << ",\"device_type\":\"" << jsonEscape(deviceType) << "\""
                        << ",\"poll_interval_ms\":" << pollIntervalMs;
                payload << ",\"device\":{\"slave_id\":" << deviceId
                        << ",\"deviceId\":" << deviceId
                        << ",\"slaveAddress\":" << deviceId
                        << ",\"device_type\":\"" << jsonEscape(deviceType) << "\""
                        << ",\"deviceType\":\"" << jsonEscape(deviceType) << "\""
                        << ",\"poll_interval_ms\":" << pollIntervalMs
                        << ",\"pollIntervalMs\":" << pollIntervalMs;
                if (deviceOptions) {
                    payload << ",\"device_options\":" << jsonValueToString(*deviceOptions);
                }
                payload << "}";
                if (root.IsObject() && root.HasMember("thresholds") && root["thresholds"].IsObject()) {
                    payload << ",\"threshold_enabled\":" << (getJsonBoolAny(root, {"threshold_enabled", "thresholdEnabled"}, true) ? "true" : "false")
                            << ",\"thresholds\":" << jsonValueToString(root["thresholds"]);
                } else if (root.IsObject() && root.HasMember("threshold_config") && root["threshold_config"].IsObject()) {
                    payload << ",\"threshold_enabled\":" << (getJsonBoolAny(root, {"threshold_enabled", "thresholdEnabled"}, true) ? "true" : "false")
                            << ",\"threshold_config\":" << jsonValueToString(root["threshold_config"]);
                } else if (root.IsObject() && root.HasMember("thresholdConfig") && root["thresholdConfig"].IsObject()) {
                    payload << ",\"threshold_enabled\":" << (getJsonBoolAny(root, {"threshold_enabled", "thresholdEnabled"}, true) ? "true" : "false")
                            << ",\"thresholdConfig\":" << jsonValueToString(root["thresholdConfig"]);
                }
            }
            payload << ",\"payload\":{\"slot\":" << slot << ",\"slave_id\":" << deviceId
                    << ",\"deviceId\":" << deviceId;
            if (commandType == "add_device") {
                payload << ",\"device_type\":\"" << jsonEscape(deviceType) << "\""
                        << ",\"deviceType\":\"" << jsonEscape(deviceType) << "\""
                        << ",\"poll_interval_ms\":" << pollIntervalMs
                        << ",\"pollIntervalMs\":" << pollIntervalMs;
                if (root.IsObject() && root.HasMember("thresholds") && root["thresholds"].IsObject()) {
                    payload << ",\"thresholds\":" << jsonValueToString(root["thresholds"]);
                } else if (root.IsObject() && root.HasMember("threshold_config") && root["threshold_config"].IsObject()) {
                    payload << ",\"threshold_config\":" << jsonValueToString(root["threshold_config"]);
                } else if (root.IsObject() && root.HasMember("thresholdConfig") && root["thresholdConfig"].IsObject()) {
                    payload << ",\"thresholdConfig\":" << jsonValueToString(root["thresholdConfig"]);
                }
            }
            payload << "}}";
            const bool publishOk = m_mqtt.publish(topic, payload.str());
            if (m_database.isOpen()) {
                m_database.updateCommandLogByCommandId(commandId,
                                                       publishOk ? "sent" : "failed",
                                                       publishOk ? "" : "mqtt_publish_failed",
                                                       publishOk ? "command sent" : "mqtt publish failed",
                                                       currentTimeMs());
            }
            if (publishOk) {
                rememberPendingCommand(m_dataService, commandId, uiSeq, boardSeq, commandType, gatewayId, portId, deviceId);
            }
            m_ipc.sendMessage(buildCommandAckJson(commandId,
                                               publishOk,
                                               publishOk ? "" : "mqtt_publish_failed",
                                               uiSeq,
                                               commandType,
                                               publishOk ? "sent" : "done",
                                               publishOk ? "command published to gateway" : "MQTT publish failed",
                                               boardSeq,
                                               gatewayId,
                                               portId,
                                               deviceId));
            std::cout << commandType << " publish "
                      << (publishOk ? "ok" : "failed")
                      << ", topic: " << topic
                      << ", cmd_id: " << commandId << std::endl;
            std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
                      << " topic=" << topic
                      << " cmd=" << commandType
                      << " seq=" << boardSeq << std::endl;
            return;
        }

        const std::int64_t seq = root.IsObject() ? sequenceFrom(root) : 0;
        const std::int64_t boardSeq = linuxDataSeqFrom(seq);
        if (gatewayId.empty()) {
            m_ipc.sendMessage(buildCommandAckJson(commandIdForAck, false, "invalid_request", seq, commandType, "done", "gateway id is required", boardSeq, gatewayId, portId, 0));
            std::cout << "command rejected, missing gateway, cmd_id: " << cmdId << std::endl;
            return;
        }
        const std::string topic = commandTopicFor(commandType, gatewayId, portId);
        if (topic.empty()) {
            failGatewayNotRegistered(m_ipc, m_database, commandIdForAck, seq, boardSeq,
                                     commandType, gatewayId, portId, 0);
            return;
        }
        std::ostringstream payload;
        payload << "{\"type\":\"command\",\"cmd\":\"" << jsonEscape(commandType)
                << "\",\"cmd_id\":\"" << jsonEscape(commandIdForAck)
                << "\",\"seq\":" << boardSeq
                << ",\"cmdType\":\"" << jsonEscape(commandType)
                << "\",\"commandType\":\"" << jsonEscape(commandType)
                << "\",\"gatewayId\":\"" << jsonEscape(gatewayId) << "\"";
        if (!portId.empty()) {
            payload << ",\"portId\":\"" << jsonEscape(portId) << "\"";
        }
        if (root.IsObject() && root.HasMember("target")) {
            payload << ",\"target\":" << jsonValueToString(root["target"]);
        }
        if (root.IsObject() && root.HasMember("payload")) {
            payload << ",\"payload\":" << jsonValueToString(root["payload"]);
        }
        payload << "}";

        if (m_database.isOpen()) {
            m_database.createCommandLog(commandIdForAck, boardSeq, commandType, gatewayId, portId, 0, currentTimeMs());
        }
        const bool publishOk = m_mqtt.publish(topic, payload.str());
        if (m_database.isOpen()) {
            m_database.updateCommandLogByCommandId(commandIdForAck,
                                                   publishOk ? "sent" : "failed",
                                                   publishOk ? "" : "mqtt_publish_failed",
                                                   publishOk ? "command sent" : "mqtt publish failed",
                                                   currentTimeMs());
        }
        if (publishOk) {
            rememberPendingCommand(m_dataService, commandIdForAck, seq, boardSeq,
                                   commandType, gatewayId, portId, 0);
        }
        m_ipc.sendMessage(buildCommandAckJson(commandIdForAck,
                                               publishOk,
                                               publishOk ? "" : "mqtt_publish_failed",
                                               seq,
                                               commandType,
                                               publishOk ? "sent" : "done",
                                               publishOk ? "command published to gateway" : "MQTT publish failed",
                                               boardSeq,
                                               gatewayId,
                                               portId,
                                               0));
        std::cout << commandType << " publish " << (publishOk ? "ok" : "failed")
                  << ", topic: " << topic << ", cmd_id: " << commandIdForAck << std::endl;
        std::cout << "[MQTT TX CMD] gatewayId=" << gatewayId
                  << " topic=" << topic
                  << " cmd=" << commandType
                  << " seq=" << boardSeq << std::endl;
    } else {
        m_ipc.sendMessage(buildCommandAckJson("", false, "unsupported_command", 0, "unsupported", "done", "unsupported command", 0, "", "", 0));

        std::cout << "send unsupported ack done" << std::endl;
    }
}
