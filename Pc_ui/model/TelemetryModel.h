#pragma once
#include "DeviceModel.h"
#include <QMetaType>

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
    bool valid = false;
    qint64 timestamp = 0;
};

struct TelemetryRecord
{
    RealtimeDeviceData data;
};

Q_DECLARE_METATYPE(RealtimeDeviceData)
Q_DECLARE_METATYPE(TelemetryRecord)
