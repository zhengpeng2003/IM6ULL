#pragma once

#include "data_protocol.h"
#include "modbus_master.hpp"

int sensor_th_read(ModbusMaster &bus, int slave_id, device_data_t *dev);
