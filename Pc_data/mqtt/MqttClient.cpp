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

    std::cout << "MQTTAsync_create begin, address=" << m_address
              << ", clientId=" << m_clientId
              << ", topicCount=" << m_topics.size()
              << std::endl;
    for (const std::string& topic : m_topics) {
        std::cout << "MQTT will subscribe topic: " << topic << std::endl;
    }

    int rc = MQTTAsync_create(
        &m_client,
        m_address.c_str(),
        m_clientId.c_str(),
        MQTTCLIENT_PERSISTENCE_NONE,
        nullptr
        );

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_create failed, rc=" << rc
                  << ", address=" << m_address
                  << ", clientId=" << m_clientId
                  << std::endl;
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    std::cout << "MQTTAsync_create ok, address=" << m_address
              << ", clientId=" << m_clientId
              << std::endl;

    rc = MQTTAsync_setCallbacks(
        m_client,
        this,
        &MqttClient::onConnectionLost,
        &MqttClient::onMessageArrived,
        nullptr
        );

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_setCallbacks failed, rc=" << rc
                  << ", address=" << m_address
                  << ", clientId=" << m_clientId
                  << std::endl;
        MQTTAsync_destroy(&m_client);
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    std::cout << "MQTTAsync_setCallbacks ok, address=" << m_address
              << ", clientId=" << m_clientId
              << std::endl;

    MQTTAsync_connectOptions connOpts = MQTTAsync_connectOptions_initializer;
    connOpts.keepAliveInterval = 20;
    connOpts.cleansession = 1;
    connOpts.automaticReconnect = 1;

    connOpts.onSuccess = &MqttClient::onConnectSuccess;
    connOpts.onFailure = &MqttClient::onConnectFailure;
    connOpts.context = this;

    rc = MQTTAsync_connect(m_client, &connOpts);

    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTTAsync_connect failed, rc=" << rc
                  << ", address=" << m_address
                  << ", clientId=" << m_clientId
                  << std::endl;
        MQTTAsync_destroy(&m_client);
        m_client = nullptr;
        setStatus("failed");
        return false;
    }

    std::cout << "MQTTAsync_connect request sent, rc=" << rc
              << ", address=" << m_address
              << ", clientId=" << m_clientId
              << ", status=" << status()
              << std::endl;
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

bool MqttClient::publish(const std::string& topic, const std::string& payload, int qos)
{
    if (!m_client || topic.empty()) {
        std::cerr << "MQTT publish skipped, clientReady=" << (m_client ? "true" : "false")
                  << ", topic=" << topic
                  << ", status=" << status()
                  << std::endl;
        return false;
    }

    MQTTAsync_message message = MQTTAsync_message_initializer;
    message.payload = const_cast<char*>(payload.c_str());
    message.payloadlen = static_cast<int>(payload.size());
    message.qos = qos;
    message.retained = 0;

    int rc = MQTTAsync_sendMessage(m_client, topic.c_str(), &message, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
        std::cerr << "MQTT publish failed, topic="
                  << topic
                  << ", rc="
                  << rc
                  << ", status="
                  << status()
                  << ", payload bytes="
                  << payload.size()
                  << std::endl;
        return false;
    }

    std::cout << "MQTT publish requested, topic=" << topic
              << ", qos=" << qos
              << ", payload bytes=" << payload.size()
              << ", status=" << status()
              << std::endl;

    return true;
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

    std::cout << "MQTT message arrived. topic="
              << topic
              << ", payload bytes="
              << payload.size()
              << std::endl;

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

    if (self) {
        self->setStatus("connected");
        std::cout << "MQTT connected, address=" << self->m_address
                  << ", clientId=" << self->m_clientId
                  << ", topicCount=" << self->m_topics.size()
                  << std::endl;
        self->subscribeTopics();
    } else {
        std::cout << "MQTT connected." << std::endl;
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
        std::cerr << "MQTT subscribe skipped, client is null, status="
                  << status()
                  << std::endl;
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
                      << ", address="
                      << m_address
                      << ", clientId="
                      << m_clientId
                      << ", status="
                      << status()
                      << std::endl;
            ok = false;
        } else {
            if (topic == "pc_data/telemetry/test") {
                std::cout << "[MQTT SUB][LEGACY] " << topic << std::endl;
            } else {
                std::cout << "[MQTT SUB] " << topic << std::endl;
            }
            std::cout << "MQTT subscribe requested, topic="
                      << topic
                      << ", qos="
                      << qos
                      << ", address="
                      << m_address
                      << ", clientId="
                      << m_clientId
                      << ", status="
                      << status()
                      << std::endl;
        }
    }

    return ok;
}
