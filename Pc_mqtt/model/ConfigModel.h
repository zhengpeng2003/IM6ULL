#pragma once
#include <QString>

struct MqttConfig
{
    QString host = "127.0.0.1";
    int port = 1883;
    QString username;
    QString password;
    QString clientId = "pc_monitor_001";
    QString topicPrefix = "factory";
    bool autoConnect = false;
    bool autoReconnect = true;
};

struct UiConfig
{
    int refreshIntervalMs = 1000;
};
