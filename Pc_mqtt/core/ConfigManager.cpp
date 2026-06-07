#include "ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {}

MqttConfig ConfigManager::loadMqttConfig() const
{
    MqttConfig cfg;
    QFile f("config/mqtt_config.json");
    if (!f.open(QIODevice::ReadOnly)) return cfg;
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    cfg.host = obj.value("host").toString(cfg.host);
    cfg.port = obj.value("port").toInt(cfg.port);
    cfg.username = obj.value("username").toString();
    cfg.password = obj.value("password").toString();
    cfg.clientId = obj.value("client_id").toString(cfg.clientId);
    cfg.topicPrefix = obj.value("topic_prefix").toString(cfg.topicPrefix);
    cfg.autoConnect = obj.value("auto_connect").toBool(false);
    cfg.autoReconnect = obj.value("auto_reconnect").toBool(true);
    return cfg;
}

void ConfigManager::saveMqttConfig(const MqttConfig &config) const
{
    QJsonObject obj;
    obj["host"] = config.host;
    obj["port"] = config.port;
    obj["username"] = config.username;
    obj["password"] = config.password;
    obj["client_id"] = config.clientId;
    obj["topic_prefix"] = config.topicPrefix;
    obj["auto_connect"] = config.autoConnect;
    obj["auto_reconnect"] = config.autoReconnect;
    QFile f("config/mqtt_config.json");
    if (f.open(QIODevice::WriteOnly)) f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

QString ConfigManager::loadDatabasePath() const
{
    return "db/pc_mqtt.db";
}

QList<DeviceNode> ConfigManager::loadDeviceConfig() const
{
    return {};
}

void ConfigManager::saveDeviceConfig(const QList<DeviceNode> &devices) const
{
    Q_UNUSED(devices);
}
