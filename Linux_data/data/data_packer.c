#include "data_packer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>  // 为了 time()
#include "service.h"

/* ========= 内部工具 ========= */

static const char *device_type_to_str(device_type_t type)
{
    switch (type) {
    case DEV_SENSOR_TH: return "sensor_th";
    case DEV_RELAY:     return "relay";
    case DEV_SYSINFO:   return "sysinfo";   // ⚡ 添加 DEV_SYSINFO
    default:            return "unknown";
    }
}

/* 打包单个 device（根据类型） */
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

        case DEV_SYSINFO:   // ⚡ 添加 sysinfo 对应字段
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

/* ========= 对外接口 ========= */

int data_pack_to_json(const data_pack_t *pack,
                      char *buf,
                      int buf_size)
{
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

    off += snprintf(buf + off, buf_size - off,
        "]}\n");   // \n 作为消息边界 //snprintf返回值本次“想要写入”的字符个数，不包含最后自动加的 \0

    if (off >= buf_size)
        return -1;

    return off;
}

data_pack_t data_pack_single(const device_data_t *dev)//就是帮你生成这个外层包。
{
    static uint32_t seq = 0;
    data_pack_t pack;

    memset(&pack, 0, sizeof(pack));
    pack.seq = seq++;
    pack.timestamp = time(NULL);
    pack.device_count = 1;
    pack.devices[0] = *dev;

    return pack;
}
//前端发送来的信息
void data_process_message(const char *jsonStr)
{
    struct json_object *root = json_tokener_parse(jsonStr);
    if (!root) return;

    struct json_object *devices;
    if (json_object_object_get_ex(root, "devices", &devices)) {
        int count = json_object_array_length(devices);
        for (int i = 0; i < count; i++) {
            struct json_object *d = json_object_array_get_idx(devices, i);
            int type = json_object_get_int(json_object_object_get(d, "type"));

            // 直接在 Data 层判断类型，然后调用 Service
            switch (type) {
                case DEV_RELAY: {
                    device_data_t dev;
                    dev.device_id = json_object_get_int(json_object_object_get(d, "id"));
                    dev.valid     = json_object_get_boolean(json_object_object_get(d, "valid"));
                    dev.data.relay.relay_states =
                        json_object_get_int(json_object_object_get(d, "states"));

                    service_handle_relay(&dev); // ✅ Data 层直接调用
                    break;
                }

                case DEV_SENSOR_TH: {
                    device_data_t dev;
                    dev.device_id = json_object_get_int(json_object_object_get(d, "id"));
                    dev.valid     = json_object_get_boolean(json_object_object_get(d, "valid"));
                    dev.data.th.temperature = json_object_get_double(json_object_object_get(d, "temp"));
                    dev.data.th.humidity    = json_object_get_double(json_object_object_get(d, "humi"));

                    service_handle_sensor(&dev); // 处理温湿度
                    break;
                }

                case DEV_SYSINFO:
                    // 可以在这里处理板子信息
                    break;

                default:
                    break;
            }
        }
    }

    json_object_put(root);
}
