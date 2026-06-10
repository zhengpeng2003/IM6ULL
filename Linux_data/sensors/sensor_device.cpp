#include "sensor_device.hpp"

SensorDevice::SensorDevice(int slave_id, int poll_interval_ms)
    : slave_id_(slave_id),
      poll_interval_ms_(poll_interval_ms)
{}

int SensorDevice::slaveId() const
{
    return slave_id_;
}

int SensorDevice::pollIntervalMs() const
{
    return poll_interval_ms_;
}

bool SensorDevice::thresholdConfig(sensor_threshold_config_t *config) const
{
    (void)config;
    return false;
}

void SensorDevice::setThresholdConfig(const sensor_threshold_config_t &config)
{
    (void)config;
}

bool SensorDevice::checkThreshold(const device_data_t &dev, ThresholdAlarmEvent *event)
{
    (void)dev;
    (void)event;
    return false;
}
