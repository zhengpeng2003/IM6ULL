#include "relay.hpp"

#include <string.h>

namespace {

const int kRelayCount = 4;

} // namespace

int relay_read_state(ModbusMaster &bus, int slave_id, device_data_t *dev)
{
    if (!dev)
        return -1;

    memset(dev, 0, sizeof(*dev));
    dev->device_id = slave_id;
    dev->type = DEV_RELAY;

    uint8_t states[kRelayCount] = {0};
    int ret = bus.readCoils(slave_id, 0, kRelayCount, states);
    dev->valid = (ret == kRelayCount);
    if (!dev->valid)
        return -1;

    uint16_t bitmap = 0;
    for (int i = 0; i < kRelayCount; ++i) {
        if (states[i])
            bitmap |= static_cast<uint16_t>(1U << i);
    }

    dev->data.relay.relay_states = bitmap;
    return 0;
}

int relay_write_states(ModbusMaster &bus, int slave_id, uint16_t states)
{
    uint8_t coils[kRelayCount] = {0};
    for (int i = 0; i < kRelayCount; ++i)
        coils[i] = (states & (1U << i)) ? 1 : 0;

    return bus.writeCoils(slave_id, 0, kRelayCount, coils) == kRelayCount ? 0 : -1;
}
