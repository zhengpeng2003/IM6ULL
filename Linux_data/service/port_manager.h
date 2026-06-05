#pragma once

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void port_manager_scan_ports(void);
void port_manager_connect(int slot, const char *port, const char *device_type, int baud);
void port_manager_disconnect(int slot);
void port_manager_poll_slot(int slot);
void port_manager_handle_relay(const device_data_t *dev);

#ifdef __cplusplus
}
#endif
