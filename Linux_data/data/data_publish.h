#ifndef DATA_PUBLISH_H
#define DATA_PUBLISH_H

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

int data_publish_device_status(const device_data_t *dev);
int data_publish_gateway_register(uint32_t seq);
int data_publish_gateway_heartbeat(uint32_t seq);
int data_publish_port_register(uint32_t seq,
                               int slot,
                               const char *device_path,
                               int baud,
                               const char *status);
int data_publish_device_register(uint32_t seq,
                                 int slot,
                                 int slave_id,
                                 const char *device_type,
                                 int poll_interval_ms);

#ifdef __cplusplus
}
#endif

#endif
