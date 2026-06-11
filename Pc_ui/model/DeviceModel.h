#pragma once
#include <QString>
#include <QMetaType>

struct DeviceNode
{
    QString factoryId;
    QString factoryName;
    QString areaId;
    QString areaName;
    QString gatewayId;
    QString gatewayName;

    int masterSlot = 0;
    QString masterName;
    QString port;
    int baud = 9600;

    int slaveAddr = 0;
    int deviceId = 0;
    QString deviceName;
    QString deviceType;
    bool expectTelemetry = true;
    QString status;
    QString statusReason;

    bool online = false;
    qint64 lastUpdateTime = 0;

    QString key() const
    {
        return QString("%1/%2/%3/%4/%5")
            .arg(factoryId, areaId, gatewayId)
            .arg(masterSlot)
            .arg(slaveAddr);
    }
};

Q_DECLARE_METATYPE(DeviceNode)
