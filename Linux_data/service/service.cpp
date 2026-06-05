#include <stdio.h>
#include "service.h"
#include "port_manager.h"

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

    port_manager_handle_relay(dev);
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
