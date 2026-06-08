#pragma once
#include <QString>
#include <QMetaType>

struct CommandRecord
{
    QString cmdId;
    qint64 timestamp = 0;
    QString factoryId;
    QString areaId;
    QString gatewayId;
    int masterSlot = 0;
    int slaveAddr = 0;
    QString deviceType;
    QString command;
    QString paramsJson;
    QString state;
    bool ok = false;
    QString reason;
    qint64 ackTime = 0;
};

Q_DECLARE_METATYPE(CommandRecord)
