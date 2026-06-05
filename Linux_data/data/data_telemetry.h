#ifndef DATA_TELEMETRY_H
#define DATA_TELEMETRY_H

#include "data_protocol.h"

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t seq;
    time_t timestamp;
    int device_count;
    device_data_t devices[MAX_DEVICES_PER_PACK];
} telemetry_pack_t;

telemetry_pack_t telemetry_pack_single(const device_data_t *dev);
int telemetry_pack_to_json(const telemetry_pack_t *pack,
                           char *buf,
                           int buf_size);

#ifdef __cplusplus
}
#endif

#endif
