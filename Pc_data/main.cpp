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
        cout << "MqttClient created" << endl;

        MqttConfig mqttConfig = loadMqttConfig();
        cout << "MqttConfig loaded: broker=" << mqttConfig.host << ":" << mqttConfig.port
             << ", clientId=" << mqttConfig.clientId
             << ", commandGatewayId=" << mqttConfig.commandGatewayId
             << ", topicCount=" << mqttConfig.topics.size()
             << endl;

        MqttMessageHandler mqttHandler(database, dataService, ipc, mqtt);
        cout << "MqttMessageHandler created" << endl;

        IpcMessageHandler ipcHandler(database, dataService, ipc, mqtt, mqttConfig);
        cout << "IpcMessageHandler created" << endl;

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
        });

        ipc.setClientDisconnectedCallback([]() {
            cout << "Pc_ui disconnected" << endl;
        });

        ipc.setMessageCallback([&](const std::string& msg) {
            ipcHandler.handle(msg);
        });
        cout << "callbacks installed" << endl;

        cout << "Before ipc.start" << endl;

        if (!ipc.start()) {
            cout << "IpcServer start failed" << endl;
            return -1;
        }

        cout << "After ipc.start" << endl;
        cout << "Pc_data IPC server running..." << endl;

        cout << "MQTT broker: " << mqttConfig.host << ":" << mqttConfig.port << endl;
        for (const std::string& topic : mqttConfig.topics) {
            cout << "MQTT subscribe topic: " << topic << endl;
        }

        bool pendingStartupGatewayDiscovery = true;
        if (database.isOpen() && database.deviceCount() == 0) {
            cout << "Pc_data device table empty, wait for gateway_register or broadcast discovery" << endl;
        }

        cout << "Before mqtt.connectToBroker" << endl;
        if (!mqtt.connectToBroker(mqttConfig.host, mqttConfig.port, mqttConfig.clientId, mqttConfig.topics)) {
            cout << "MQTT connectToBroker call failed" << endl;
        } else {
            cout << "MQTT async connect request sent, status=" << mqtt.status() << endl;
            cout << "MQTT not connected yet, defer gateway discovery broadcast" << endl;
        }

        std::int64_t lastOfflineScanMs = 0;
        while (true) {
            const std::int64_t nowMs = currentTimeMs();
            if (pendingStartupGatewayDiscovery && mqtt.status() == "connected") {
                cout << "MQTT connected, broadcast discover_gateways" << endl;
                const std::int64_t seq = currentTimeMs();
                const std::string payload = std::string("{\"type\":\"command\",\"cmd\":\"discover_gateways\",\"seq\":") +
                    std::to_string(seq) + "}";
                const bool requestOk = mqtt.publish("gateway/broadcast/down", payload);
                cout << "discover_gateways broadcast " << (requestOk ? "ok" : "failed") << endl;
                pendingStartupGatewayDiscovery = !requestOk;
            }

            if (nowMs - lastOfflineScanMs >= 1000) {
                lastOfflineScanMs = nowMs;
                if (database.isOpen()) {
                    const int offlineChanged = database.markOfflineDevices(nowMs, 30000);
                    if (offlineChanged > 0 && ipc.hasClient()) {
                        sendDevicesSnapshot(ipc, database);
                    }
                    const int staleGatewayChanged = database.markStaleGateways(nowMs, 30000);
                    if (staleGatewayChanged > 0 && ipc.hasClient()) {
                        sendGatewayStatusSnapshot(ipc, database);
                    }
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
                const std::vector<PendingCommandTarget> commandTimeouts =
                    dataService.collectCommandTimeouts(nowMs, 8000);
                for (const PendingCommandTarget& pending : commandTimeouts) {
                    if (database.isOpen()) {
                        database.updateCommandLogBySeq(pending.boardSeq,
                                                       "timeout",
                                                       "linux_data_ack_timeout",
                                                       "device execution timeout",
                                                       nowMs);
                    }
                    if (ipc.hasClient()) {
                        CommandLogTarget target;
                        target.commandId = pending.commandId;
                        target.seq = pending.uiSeq;
                        target.commandType = pending.commandType;
                        target.gatewayId = pending.gatewayId;
                        target.portId = pending.portId;
                        target.deviceId = pending.deviceId;
                        ipc.sendMessage(buildCommandLogUpdateJson(pending.uiSeq,
                                                                  pending.commandType,
                                                                  "timeout",
                                                                  "linux_data_ack_timeout",
                                                                  "device execution timeout",
                                                                  &target,
                                                                  pending.boardSeq));
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
