#ifndef DATA_ACK_H
#define DATA_ACK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void data_ack_send(uint32_t seq,
                   const char *cmd,
                   int ok,
                   const char *reason,
                   const char *message);
void data_ack_send_alarm_config(uint32_t seq,
                                const char *cmd,
                                int ok,
                                const char *reason,
                                const char *message,
                                float temp_high,
                                float humi_high);
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

const char *data_ack_message_from_reason(const char *reason);

#ifdef __cplusplus
}
#endif

#endif
