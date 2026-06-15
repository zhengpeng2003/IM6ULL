#pragma once
#include "DeviceModel.h"
#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>

struct TelemetryPointData
{
    QString pointId;
    qint64 timestampMs = 0;
    QString factoryId;
    QString factoryName;
    QString areaId;
    QString areaName;
    QString gatewayId;
    QString gatewayName;
    QString portId;
    QString portName;
    int deviceId = 0;
    QString deviceName;
    QString deviceType;
    QString pointKey;
    QString pointName;
    QString unit;
    QString valueType;
    double numberValue = 0.0;
    QString textValue;
    bool valid = false;
    QString errorMessage;
    QString dataState;
    qint64 receiveTimeMs = 0;
    qint64 lastUpdateTime = 0;
};

struct SensorThData
{
    double temperature = 0.0;
    double humidity = 0.0;
};

struct RelayData
{
    bool led = false;
    bool fan = false;
    bool buzzer = false;
    QMap<QString, bool> channels;
};

struct MeterData
{
    double voltage = 0.0;
    double current = 0.0;
    double power = 0.0;
    double energy = 0.0;
};

struct RealtimeDeviceData
{
    DeviceNode node;
    SensorThData sensorTh;
    RelayData relay;
    MeterData meter;
    QList<TelemetryPointData> points;
    QString errorMessage;
    QString statusText;
    QString statusLevel;
    QString dataState;
    bool serviceOffline = false;
    bool mock = false;
    bool valid = false;
    qint64 timestamp = 0;
};

Q_DECLARE_METATYPE(TelemetryPointData)
Q_DECLARE_METATYPE(RealtimeDeviceData)
