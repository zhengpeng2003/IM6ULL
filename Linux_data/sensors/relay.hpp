#pragma once

#include "data_protocol.h"
#include "modbus_master.hpp"

#include <stdint.h>

#define RELAY_LED_BIT     0
#define RELAY_FAN_BIT     1
#define RELAY_BUZZER_BIT  2

#define RELAY_BIT_ON(states, bit)   ((states) |  (1 << (bit)))
#define RELAY_BIT_OFF(states, bit)  ((states) & ~(1 << (bit)))
#define RELAY_BIT_GET(states, bit)  (((states) >> (bit)) & 1)

int relay_read_state(ModbusMaster &bus, int slave_id, device_data_t *dev);
int relay_write_states(ModbusMaster &bus, int slave_id, uint16_t states);
