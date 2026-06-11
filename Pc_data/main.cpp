#include <iostream>
#include <thread>
#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>
#include <exception>

#include <rapidjson/document.h>

#include "ipc/IpcServer.hpp"
#include "mqtt/MqttClient.hpp"
#include "model/ModelConverter.hpp"
#include "model/PointConfigPackParser.hpp"
#include "model/TelemetryPackParser.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

using namespace std;

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
        mqtt.setMessageCallback([&](const std::string& topic,
                                    const std::string& payload) {
            cout << "[MQTT RX] topic: " << topic << endl;
            cout << "[MQTT RX] payload: " << payload << endl;

            const std::string messageType = extractJsonStringValue(payload, "type");
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

            /*
             * 兼容你之前 Pc_ui 可能发送的 get_snapshot。
             * 后面建议统一改成 get_latest_points。
             */
            if (msg.find("get_latest_points") != std::string::npos ||
                msg.find("get_snapshot") != std::string::npos) {

                sendLatestPoints(ipc, dataService, database);
            } else if (msg.find("\"type\":\"command\"") != std::string::npos ||
                       msg.find("\"msg_type\":\"command\"") != std::string::npos) {
                const std::string cmdId = extractJsonStringValue(msg, "cmd_id");
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

        const std::vector<std::string> mqttTopics = {
            "pc_data/telemetry/test"
        };

        cout << "MQTT default broker: 127.0.0.1:1883" << endl;
        cout << "MQTT default subscribe topic: pc_data/telemetry/test" << endl;

        if (!mqtt.connectToBroker("127.0.0.1", 1883, "pc_data_001", mqttTopics)) {
            cout << "MQTT connectToBroker call failed" << endl;
        }

        while (true) {
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
