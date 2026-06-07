#include "MqttClientManager.h"
#include <QMqttTopicFilter>
#include <QMqttTopicName>

MqttClientManager::MqttClientManager(QObject *parent) : QObject(parent)
{
    m_client = new QMqttClient(this);
    connect(m_client, &QMqttClient::connected, this, [this]() {
        subscribeDefaultTopics();
        emit connected();
    });
    connect(m_client, &QMqttClient::disconnected, this, &MqttClientManager::disconnected);
    connect(m_client, &QMqttClient::messageReceived, this,
            [this](const QByteArray &message, const QMqttTopicName &topic) {
        emit messageArrived(topic.name(), message);
    });
}

bool MqttClientManager::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

void MqttClientManager::connectToBroker(const MqttConfig &config)
{
    m_config = config;
    m_client->setHostname(config.host);
    m_client->setPort(config.port);
    m_client->setClientId(config.clientId);
    if (!config.username.isEmpty()) m_client->setUsername(config.username);
    if (!config.password.isEmpty()) m_client->setPassword(config.password);
    m_client->connectToHost();
}

void MqttClientManager::disconnectFromBroker()
{
    m_client->disconnectFromHost();
}

void MqttClientManager::subscribeDefaultTopics()
{
    QStringList topics = {
        "factory/+/area/+/gateway/+/telemetry",
        "factory/+/area/+/gateway/+/status",
        "factory/+/area/+/gateway/+/alarm",
        "factory/+/area/+/gateway/+/heartbeat",
        "factory/+/area/+/gateway/+/command_ack"
    };
    for (const auto &t : topics) {
        m_client->subscribe(QMqttTopicFilter(t), 1);
    }
}

void MqttClientManager::publishMessage(const QString &topic, const QByteArray &payload, int qos, bool retain)
{
    if (!isConnected()) {
        emit mqttError(QStringLiteral("MQTT 未连接，无法发布消息"));
        return;
    }
    m_client->publish(QMqttTopicName(topic), payload, qos, retain);
}
