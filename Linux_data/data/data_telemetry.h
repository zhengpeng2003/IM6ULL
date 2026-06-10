#ifndef DATA_TELEMETRY_H
#define DATA_TELEMETRY_H

#include "data_protocol.h"

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_SOURCE_ID "gateway_001"
#define DEFAULT_TARGET_ID "pc_data_001"
#define DEFAULT_FACTORY_ID "factory_001"
#define DEFAULT_FACTORY_NAME "1号厂房"
#define DEFAULT_AREA_ID "area_001"
#define DEFAULT_AREA_NAME "配电房"
#define DEFAULT_GATEWAY_ID "gateway_001"
#define DEFAULT_GATEWAY_NAME "i.MX6ULL_001"
#define DEFAULT_PORT_ID "port_001"
#define DEFAULT_PORT_NAME "RS485-1"

typedef struct {
    char factory_id[MAX_CLIENT_ID_LEN];
    char factory_name[MAX_DEVICE_NAME_LEN];
    char area_id[MAX_CLIENT_ID_LEN];
    char area_name[MAX_DEVICE_NAME_LEN];
    char gateway_id[MAX_CLIENT_ID_LEN];
    char gateway_name[MAX_DEVICE_NAME_LEN];
    char port_id[MAX_CLIENT_ID_LEN];
    char port_name[MAX_DEVICE_NAME_LEN];
} site_context_t;

typedef struct {
    uint32_t seq;
    time_t timestamp;
    int64_t timestamp_ms;
    char source_id[MAX_CLIENT_ID_LEN];
    char target_id[MAX_CLIENT_ID_LEN];
    site_context_t site;
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
