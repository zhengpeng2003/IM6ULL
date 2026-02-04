#ifndef SERVICE_HANDLER_H
#define SERVICE_HANDLER_H

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void service_handle_relay(const device_data_t *dev);
void service_handle_sensor(const device_data_t *dev);

#ifdef __cplusplus
}
#endif

#endif // SERVICE_HANDLER_H

