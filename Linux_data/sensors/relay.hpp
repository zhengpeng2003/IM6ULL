#pragma once

#include "sensor_device.hpp"

#include <stdint.h>

#define RELAY_LED_BIT     0
#define RELAY_FAN_BIT     1
#define RELAY_BUZZER_BIT  2

#define RELAY_BIT_ON(states, bit)   ((states) |  (1 << (bit)))
#define RELAY_BIT_OFF(states, bit)  ((states) & ~(1 << (bit)))
#define RELAY_BIT_GET(states, bit)  (((states) >> (bit)) & 1)

class RelayDevice : public SensorDevice {
public:
    static const int ChannelCount = 4;

    RelayDevice(int slave_id, int poll_interval_ms);

    device_type_t deviceType() const override;
    const char *deviceTypeName() const override;
    int read(ModbusMaster &bus, device_data_t *dev) override;
    int writeStates(ModbusMaster &bus, uint16_t states) const;
};
