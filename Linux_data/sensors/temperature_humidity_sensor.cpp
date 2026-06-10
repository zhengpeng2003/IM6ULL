#include "temperature_humidity_sensor.hpp"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

TemperatureHumiditySensor::TemperatureHumiditySensor(int slave_id,
                                                     int poll_interval_ms,
                                                     const sensor_threshold_config_t *config)
    : SensorDevice(slave_id, poll_interval_ms)
{
    memset(&config_, 0, sizeof(config_));
    if (config) {
        config_ = *config;
        has_config_ = true;
    }
}

device_type_t TemperatureHumiditySensor::deviceType() const
{
    return DEV_SENSOR_TH;
}

const char *TemperatureHumiditySensor::deviceTypeName() const
{
    return "sensor_th";
}

int TemperatureHumiditySensor::read(ModbusMaster &bus, device_data_t *dev)
{
    if (!dev)
        return -1;

    memset(dev, 0, sizeof(*dev));
    dev->device_id = slaveId();
    dev->type = DEV_SENSOR_TH;

    uint16_t regs[2] = {0};
    int ret = bus.readRegisters(slaveId(), 0x0000, 2, regs);
    dev->valid = (ret == 2);
    if (!dev->valid)
        return -1;

    dev->data.th.temperature = static_cast<int16_t>(regs[1]) / 10.0f;
    dev->data.th.humidity = regs[0] / 10.0f;
    return 0;
}

bool TemperatureHumiditySensor::thresholdConfig(sensor_threshold_config_t *config) const
{
    if (!config)
        return false;

    if (!has_config_)
        return false;

    *config = config_;
    return true;
}

void TemperatureHumiditySensor::setThresholdConfig(const sensor_threshold_config_t &config)
{
    config_ = config;
    has_config_ = true;
    last_temperature_alarm_ = 0;
    last_humidity_alarm_ = 0;
}

bool TemperatureHumiditySensor::checkPoint(const char *point_key,
                                           const point_threshold_config_t &threshold,
                                           float value,
                                           int *last_state,
                                           ThresholdAlarmEvent *event)
{
    if (!threshold.enable_alarm || !last_state)
        return false;

    int active = 0;
    float limit = 0.0f;
    const char *suffix = "";

    if (threshold.has_high && value > threshold.alarm_high) {
        active = 1;
        limit = threshold.alarm_high;
        suffix = "_high";
    } else if (threshold.has_low && value < threshold.alarm_low) {
        active = 1;
        limit = threshold.alarm_low;
        suffix = "_low";
    }

    if (!active) {
        *last_state = 0;
        return false;
    }

    if (*last_state)
        return false;

    *last_state = 1;
    if (event) {
        event->active = 1;
        event->value = value;
        event->threshold = limit;
        snprintf(event->point_key, sizeof(event->point_key), "%s", point_key ? point_key : "");
        snprintf(event->reason, sizeof(event->reason), "%s%s", point_key ? point_key : "point", suffix);
    }
    return true;
}

bool TemperatureHumiditySensor::checkThreshold(const device_data_t &dev,
                                               ThresholdAlarmEvent *event)
{
    if (!has_config_ || !config_.threshold_enabled || !dev.valid || dev.type != DEV_SENSOR_TH)
        return false;

    if (checkPoint("temperature",
                   config_.temperature,
                   dev.data.th.temperature,
                   &last_temperature_alarm_,
                   event)) {
        return true;
    }

    return checkPoint("humidity",
                      config_.humidity,
                      dev.data.th.humidity,
                      &last_humidity_alarm_,
                      event);
}
