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

struct GatewayNode
{
    QString gatewayId;
    QString gatewayName;
    QString factoryId;
    QString areaId;
    QString status;
    qint64 lastRegisterTimeMs = 0;
    qint64 lastHeartbeatTimeMs = 0;
    qint64 updateTimeMs = 0;

    bool online() const
    {
        return status == QStringLiteral("online");
    }
};

struct PortNode
{
    QString gatewayId;
    QString portId;
    QString portName;
    int slot = 0;
    QString devicePath;
    int baud = 0;
    QString status;
    qint64 lastRegisterTimeMs = 0;
    qint64 updateTimeMs = 0;

    QString key() const
    {
        return gatewayId + QStringLiteral("/") + portId;
    }

    bool connected() const
    {
        return status == QStringLiteral("connected");
    }
};

Q_DECLARE_METATYPE(DeviceNode)
Q_DECLARE_METATYPE(GatewayNode)
Q_DECLARE_METATYPE(PortNode)
