#pragma once

#include "data_protocol.h"
#include "modbus_master.hpp"

#include <stdint.h>

struct ThresholdAlarmEvent {
    int active = 0;
    char reason[MAX_ACK_MSG_LEN] = {0};
    float value = 0.0f;
    float threshold = 0.0f;
    char point_key[32] = {0};
};

class SensorDevice {
public:
    SensorDevice(int slave_id, int poll_interval_ms);
    virtual ~SensorDevice() = default;

    int slaveId() const;
    int pollIntervalMs() const;

    virtual device_type_t deviceType() const = 0;
    virtual const char *deviceTypeName() const = 0;
    virtual int read(ModbusMaster &bus, device_data_t *dev) = 0;
    virtual bool thresholdConfig(sensor_threshold_config_t *config) const;
    virtual void setThresholdConfig(const sensor_threshold_config_t &config);
    virtual bool checkThreshold(const device_data_t &dev, ThresholdAlarmEvent *event);

private:
    int slave_id_ = 0;
    int poll_interval_ms_ = 0;
};
