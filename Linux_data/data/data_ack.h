#ifndef DATA_ACK_H
#define DATA_ACK_H

#include <stddef.h>
#include <stdint.h>

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void data_ack_send(uint32_t seq,
                   const char *cmd,
                   int ok,
                   const char *reason,
                   const char *message);
void data_ack_send_ports(uint32_t seq,
                         const char *cmd,
                         const char * const *ports,
                         size_t port_count);
void data_ack_send_port_result(uint32_t seq,
                               const char *cmd,
                               int ok,
                               const char *reason,
                               const char *message,
                               int slot,
                               const char *port,
                               const char *device_type,
                               int baud,
                               int connected);
void data_ack_send_device_result(uint32_t seq,
                                 const char *cmd,
                                 int ok,
                                 const char *reason,
                                 const char *message,
                                 int slot,
                                 int slave_id,
                                 const char *device_type,
                                 int poll_interval_ms);
void data_ack_send_relay_result(uint32_t seq,
                                const char *cmd,
                                int ok,
                                const char *reason,
                                const char *message,
                                int slot,
                                int slave_id,
                                int device_id,
                                uint16_t states);
void data_ack_send_threshold_result(uint32_t seq,
                                    const char *cmd,
                                    int ok,
                                    const char *reason,
                                    const char *message,
                                    int slot,
                                    int slave_id,
                                    const char *device_type,
                                    int threshold_enabled,
                                    const sensor_threshold_config_t *threshold_config);
void data_ack_send_offline_cache_config(uint32_t seq,
                                        const char *cmd,
                                        int ok,
                                        const char *reason,
                                        const char *message,
                                        int cache_enabled,
                                        int flush_enabled,
                                        int pending_count);

const char *data_ack_message_from_reason(const char *reason);

#ifdef __cplusplus
}
#endif

#endif
