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

        if (database.isOpen()) {
            dataService.updateGatewayStatuses(database.queryGatewayStatuses());
            dataService.rememberGatewayPorts(database.queryGatewayPorts());
            dataService.rememberDeviceRegistries(database.queryDevices());
            dataService.rememberPointConfigs(database.queryPointConfigs());
            dataService.handleTelemetryPoints(database.queryLatestPoints());
            cout << "PcDataService registry cache seeded from database" << endl;
        }

        IpcMessageHandler ipcHandler(database, dataService, ipc, mqtt, mqttConfig);
        cout << "IpcMessageHandler created" << endl;

        mqtt.setMessageCallback([&](const std::string& topic,
                                    const std::string& payload) {
            mqttHandler.handle(topic, payload);
        });

        ipc.setClientConnectedCallback([&]() {
            cout << "Pc_ui connected" << endl;

            // Initial snapshots do not need to branch on send status; latest_points debug paths log it.
            ipc.sendMessage(R"({"type":"hello","message":"hello pc_ui"})");
            sendGatewayStatusSnapshot(ipc, dataService);
            sendPortStatusSnapshot(ipc, dataService);
            sendDevicesSnapshot(ipc, dataService);
            sendLatestPoints(ipc, dataService);

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

        if (database.isOpen() && database.deviceCount() == 0) {
            cout << "Pc_data device table empty, wait for gateway_register" << endl;
        }

        cout << "Before mqtt.connectToBroker" << endl;
        if (!mqtt.connectToBroker(mqttConfig.host, mqttConfig.port, mqttConfig.clientId, mqttConfig.topics)) {
            cout << "MQTT connectToBroker call failed" << endl;
        } else {
            cout << "MQTT async connect request sent, status=" << mqtt.status() << endl;
            cout << "MQTT not connected yet, wait for gateway_register" << endl;
        }

        std::int64_t lastOfflineScanMs = 0;
        while (true) {
            const std::int64_t nowMs = currentTimeMs();
            if (nowMs - lastOfflineScanMs >= 1000) {
                lastOfflineScanMs = nowMs;
                if (database.isOpen()) {
                    const int offlineChanged = database.markOfflineDevices(nowMs, 30000);
                    if (offlineChanged > 0 && ipc.hasClient()) {
                        sendDevicesSnapshot(ipc, dataService);
                    }
                    const int staleGatewayChanged = database.markStaleGateways(nowMs, 30000);
                    if (staleGatewayChanged > 0 && ipc.hasClient()) {
                        sendGatewayStatusSnapshot(ipc, dataService);
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
                const std::vector<PendingCommandTarget> commandSoftTimeouts =
                    dataService.collectCommandSoftTimeouts(nowMs, 10000);
                for (const PendingCommandTarget& pending : commandSoftTimeouts) {
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
                                                                  "waiting",
                                                                  "waiting_linux_data_ack",
                                                                  "waiting for Linux_data final ack",
                                                                  &target,
                                                                  pending.boardSeq));
                    }
                }

                const std::vector<PendingCommandTarget> commandHardTimeouts =
                    dataService.collectCommandHardTimeouts(nowMs, 30000);
                for (const PendingCommandTarget& pending : commandHardTimeouts) {
                    if (database.isOpen()) {
                        database.updateCommandLogByCommandId(pending.commandId,
                                                             "timeout",
                                                             "linux_data_ack_timeout",
                                                             "device execution timeout, keep waiting final ack",
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
                                                                  "device execution timeout, keep waiting final ack",
                                                                  &target,
                                                                  pending.boardSeq));
                    }
                    cout << "[CMD TIMEOUT] keep pending for late final ack"
                         << " cmd_id=" << pending.commandId
                         << " cmd=" << pending.commandType
                         << " uiSeq=" << pending.uiSeq
                         << " boardSeq=" << pending.boardSeq
                         << endl;
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
