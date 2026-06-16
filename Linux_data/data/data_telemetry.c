#include "data_telemetry.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static const char *device_type_to_str(device_type_t type)
{
    switch (type) {
    case DEV_SENSOR_TH: return "sensor_th";
    case DEV_RELAY: return "relay";
    case DEV_ELECTRIC_METER: return "electric_meter";
    case DEV_SYSINFO: return "sysinfo";
    default: return "unknown";
    }
}

static int64_t current_time_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
        return (int64_t)time(NULL) * 1000;

    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;

    snprintf(dst, dst_size, "%s", src ? src : "");
}

static void fill_default_site(site_context_t *site)
{
    if (!site)
        return;

    copy_text(site->factory_id, sizeof(site->factory_id), DEFAULT_FACTORY_ID);
    copy_text(site->factory_name, sizeof(site->factory_name), DEFAULT_FACTORY_NAME);
    copy_text(site->area_id, sizeof(site->area_id), DEFAULT_AREA_ID);
    copy_text(site->area_name, sizeof(site->area_name), DEFAULT_AREA_NAME);
    copy_text(site->gateway_id, sizeof(site->gateway_id), DEFAULT_GATEWAY_ID);
    copy_text(site->gateway_name, sizeof(site->gateway_name), DEFAULT_GATEWAY_NAME);
    copy_text(site->port_id, sizeof(site->port_id), DEFAULT_PORT_ID);
    copy_text(site->port_name, sizeof(site->port_name), DEFAULT_PORT_NAME);
}

static void fill_slot_site(site_context_t *site, int slot)
{
    fill_default_site(site);
    if (!site)
        return;

    if (slot == 1) {
        copy_text(site->port_id, sizeof(site->port_id), "port_002");
        copy_text(site->port_name, sizeof(site->port_name), "RS485-2");
    }
}

static void add_string(struct json_object *obj, const char *key, const char *value)
{
    json_object_object_add(obj, key, json_object_new_string(value ? value : ""));
}

static struct json_object *site_to_json(const site_context_t *site)
{
    struct json_object *obj = json_object_new_object();
    if (!obj)
        return NULL;

    add_string(obj, "factoryId", site ? site->factory_id : DEFAULT_FACTORY_ID);
    add_string(obj, "factoryName", site ? site->factory_name : DEFAULT_FACTORY_NAME);
    add_string(obj, "areaId", site ? site->area_id : DEFAULT_AREA_ID);
    add_string(obj, "areaName", site ? site->area_name : DEFAULT_AREA_NAME);
    add_string(obj, "gatewayId", site ? site->gateway_id : DEFAULT_GATEWAY_ID);
    add_string(obj, "gatewayName", site ? site->gateway_name : DEFAULT_GATEWAY_NAME);
    add_string(obj, "portId", site ? site->port_id : DEFAULT_PORT_ID);
    add_string(obj, "portName", site ? site->port_name : DEFAULT_PORT_NAME);

    return obj;
}

static const char *default_device_name(const device_data_t *dev,
                                       char *buf,
                                       size_t buf_size)
{
    if (dev && dev->device_name[0] != '\0')
        return dev->device_name;

    snprintf(buf, buf_size, "Device %d", dev ? dev->device_id : 0);
    return buf;
}

static struct json_object *pack_device_json(const device_data_t *dev)
{
    if (!dev)
        return NULL;

    char name_buf[MAX_DEVICE_NAME_LEN];
    const char *device_type = device_type_to_str(dev->type);
    const char *device_name = default_device_name(dev, name_buf, sizeof(name_buf));

    struct json_object *obj = json_object_new_object();
    if (!obj)
        return NULL;

    json_object_object_add(obj, "deviceId", json_object_new_int(dev->device_id));
    add_string(obj, "deviceName", device_name);
    add_string(obj, "deviceType", device_type);
    json_object_object_add(obj, "timestampMs", json_object_new_int64(current_time_ms()));
    json_object_object_add(obj, "valid", json_object_new_boolean(dev->valid));
    add_string(obj, "errorMessage", dev->valid || dev->error_message[0] != '\0' ? dev->error_message : "数据无效");

