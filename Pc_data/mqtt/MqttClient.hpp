#ifndef MQTT_CLIENT_HPP
#define MQTT_CLIENT_HPP

#include <functional>
#include <string>
#include <vector>

#include "MQTTAsync.h"

class MqttClient
{
public:
    using MessageCallback = std::function<void(const std::string& topic,
                                               const std::string& payload)>;

public:
    MqttClient();
    ~MqttClient();

    bool connectToBroker(const std::string& host,
                         int port,
                         const std::string& clientId,
                         const std::vector<std::string>& topics);

    void disconnect();

    void setMessageCallback(MessageCallback callback);

private:
    static void onConnectionLost(void* context, char* cause);
    static int onMessageArrived(void* context,
                                char* topicName,
                                int topicLen,
                                MQTTAsync_message* message);

    static void onConnectSuccess(void* context, MQTTAsync_successData* response);
    static void onConnectFailure(void* context, MQTTAsync_failureData* response);

    bool subscribeTopics();

private:
    MQTTAsync m_client = nullptr;

    std::string m_address;
    std::string m_clientId;
    std::vector<std::string> m_topics;

    MessageCallback m_messageCallback;
};

#endif // MQTT_CLIENT_HPP