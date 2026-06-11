#ifndef MQTT_MESSAGE_HANDLER_HPP
#define MQTT_MESSAGE_HANDLER_HPP

#include <string>

class IpcServer;
class MqttClient;
class PcDataService;
class PcDatabase;

class MqttMessageHandler
{
public:
    MqttMessageHandler(PcDatabase& database,
                       PcDataService& dataService,
                       IpcServer& ipc,
                       MqttClient& mqtt);

    void handle(const std::string& topic, const std::string& payload);

private:
    PcDatabase& m_database;
    PcDataService& m_dataService;
    IpcServer& m_ipc;
    MqttClient& m_mqtt;
};

#endif // MQTT_MESSAGE_HANDLER_HPP