    /* Keep legacy field aliases for Linux_ui/data/data_parser.cpp. */
    json_object_object_add(obj, "id", json_object_new_int(dev->device_id));
    add_string(obj, "type", device_type);

    switch (dev->type) {
    case DEV_SENSOR_TH: {
        struct json_object *th = json_object_new_object();
        if (th) {
            json_object_object_add(th, "temperature", json_object_new_double(dev->data.th.temperature));
            json_object_object_add(th, "humidity", json_object_new_double(dev->data.th.humidity));
            json_object_object_add(obj, "th", th);
        }
        json_object_object_add(obj, "temperature", json_object_new_double(dev->data.th.temperature));
        json_object_object_add(obj, "humidity", json_object_new_double(dev->data.th.humidity));
        json_object_object_add(obj, "temp", json_object_new_double(dev->data.th.temperature));
        json_object_object_add(obj, "humi", json_object_new_double(dev->data.th.humidity));
        break;
    }

    case DEV_RELAY: {
        const int channel_count = dev->data.relay.channel_count > 0
            ? dev->data.relay.channel_count
            : 16;
        struct json_object *relay = json_object_new_object();
        if (relay) {
            json_object_object_add(relay, "relayStates", json_object_new_int(dev->data.relay.relay_states));
            json_object_object_add(relay, "channelCount", json_object_new_int(channel_count));
            json_object_object_add(obj, "relay", relay);
        }
        json_object_object_add(obj, "relayStates", json_object_new_int(dev->data.relay.relay_states));
        json_object_object_add(obj, "channelCount", json_object_new_int(channel_count));
        json_object_object_add(obj, "states", json_object_new_int(dev->data.relay.relay_states));
        break;
    }

    case DEV_ELECTRIC_METER: {
        struct json_object *meter = json_object_new_object();
        if (meter) {
            json_object_object_add(meter, "voltage", json_object_new_double(dev->data.meter.voltage));
            json_object_object_add(meter, "current", json_object_new_double(dev->data.meter.current));
            json_object_object_add(meter, "power", json_object_new_double(dev->data.meter.power));
            json_object_object_add(meter, "energy", json_object_new_double(dev->data.meter.energy));
            json_object_object_add(obj, "meter", meter);
        }
        json_object_object_add(obj, "voltage", json_object_new_double(dev->data.meter.voltage));
        json_object_object_add(obj, "current", json_object_new_double(dev->data.meter.current));
        json_object_object_add(obj, "power", json_object_new_double(dev->data.meter.power));
        json_object_object_add(obj, "energy", json_object_new_double(dev->data.meter.energy));
        break;
    }

    case DEV_SYSINFO: {
        struct json_object *sys = json_object_new_object();
        if (sys) {
            add_string(sys, "kernel", dev->data.sys.kernel);
            add_string(sys, "arch", dev->data.sys.arch);
            add_string(sys, "os", dev->data.sys.os);
            json_object_object_add(sys, "screenWidth", json_object_new_int(dev->data.sys.screen_w));
            json_object_object_add(sys, "screenHeight", json_object_new_int(dev->data.sys.screen_h));
            json_object_object_add(sys, "cpuUsage", json_object_new_double(dev->data.sys.cpu_usage));
            json_object_object_add(sys, "memoryUsage", json_object_new_double(dev->data.sys.memory_usage));
            json_object_object_add(obj, "sys", sys);
        }
        add_string(obj, "kernel", dev->data.sys.kernel);
        add_string(obj, "arch", dev->data.sys.arch);
        add_string(obj, "os", dev->data.sys.os);
        json_object_object_add(obj, "screenWidth", json_object_new_int(dev->data.sys.screen_w));
        json_object_object_add(obj, "screenHeight", json_object_new_int(dev->data.sys.screen_h));
        json_object_object_add(obj, "cpuUsage", json_object_new_double(dev->data.sys.cpu_usage));
        json_object_object_add(obj, "memoryUsage", json_object_new_double(dev->data.sys.memory_usage));
        json_object_object_add(obj, "screen_w", json_object_new_int(dev->data.sys.screen_w));
        json_object_object_add(obj, "screen_h", json_object_new_int(dev->data.sys.screen_h));
        break;
    }

    default:
        break;
    }

