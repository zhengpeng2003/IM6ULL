#include "relay.hpp"

#include <string.h>

RelayDevice::RelayDevice(int slave_id, int poll_interval_ms)
    : SensorDevice(slave_id, poll_interval_ms)
{}

device_type_t RelayDevice::deviceType() const
{
    return DEV_RELAY;
}

const char *RelayDevice::deviceTypeName() const
{
    return "relay";
}

int RelayDevice::read(ModbusMaster &bus, device_data_t *dev)
{
    if (!dev)
        return -1;

    memset(dev, 0, sizeof(*dev));
    dev->device_id = slaveId();
    dev->type = DEV_RELAY;

    uint8_t states[ChannelCount] = {0};
    int ret = bus.readCoils(slaveId(), 0, ChannelCount, states);
    dev->valid = (ret == ChannelCount);
    if (!dev->valid)
        return -1;

    uint16_t bitmap = 0;
    for (int i = 0; i < ChannelCount; ++i) {
        if (states[i])
            bitmap |= static_cast<uint16_t>(1U << i);
    }

    dev->data.relay.relay_states = bitmap;
    dev->data.relay.channel_count = ChannelCount;
    return 0;
}

int RelayDevice::writeStates(ModbusMaster &bus, uint16_t states) const
{
    uint8_t coils[ChannelCount] = {0};
    for (int i = 0; i < ChannelCount; ++i)
        coils[i] = (states & (1U << i)) ? 1 : 0;

    return bus.writeCoils(slaveId(), 0, ChannelCount, coils) == ChannelCount ? 0 : -1;
}
