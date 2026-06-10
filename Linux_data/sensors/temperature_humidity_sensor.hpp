#pragma once

#include "sensor_device.hpp"

class TemperatureHumiditySensor : public SensorDevice {
public:
    TemperatureHumiditySensor(int slave_id,
                              int poll_interval_ms,
                              const sensor_threshold_config_t *config);

    device_type_t deviceType() const override;
    const char *deviceTypeName() const override;
    int read(ModbusMaster &bus, device_data_t *dev) override;
    bool thresholdConfig(sensor_threshold_config_t *config) const override;
    void setThresholdConfig(const sensor_threshold_config_t &config) override;
    bool checkThreshold(const device_data_t &dev, ThresholdAlarmEvent *event) override;

private:
    bool checkPoint(const char *point_key,
                    const point_threshold_config_t &threshold,
                    float value,
                    int *last_state,
                    ThresholdAlarmEvent *event);

private:
    sensor_threshold_config_t config_ = {};
    bool has_config_ = false;
    int last_temperature_alarm_ = 0;
    int last_humidity_alarm_ = 0;
};
