#include "MqttClient.hpp"

#include <iostream>

MqttClient::MqttClient()
{
}

MqttClient::~MqttClient()
{
    disconnect();
}

bool MqttClient::connectToBroker(const std::string& host,
                                 int port,
                                 const std::string& clientId,
                                 const std::vector<std::string>& topics)
{
    disconnect();

    m_address = "tcp://" + host + ":" + std::to_string(port);
    m_clientId = clientId;
    m_topics = topics;
    setStatus("connecting");

    int rc = MQTTAsync_create(
        &m_client,
        m_address.c_str(),
        m_clientId.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE,
        nullptr
        );

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_create failed, rc=" << rc << std::endl;
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    rc = MQTTAsync_setCallbacks(
        m_client,
        this,
        &MqttClient::onConnectionLost,
        &MqttClient::onMessageArrived,
        nullptr
        );

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_setCallbacks failed, rc=" << rc << std::endl;
        MQTTAsync_destroy(&m_client);
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    MQTTAsync_connectOptions connOpts = MQTTAsync_connectOptions_initializer;
    connOpts.keepAliveInterval = 20;
    connOpts.cleansession = 1;
    connOpts.automaticReconnect = 1;

    connOpts.onSuccess = &MqttClient::onConnectSuccess;
    connOpts.onFailure = &MqttClient::onConnectFailure;
    connOpts.context = this;

    rc = MQTTAsync_connect(m_client, &connOpts);

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_connect failed, rc=" << rc << std::endl;
        MQTTAsync_destroy(&m_client);
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    std::cout << "MQTT connecting to " << m_address << std::endl;
    return true;
}

void MqttClient::disconnect()
{
    if (!m_client) {
        return;
    }

    MQTTAsync_disconnectOptions discOpts = MQTTAsync_disconnectOptions_initializer;
    MQTTAsync_disconnect(m_client, &discOpts);

    MQTTAsync_destroy(&m_client);
    m_client = nullptr;
    setStatus("disconnected");

    std::cout << "MQTT disconnected." << std::endl;
}

std::string MqttClient::status() const
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    return m_status;
}

void MqttClient::setStatus(const std::string& status)
{
    std::lock_guard<std::mutex> lock(m_statusMutex);
    m_status = status;
}

void MqttClient::setMessageCallback(MessageCallback callback)
{
    m_messageCallback = std::move(callback);
}

void MqttClient::onConnectionLost(void* context, char* cause)
{
    auto* self = static_cast<MqttClient*>(context);

    std::cerr << "MQTT connection lost.";

    if (cause) {
        std::cerr << " cause: " << cause;
    }

    std::cerr << std::endl;
    if (self) {
        self->setStatus("connecting");
    }

    /*
     * 这里不用手动重连。
     * connectOptions 里面已经设置 automaticReconnect = 1。
     */
    (void)self;
}

int MqttClient::onMessageArrived(void* context,
                                 char* topicName,
                                 int topicLen,
                                 MQTTAsync_message* message)
{
    auto* self = static_cast<MqttClient*>(context);

    std::string topic;

    if (topicName) {
        if (topicLen > 0) {
            topic.assign(topicName, topicLen);
        } else {
            topic = topicName;
        }
    }

    std::string payload;

    if (message && message->payload && message->payloadlen > 0) {
        payload.assign(
            static_cast<const char*>(message->payload),
            static_cast<size_t>(message->payloadlen)
            );
    }

    std::cout << "MQTT message arrived." << std::endl;
    std::cout << "Topic: " << topic << std::endl;
    std::cout << "Payload: " << payload << std::endl;

    if (self && self->m_messageCallback) {
        self->m_messageCallback(topic, payload);
    }

    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topicName);

    return 1;
}

void MqttClient::onConnectSuccess(void* context, MQTTAsync_successData* response)
{
    auto* self = static_cast<MqttClient*>(context);

    std::cout << "MQTT connected." << std::endl;

    if (self) {
        self->setStatus("connected");
        self->subscribeTopics();
    }

    (void)response;
}

void MqttClient::onConnectFailure(void* context, MQTTAsync_failureData* response)
{
    std::cerr << "MQTT connect failed.";

    if (response) {
        std::cerr << " code=" << response->code;

        if (response->message) {
            std::cerr << " message=" << response->message;
        }
    }

    std::cerr << std::endl;

    auto* self = static_cast<MqttClient*>(context);
    if (self) {
        self->setStatus("failed");
    }
}

bool MqttClient::subscribeTopics()
{
    if (!m_client) {
        return false;
    }

    bool ok = true;

    for (const auto& topic : m_topics) {
        int qos = 1;

        int rc = MQTTAsync_subscribe(
            m_client,
            topic.c_str(),
            qos,
            nullptr
            );

        if (rc != MQTTASYNC_SUCCESS) {
            std::cerr << "MQTT subscribe failed, topic="
                      << topic
                      << ", rc="
                      << rc
                      << std::endl;
            ok = false;
        } else {
            std::cout << "MQTT subscribed: " << topic << std::endl;
        }
    }

    return ok;
}
