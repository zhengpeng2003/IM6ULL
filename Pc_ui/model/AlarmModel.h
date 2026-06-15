#pragma once
#include <QString>
#include <QMetaType>

struct AlarmRecord
{
    QString alarmId;
    QString factoryId;
    QString areaId;
    QString areaName;
    QString gatewayId;
    QString portId;
    int masterSlot = 0;
    int slaveAddr = 0;
    QString deviceName;
    QString deviceType;
    QString pointKey;
    QString alarmType;
    QString level;
    QString message;
    double value = 0.0;
    double threshold = 0.0;
    QString state;
    qint64 startTime = 0;
    qint64 ackTime = 0;
    qint64 recoverTime = 0;
};

Q_DECLARE_METATYPE(AlarmRecord)
