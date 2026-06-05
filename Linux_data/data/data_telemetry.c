#include "data_telemetry.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *device_type_to_str(device_type_t type)
{
    switch (type) {
    case DEV_SENSOR_TH: return "sensor_th";
    case DEV_RELAY:     return "relay";
    case DEV_SYSINFO:   return "sysinfo";
    default:            return "unknown";
    }
}

static int pack_device_json(const device_data_t *dev,
                            char *buf,
                            int size)
{
    int off = 0;

    off += snprintf(buf + off, size - off,
        "{"
        "\"id\":%d,"
        "\"type\":\"%s\","
        "\"valid\":%d",
        dev->device_id,
        device_type_to_str(dev->type),
        dev->valid);

    if (dev->valid) {
        switch (dev->type) {
        case DEV_SENSOR_TH:
            off += snprintf(buf + off, size - off,
                ",\"temp\":%.1f,"
                "\"humi\":%.1f",
                dev->data.th.temperature,
                dev->data.th.humidity);
            break;

        case DEV_RELAY:
            off += snprintf(buf + off, size - off,
                ",\"states\":%u",
                dev->data.relay.relay_states);
            break;

        case DEV_SYSINFO:
            off += snprintf(buf + off, size - off,
                ",\"kernel\":\"%s\","
                "\"arch\":\"%s\","
                "\"os\":\"%s\","
                "\"screen_w\":%d,"
                "\"screen_h\":%d",
                dev->data.sys.kernel,
                dev->data.sys.arch,
                dev->data.sys.os,
                dev->data.sys.screen_w,
                dev->data.sys.screen_h);
            break;

        default:
            break;
        }
    }

    off += snprintf(buf + off, size - off, "}");
    return off;
}

telemetry_pack_t telemetry_pack_single(const device_data_t *dev)
{
    static uint32_t seq = 0;
    telemetry_pack_t pack;

    memset(&pack, 0, sizeof(pack));
    pack.seq = seq++;
    pack.timestamp = time(NULL);
    pack.device_count = 1;
    if (dev)
        pack.devices[0] = *dev;

    return pack;
}

int telemetry_pack_to_json(const telemetry_pack_t *pack,
                           char *buf,
                           int buf_size)
{
    if (!pack || !buf || buf_size <= 0)
        return -1;

    int off = 0;

    off += snprintf(buf + off, buf_size - off,
        "{"
        "\"ver\":1,"
        "\"seq\":%u,"
        "\"time\":%ld,"
        "\"count\":%d,"
        "\"devices\":[",
        pack->seq,
        pack->timestamp,
        pack->device_count);

    for (int i = 0; i < pack->device_count; i++) {
        if (i > 0)
            off += snprintf(buf + off, buf_size - off, ",");

        off += pack_device_json(&pack->devices[i],
                                buf + off,
                                buf_size - off);
    }

    off += snprintf(buf + off, buf_size - off, "]}\n");

    if (off >= buf_size)
        return -1;

    return off;
}
