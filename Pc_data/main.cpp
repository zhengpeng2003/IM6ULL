#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>
#include <exception>

#include "ipc/IpcServer.hpp"
#include "service/PcDataService.hpp"

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

int main()
{
    try {
        cout << "Pc_data start" << endl;

        PcDataService dataService;
        cout << "PcDataService created" << endl;

        /*
         * 当前阶段先用模拟数据。
         * 后面接 MQTT 后，就不是 generateMockData()，
         * 而是 MqttClient 收到数据后调用：
         *
         * dataService.handleTelemetryPack(pack);
         */
        dataService.generateMockData();
        cout << "generateMockData ok" << endl;

        std::vector<TelemetryPoint> testPoints = dataService.getLatestPoints();
        cout << "PcDataService mock point count: " << testPoints.size() << endl;

        cout << "Before create IpcServer" << endl;

        IpcServer ipc(R"(\\.\pipe\PcDataIpcPipe)");

        cout << "IpcServer created" << endl;

        ipc.setClientConnectedCallback([&]() {
            cout << "Pc_ui connected" << endl;

            /*
             * 注意：
             * 你的 IpcServer::sendMessage() 返回值是 void，
             * 所以这里只能直接调用，不能写 bool ok = ...
             */
            ipc.sendMessage(R"({"type":"hello","message":"hello pc_ui"})");

            cout << "send hello done" << endl;
        });

        ipc.setClientDisconnectedCallback([]() {
            cout << "Pc_ui disconnected" << endl;
        });

        ipc.setMessageCallback([&](const std::string& msg) {
            cout << "Pc_data recv: " << msg << endl;

            /*
             * 兼容你之前 Pc_ui 可能发送的 get_snapshot。
             * 后面建议统一改成 get_latest_points。
             */
            if (msg.find("get_latest_points") != std::string::npos ||
                msg.find("get_snapshot") != std::string::npos) {

                std::vector<TelemetryPoint> points = dataService.getLatestPoints();

                cout << "latest point count: " << points.size() << endl;

                std::string json = buildLatestPointsJson(points);

                cout << "json build ok, size: " << json.size() << endl;

                ipc.sendMessage(json);

                cout << "send latest_points done" << endl;
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