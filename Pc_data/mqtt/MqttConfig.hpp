#ifndef MQTT_CONFIG_HPP
#define MQTT_CONFIG_HPP

#include <string>
#include <vector>

struct MqttConfig
{
    std::string host = "127.0.0.1";
    int port = 1883;
    std::string clientId = "pc_data_001";
    std::string commandGatewayId;
    std::vector<std::string> topics = {"gateway/register", "gateway/+/up", "pc_data/telemetry/test"};
};

MqttConfig loadMqttConfig();
bool saveMqttConfigFile(const MqttConfig& config);

#endif // MQTT_CONFIG_HPP