    struct json_object *points = json_object_new_array();
    if (points) {
        if (dev->type == DEV_SENSOR_TH) {
            struct json_object *temperature = json_object_new_object();
            if (temperature) {
                add_string(temperature, "pointKey", "temperature");
                add_string(temperature, "pointName", "温度");
                add_string(temperature, "valueType", "Number");
                json_object_object_add(temperature, "numberValue", json_object_new_double(dev->data.th.temperature));
                add_string(temperature, "unit", "℃");
                json_object_array_add(points, temperature);
            }
            struct json_object *humidity = json_object_new_object();
            if (humidity) {
                add_string(humidity, "pointKey", "humidity");
                add_string(humidity, "pointName", "湿度");
                add_string(humidity, "valueType", "Number");
                json_object_object_add(humidity, "numberValue", json_object_new_double(dev->data.th.humidity));
                add_string(humidity, "unit", "%");
                json_object_array_add(points, humidity);
            }
        } else if (dev->type == DEV_RELAY) {
            struct json_object *relay_point = json_object_new_object();
            if (relay_point) {
                add_string(relay_point, "pointKey", "relay.ch1");
                add_string(relay_point, "pointName", "继电器通道1");
                add_string(relay_point, "valueType", "Boolean");
                json_object_object_add(relay_point,
                                       "boolValue",
                                       json_object_new_boolean((dev->data.relay.relay_states & 0x01) != 0));
                add_string(relay_point, "unit", "");
                json_object_array_add(points, relay_point);
            }
        }

        if (json_object_array_length(points) > 0) {
            json_object_object_add(obj, "points", points);
        } else {
            json_object_put(points);
        }
    }

    return obj;
}

telemetry_pack_t telemetry_pack_single(const device_data_t *dev)
{
    return telemetry_pack_single_for_slot(0, dev);
}

telemetry_pack_t telemetry_pack_single_for_slot(int slot, const device_data_t *dev)
{
    static uint32_t seq = 0;
    telemetry_pack_t pack;

    memset(&pack, 0, sizeof(pack));
    pack.seq = seq++;
    pack.timestamp_ms = current_time_ms();
    pack.timestamp = (time_t)(pack.timestamp_ms / 1000);
    copy_text(pack.source_id, sizeof(pack.source_id), DEFAULT_SOURCE_ID);
    copy_text(pack.target_id, sizeof(pack.target_id), DEFAULT_TARGET_ID);
    fill_slot_site(&pack.site, slot);
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

    struct json_object *root = json_object_new_object();
    if (!root)
        return -1;

    add_string(root, "type", "telemetry_pack");
    json_object_object_add(root, "version", json_object_new_int(PROTOCOL_VER));
    json_object_object_add(root, "sequence", json_object_new_int64(pack->seq));
    json_object_object_add(root, "timestampMs", json_object_new_int64(pack->timestamp_ms));
    add_string(root, "sourceId", pack->source_id);
    add_string(root, "targetId", pack->target_id);
    json_object_object_add(root, "site", site_to_json(&pack->site));

    /* Keep legacy top-level aliases for Linux_ui. */
    json_object_object_add(root, "ver", json_object_new_int(PROTOCOL_VER));
    json_object_object_add(root, "seq", json_object_new_int64(pack->seq));
    json_object_object_add(root, "time", json_object_new_int64(pack->timestamp));
    json_object_object_add(root, "count", json_object_new_int(pack->device_count));

    struct json_object *devices = json_object_new_array();
    if (!devices) {
        json_object_put(root);
        return -1;
    }

    int count = pack->device_count;
    if (count < 0)
        count = 0;
    if (count > MAX_DEVICES_PER_PACK)
        count = MAX_DEVICES_PER_PACK;

    for (int i = 0; i < count; ++i) {
        struct json_object *dev = pack_device_json(&pack->devices[i]);
        if (dev)
            json_object_array_add(devices, dev);
    }

    json_object_object_add(root, "devices", devices);

    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    int len = json ? (int)strlen(json) : -1;
    if (len < 0 || len + 2 > buf_size) {
        json_object_put(root);
        return -1;
    }

    memcpy(buf, json, (size_t)len);
    buf[len++] = '\n';
    buf[len] = '\0';

    json_object_put(root);
    return len;
}
