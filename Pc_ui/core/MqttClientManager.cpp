#include "MqttClientManager.h"

#include <QMqttTopicFilter>
#include <QMqttTopicName>
#include <QDebug>

MqttClientManager::MqttClientManager(QObject *parent) : QObject(parent)
{
    m_client = new QMqttClient(this);

    connect(m_client, &QMqttClient::connected, this, [this]() {
        qDebug() << "[MQTT] 连接成功";
        qDebug() << "[MQTT] Host:" << m_config.host;
        qDebug() << "[MQTT] Port:" << m_config.port;
        qDebug() << "[MQTT] ClientId:" << m_config.clientId;

        subscribeDefaultTopics();

        emit connected();
    });

    connect(m_client, &QMqttClient::disconnected, this, [this]() {
        qDebug() << "[MQTT] 已断开连接";
        emit disconnected();
    });

    connect(m_client, &QMqttClient::messageReceived, this,
            [this](const QByteArray &message, const QMqttTopicName &topic) {
                qDebug() << "[MQTT] 收到消息";
                qDebug() << "[MQTT] Topic:" << topic.name();
                qDebug() << "[MQTT] Payload:" << message;

                emit messageArrived(topic.name(), message);
            });

    connect(m_client, &QMqttClient::stateChanged, this,
            [](QMqttClient::ClientState state) {
                qDebug() << "[MQTT] 状态变化:" << state;
            });

    connect(m_client, &QMqttClient::errorChanged, this,
            [this](QMqttClient::ClientError error) {
                if (error == QMqttClient::NoError) {
                    return;
                }

                qDebug() << "[MQTT] 连接错误，错误码:" << error;
                emit mqttError(QStringLiteral("MQTT 错误码: %1").arg(error));
            });
}

bool MqttClientManager::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

void MqttClientManager::connectToBroker(const MqttConfig &config)
{
    m_config = config;

    qDebug() << "[MQTT] 点击测试连接，准备连接 Broker";
    qDebug() << "[MQTT] Host:" << config.host;
    qDebug() << "[MQTT] Port:" << config.port;
    qDebug() << "[MQTT] ClientId:" << config.clientId;
    qDebug() << "[MQTT] Username:" << config.username;

    m_client->setHostname(config.host);
    m_client->setPort(config.port);
    m_client->setClientId(config.clientId);

    if (!config.username.isEmpty()) {
        m_client->setUsername(config.username);
    }

    if (!config.password.isEmpty()) {
        m_client->setPassword(config.password);
    }

    m_client->connectToHost();

    qDebug() << "[MQTT] connectToHost() 已调用，正在等待连接结果...";
}

void MqttClientManager::disconnectFromBroker()
{
    qDebug() << "[MQTT] 主动断开 MQTT";
    m_client->disconnectFromHost();
}

void MqttClientManager::subscribeDefaultTopics()
{
    QStringList topics = {
        // "factory/+/area/+/gateway/+/telemetry",
        // "factory/+/area/+/gateway/+/status",
        // "factory/+/area/+/gateway/+/alarm",
        // "factory/+/area/+/gateway/+/heartbeat",
        // "factory/+/area/+/gateway/+/command_ack"
        "imx6ull/gpio/buzzer/set"
    };

    qDebug() << "[MQTT] 开始订阅默认主题";

    for (const auto &t : topics) {
        auto *sub = m_client->subscribe(QMqttTopicFilter(t), 1);

        if (sub) {
            qDebug() << "[MQTT] 订阅成功:" << t;
        } else {
            qDebug() << "[MQTT] 订阅失败:" << t;
            emit mqttError(QStringLiteral("MQTT 订阅失败: %1").arg(t));
        }
    }
}

void MqttClientManager::publishMessage(const QString &topic,
                                       const QByteArray &payload,
                                       int qos,
                                       bool retain)
{
    if (!isConnected()) {
        qDebug() << "[MQTT] 发布失败，当前未连接";
        emit mqttError(QStringLiteral("MQTT 未连接，无法发布消息"));
        return;
    }

    auto result = m_client->publish(QMqttTopicName(topic), payload, qos, retain);

    if (result == -1) {
        qDebug() << "[MQTT] 发布失败:" << topic;
        emit mqttError(QStringLiteral("MQTT 发布失败: %1").arg(topic));
        return;
    }

    qDebug() << "[MQTT] 发布成功:" << topic;
    qDebug() << "[MQTT] Payload:" << payload;
}