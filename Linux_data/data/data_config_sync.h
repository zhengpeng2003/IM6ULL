#ifndef DATA_CONFIG_SYNC_H
#define DATA_CONFIG_SYNC_H

#include <stdint.h>

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

int data_config_sync_mark_sent(uint32_t seq,
                               const char *cmd,
                               const char *payload,
                               const char *port_id);
int data_config_sync_publish_latest_snapshot(uint32_t seq);
void data_config_sync_handle_ack_json(const char *payload);
void data_config_sync_tick(void);
int data_config_sync_telemetry_allowed(int slot, const device_data_t *dev);
uint32_t data_config_sync_next_seq(void);

#ifdef __cplusplus
}
#endif

#endif
