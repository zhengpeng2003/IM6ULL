#ifndef IPC_MESSAGE_HANDLER_HPP
#define IPC_MESSAGE_HANDLER_HPP

#include <string>

#include "mqtt/MqttConfig.hpp"

class IpcServer;
class MqttClient;
class PcDataService;
class PcDatabase;

class IpcMessageHandler
{
public:
    IpcMessageHandler(PcDatabase& database,
                      PcDataService& dataService,
                      IpcServer& ipc,
                      MqttClient& mqtt,
                      MqttConfig& mqttConfig);

    void handle(const std::string& msg);

private:
    PcDatabase& m_database;
    PcDataService& m_dataService;
    IpcServer& m_ipc;
    MqttClient& m_mqtt;
    MqttConfig& m_mqttConfig;
};

#endif // IPC_MESSAGE_HANDLER_HPP
