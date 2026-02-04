#include <stdio.h>
#include "service.h"
#include "rs485_bus.hpp"

extern "C" {

/*
 * 处理继电器（来自前端 JSON）
 */
void service_handle_relay(const device_data_t *dev)
{
    if (!dev || !dev->valid)
        return;

    uint16_t states = dev->data.relay.relay_states;
    printf("[Service] relayStates=0x%04x\n", states);

    for (int i = 0; i < 16; i++) {
        uint8_t v = (states & (1 << i)) ? 1 : 0;
        RS485_1.writeCoils(1, i, 1, &v);
    }
}

/*
 * 处理温湿度
 */
void service_handle_sensor(const device_data_t *dev)
{
    if (!dev || !dev->valid)
        return;

    printf("[Service] sensor id=%d temp=%.1f humi=%.1f\n",
           dev->device_id,
           dev->data.th.temperature,
           dev->data.th.humidity);
}

} // extern "C"

