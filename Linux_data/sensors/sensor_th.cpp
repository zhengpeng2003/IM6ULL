#include "sensor_th.hpp"

#include <stdint.h>
#include <string.h>

int sensor_th_read(ModbusMaster &bus, int slave_id, device_data_t *dev)
{
    if (!dev)
        return -1;

    memset(dev, 0, sizeof(*dev));
    dev->device_id = slave_id;
    dev->type = DEV_SENSOR_TH;

    uint16_t regs[2] = {0};
    int ret = bus.readRegisters(slave_id, 0x0000, 2, regs);
    dev->valid = (ret == 2);
    if (!dev->valid)
        return -1;

    dev->data.th.temperature = static_cast<int16_t>(regs[1]) / 10.0f;
    dev->data.th.humidity = regs[0] / 10.0f;
    return 0;
}
