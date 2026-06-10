#pragma once
#include <QObject>
#include "model/DeviceModel.h"

class ConfigManager : public QObject
{
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    QList<DeviceNode> loadDeviceConfig() const;
    void saveDeviceConfig(const QList<DeviceNode> &devices) const;
};
