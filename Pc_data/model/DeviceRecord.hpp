#ifndef DEVICE_RECORD_HPP
#define DEVICE_RECORD_HPP

#include <cstdint>
#include <string>

struct DeviceRecord
{
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
    int pollIntervalMs = 1000;
    bool expectTelemetry = true;
    bool enabled = true;

    std::string status = "unknown";
    std::int64_t lastSeenMs = 0;
    std::int64_t lastOfflineMs = 0;
    std::string statusReason;

    std::int64_t createTimeMs = 0;
    std::int64_t updateTimeMs = 0;
};

#endif // DEVICE_RECORD_HPP
