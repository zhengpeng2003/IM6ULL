#include "data_publish.h"

#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"

#include <json-c/json.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static int64_t current_time_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
        return (int64_t)time(NULL) * 1000;

    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void add_string(struct json_object *obj, const char *key, const char *value)
{
    json_object_object_add(obj, key, json_object_new_string(value ? value : ""));
}

static const char *port_id_from_slot(int slot)
{
    return slot == 0 ? "port_001" : "port_002";
}

static const char *port_name_from_slot(int slot)
{
    return slot == 0 ? "RS485-1" : "RS485-2";
}

static int device_type_expects_telemetry(const char *device_type)
{
    if (!device_type)
        return 1;

    return strcmp(device_type, "relay") != 0 && strcmp(device_type, "led") != 0;
}

static const char *send_code_message(int code)
{
    switch (code) {
    case DATA_SEND_OK:
        return "send success";
    case DATA_SEND_INVALID_ARG:
        return "invalid argument";
    case DATA_SEND_JSON_ERROR:
        return "json build failed";
    case DATA_SEND_IPC_NO_CLIENT:
        return "ipc client not connected";
    case DATA_SEND_IPC_WRITE_FAILED:
        return "ipc write failed";
    case DATA_SEND_MQTT_NOT_READY:
        return "mqtt not ready";
    case DATA_SEND_MQTT_QUEUE_FULL:
        return "mqtt queue full";
    case DATA_SEND_PARTIAL_FAILED:
        return "partial send failed";
    default:
        return "send failed";
    }
}

static int merge_send_code(int ipc_code, int mqtt_code)
{
    if (ipc_code == DATA_SEND_OK && mqtt_code == DATA_SEND_OK)
        return DATA_SEND_OK;
    if (ipc_code != DATA_SEND_OK && mqtt_code != DATA_SEND_OK)
        return DATA_SEND_PARTIAL_FAILED;
    if (ipc_code != DATA_SEND_OK)
        return ipc_code;
    return mqtt_code;
}

static void send_publish_ack(uint32_t seq, int code, int ipc_code, int mqtt_code)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("publish_ack"));
    json_object_object_add(root, "sequence", json_object_new_int64(seq));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "status", json_object_new_string(code == DATA_SEND_OK ? "ok" : "failed"));
    json_object_object_add(root, "code", json_object_new_int(code));
    json_object_object_add(root, "ipcCode", json_object_new_int(ipc_code));
    json_object_object_add(root, "mqttCode", json_object_new_int(mqtt_code));
    json_object_object_add(root, "message", json_object_new_string(send_code_message(code)));

    (void)ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

int data_publish_device_status(const device_data_t *dev)
{
    if (!dev)
        return DATA_SEND_INVALID_ARG;

    char json[4096];
    telemetry_pack_t pack = telemetry_pack_single(dev);
    int len = telemetry_pack_to_json(&pack, json, sizeof(json));
    if (len <= 0) {
        send_publish_ack(pack.seq, DATA_SEND_JSON_ERROR, DATA_SEND_JSON_ERROR, DATA_SEND_JSON_ERROR);
        return DATA_SEND_JSON_ERROR;
    }

    int ipc_code = ipc_server_send(json);
    int mqtt_code = mqtt_send(MQTT_DEFAULT_PUBLISH_TOPIC, json);
    int code = merge_send_code(ipc_code, mqtt_code);

    send_publish_ack(pack.seq, code, ipc_code, mqtt_code);

    return code;
}

int data_publish_device_register(uint32_t seq,
                                 int slot,
                                 int slave_id,
                                 const char *device_type,
                                 int poll_interval_ms)
{
    if (slave_id <= 0 || !device_type || device_type[0] == '\0')
        return DATA_SEND_INVALID_ARG;

    struct json_object *root = json_object_new_object();
    if (!root)
        return DATA_SEND_JSON_ERROR;

    const int64_t now_ms = current_time_ms();
    char device_name[MAX_DEVICE_NAME_LEN];
    snprintf(device_name, sizeof(device_name), "Device %d", slave_id);

    add_string(root, "type", "device_register");
    json_object_object_add(root, "sequence", json_object_new_int64(seq));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "timestampMs", json_object_new_int64(now_ms));
    add_string(root, "sourceId", DEFAULT_SOURCE_ID);
    add_string(root, "targetId", DEFAULT_TARGET_ID);

    struct json_object *site = json_object_new_object();
    if (!site) {
        json_object_put(root);
        return DATA_SEND_JSON_ERROR;
    }

    add_string(site, "factoryId", DEFAULT_FACTORY_ID);
    add_string(site, "factoryName", DEFAULT_FACTORY_NAME);
    add_string(site, "areaId", DEFAULT_AREA_ID);
    add_string(site, "areaName", DEFAULT_AREA_NAME);
    add_string(site, "gatewayId", DEFAULT_GATEWAY_ID);
    add_string(site, "gatewayName", DEFAULT_GATEWAY_NAME);
    add_string(site, "portId", port_id_from_slot(slot));
    add_string(site, "portName", port_name_from_slot(slot));
    json_object_object_add(root, "site", site);

    json_object_object_add(root, "deviceId", json_object_new_int(slave_id));
    add_string(root, "deviceName", device_name);
    add_string(root, "deviceType", device_type);
    json_object_object_add(root, "pollIntervalMs", json_object_new_int(poll_interval_ms > 0 ? poll_interval_ms : 1000));
    json_object_object_add(root,
                           "expectTelemetry",
                           json_object_new_boolean(device_type_expects_telemetry(device_type)));

    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!json) {
        json_object_put(root);
        return DATA_SEND_JSON_ERROR;
    }

    int ret = mqtt_send(MQTT_DEFAULT_PUBLISH_TOPIC, json);
    json_object_put(root);
    return ret;
}
