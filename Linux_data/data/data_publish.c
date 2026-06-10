#include "data_publish.h"

#include "alarm_config.h"
#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"

#include <json-c/json.h>

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

    if (dev->type == DEV_SENSOR_TH && dev->valid)
        alarm_config_check_sensor(dev->data.th.temperature, dev->data.th.humidity);

    return code;
}
