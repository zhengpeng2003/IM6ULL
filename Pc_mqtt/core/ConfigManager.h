#pragma once
#include <QObject>
#include "model/ConfigModel.h"
#include "model/DeviceModel.h"

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    MqttConfig loadMqttConfig() const;
    void saveMqttConfig(const MqttConfig &config) const;
    QString loadDatabasePath() const;

    QList<DeviceNode> loadDeviceConfig() const;
    void saveDeviceConfig(const QList<DeviceNode> &devices) const;
};
