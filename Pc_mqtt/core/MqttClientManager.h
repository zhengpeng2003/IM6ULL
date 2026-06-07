#pragma once
#include <QObject>
#include <QMqttClient>
#include "model/ConfigModel.h"

class MqttClientManager : public QObject
{
    Q_OBJECT
public:
    explicit MqttClientManager(QObject *parent = nullptr);
    bool isConnected() const;

public slots:
    void connectToBroker(const MqttConfig &config);
    void disconnectFromBroker();
    void subscribeDefaultTopics();
    void publishMessage(const QString &topic, const QByteArray &payload, int qos = 1, bool retain = false);

signals:
    void connected();
    void disconnected();
    void messageArrived(const QString &topic, const QByteArray &payload);
    void mqttError(const QString &error);

private:
    QMqttClient *m_client = nullptr;
    MqttConfig m_config;
};
