#ifndef POINT_CONFIG_HPP
#define POINT_CONFIG_HPP

#include <cstdint>
#include <string>

struct PointConfig
{
    std::string pointId;

    std::string factoryId;
    std::string factoryName;
    std::string areaId;
    std::string areaName;
    std::string gatewayId;
    std::string gatewayName;
    std::string portId;
    std::string portName;

    int deviceId = 0;
    std::string deviceName;
    std::string deviceType;

    std::string pointKey;
    std::string pointName;
    std::string unit;
    std::string valueType;

    bool enableAlarm = false;
    bool hasAlarmLow = false;
    double alarmLow = 0.0;
    bool hasAlarmHigh = false;
    double alarmHigh = 0.0;
    bool enabled = true;

    std::int64_t timestampMs = 0;
};

#endif // POINT_CONFIG_HPP
