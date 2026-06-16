#include <chrono>
#include <cstdint>
#include <exception>
#include <iostream>
#include <thread>

#include "common/JsonUtils.hpp"
#include "ipc/IpcMessageHandler.hpp"
#include "ipc/IpcServer.hpp"
#include "ipc/PcUiPublisher.hpp"
#include "mqtt/MqttClient.hpp"
#include "mqtt/MqttConfig.hpp"
#include "mqtt/MqttMessageHandler.hpp"
#include "protocol/PcDataMessages.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

using namespace std;

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

        MqttMessageHandler mqttHandler(database, dataService, ipc, mqtt);
        IpcMessageHandler ipcHandler(database, dataService, ipc, mqtt, mqttConfig);

        mqtt.setMessageCallback([&](const std::string& topic,
                                    const std::string& payload) {
            mqttHandler.handle(topic, payload);
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
            ipcHandler.handle(msg);
        });

        cout << "Before ipc.start" << endl;

        if (!ipc.start()) {
            cout << "IpcServer start failed" << endl;
            return -1;
        }

        cout << "Pc_data IPC server running..." << endl;

        cout << "MQTT broker: " << mqttConfig.host << ":" << mqttConfig.port << endl;
        cout << "MQTT subscribe topic: pc_data/telemetry/test" << endl;

        bool pendingStartupConfigRequest = database.isOpen() && database.deviceCount() == 0;
        if (pendingStartupConfigRequest) {
            cout << "Pc_data device table empty, need_config_sync=true" << endl;
        }
        if (!mqtt.connectToBroker(mqttConfig.host, mqttConfig.port, mqttConfig.clientId, mqttConfig.topics)) {
            cout << "MQTT connectToBroker call failed" << endl;
        } else if (pendingStartupConfigRequest) {
            cout << "MQTT not connected, defer request_config_snapshot gateway=gateway_001" << endl;
        }

        std::int64_t lastOfflineScanMs = 0;
        while (true) {
            const std::int64_t nowMs = currentTimeMs();
            if (pendingStartupConfigRequest && mqtt.status() == "connected") {
                cout << "MQTT connected, flush pending request_config_snapshot gateway=gateway_001" << endl;
                const std::int64_t seq = currentTimeMs();
                const std::string payload = std::string("{\"type\":\"command\",\"cmd\":\"get_config\",\"seq\":") +
                    std::to_string(seq) + ",\"target\":{\"gatewayId\":\"gateway_001\"}}";
                const bool requestOk = mqtt.publish("cmd/gateway_001", payload);
                cout << "request_config_snapshot publish " << (requestOk ? "ok" : "failed")
                     << ", gateway=gateway_001" << endl;
                pendingStartupConfigRequest = !requestOk;
            }

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
                const std::vector<SyncConfigResult> syncTimeouts =
                    dataService.collectSyncConfigTimeouts(nowMs, 5000);
                for (const SyncConfigResult& result : syncTimeouts) {
                    if (ipc.hasClient()) {
                        ipc.sendMessage(buildSyncConfigResultJson(result.success,
                                                                  result.message,
                                                                  result.portCount,
                                                                  result.deviceCount));
                    }
                }
                const std::vector<CommandLogTarget> commandTimeouts =
                    database.collectCommandTimeouts(nowMs, 8000);
                for (const CommandLogTarget& target : commandTimeouts) {
                    if (ipc.hasClient()) {
                        ipc.sendMessage(buildCommandLogUpdateJson(target.seq,
                                                                  target.commandType,
                                                                  "timeout",
                                                                  "linux_data_ack_timeout",
                                                                  "device execution timeout",
                                                                  &target));
                    }
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
