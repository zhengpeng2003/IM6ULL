#include "data_ack.h"

#include "data_protocol.h"
#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"
#include "OfflinePublishQueueC.h"

#include <json-c/json.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static int64_t data_ack_current_time_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
        return (int64_t)time(NULL) * 1000;
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static int data_ack_code_from_reason(const char *reason)
{
    if (!reason || reason[0] == '\0')
        return DATA_SEND_OK;
    if (strcmp(reason, "invalid_request") == 0 || strcmp(reason, "invalid_argument") == 0)
        return DATA_SEND_INVALID_ARG;
    if (strcmp(reason, "config_write_failed") == 0)
        return DATA_SEND_IPC_WRITE_FAILED;
    if (strcmp(reason, "mqtt_disconnected") == 0 || strcmp(reason, "mqtt_not_connected") == 0)
        return DATA_SEND_MQTT_NOT_READY;
    if (strcmp(reason, "offline_cache_flush_disabled") == 0)
        return DATA_SEND_OFFLINE_CACHE_DISABLED;
    return ACK_ERROR;
}

static const char *data_ack_port_id_from_slot(int slot)
{
    return slot == 0 ? "port_001" : "port_002";
}

static int data_ack_publish_gateway(struct json_object *root, offline_publish_meta_t *meta)
{
    if (!root)
        return DATA_SEND_JSON_ERROR;

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!payload)
        return DATA_SEND_JSON_ERROR;

    return offline_publish_or_cache(MQTT_DEFAULT_PUBLISH_TOPIC, payload, meta);
}

static int data_ack_publish_port(int slot, struct json_object *root, offline_publish_meta_t *meta)
{
    if (!root)
        return DATA_SEND_JSON_ERROR;

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!payload)
        return DATA_SEND_JSON_ERROR;

    char topic[128];
    if (mqtt_make_port_up_topic_for_slot(slot, topic, sizeof(topic)) != 0)
        return DATA_SEND_INVALID_ARG;

    return offline_publish_or_cache(topic, payload, meta);
}

void data_ack_send(uint32_t seq,
                   const char *cmd,
                   int ok,
                   const char *reason,
                   const char *message)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "boardSeq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "command", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "commandType", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "stage", json_object_new_string("done"));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    json_object_object_add(root, "code", json_object_new_int(ok ? DATA_SEND_OK : data_ack_code_from_reason(reason)));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "timestampMs", json_object_new_int64(data_ack_current_time_ms()));

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ipc_server_send(payload);
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.priority = 3;
    meta.timestamp_ms = 0;
    (void)data_ack_publish_gateway(root, &meta);
    json_object_put(root);
}

void data_ack_send_ports(uint32_t seq,
                         const char *cmd,
                         const char * const *ports,
                         size_t port_count)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    struct json_object *port_array = json_object_new_array();
    if (!port_array) {
        json_object_put(root);
        return;
    }

    for (size_t i = 0; i < port_count; ++i)
        json_object_array_add(port_array, json_object_new_string(ports && ports[i] ? ports[i] : ""));

    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "boardSeq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : "scan_ports"));
    json_object_object_add(root, "command", json_object_new_string(cmd ? cmd : "scan_ports"));
    json_object_object_add(root, "commandType", json_object_new_string(cmd ? cmd : "scan_ports"));
    json_object_object_add(root, "stage", json_object_new_string("done"));
    json_object_object_add(root, "status", json_object_new_string("ok"));
    json_object_object_add(root, "ok", json_object_new_boolean(1));
    json_object_object_add(root, "code", json_object_new_int(DATA_SEND_OK));
    json_object_object_add(root, "reason", json_object_new_string(""));
    json_object_object_add(root, "message", json_object_new_string("scan ports success"));
    json_object_object_add(root, "timestampMs", json_object_new_int64(data_ack_current_time_ms()));
    json_object_object_add(root, "ports", port_array);

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.priority = 3;
    meta.timestamp_ms = data_ack_current_time_ms();
    (void)data_ack_publish_gateway(root, &meta);
    json_object_put(root);
}

void data_ack_send_port_result(uint32_t seq,
                               const char *cmd,
                               int ok,
                               const char *reason,
                               const char *message,
                               int slot,
                               const char *port,
                               const char *device_type,
                               int baud,
                               int connected)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "boardSeq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "command", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "commandType", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "stage", json_object_new_string("done"));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    json_object_object_add(root, "code", json_object_new_int(ok ? DATA_SEND_OK : data_ack_code_from_reason(reason)));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "timestampMs", json_object_new_int64(data_ack_current_time_ms()));
    json_object_object_add(root, "slot", json_object_new_int(slot));
    json_object_object_add(root, "port", json_object_new_string(port ? port : ""));
    json_object_object_add(root, "device_type", json_object_new_string(device_type ? device_type : "unknown"));
    json_object_object_add(root, "baud", json_object_new_int(baud));
    json_object_object_add(root, "connected", json_object_new_boolean(connected));
    if (cmd && strcmp(cmd, "connect_port") == 0)
        json_object_object_add(root, "auto_restore", json_object_new_boolean(1));
    if (cmd && strcmp(cmd, "disconnect_port") == 0) {
        json_object_object_add(root, "config_removed", json_object_new_boolean(0));
        json_object_object_add(root, "auto_restore", json_object_new_boolean(1));
    }

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.port_id = data_ack_port_id_from_slot(slot);
    meta.priority = 3;
    meta.timestamp_ms = data_ack_current_time_ms();
    (void)data_ack_publish_port(slot, root, &meta);
    json_object_put(root);
}


static void data_ack_add_common(struct json_object *root,
                                uint32_t seq,
                                const char *cmd,
                                int ok,
                                const char *reason,
                                const char *message)
{
    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "boardSeq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "command", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "commandType", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "stage", json_object_new_string("done"));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    json_object_object_add(root, "code", json_object_new_int(ok ? DATA_SEND_OK : data_ack_code_from_reason(reason)));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "timestampMs", json_object_new_int64(data_ack_current_time_ms()));
}

static struct json_object *data_ack_thresholds_to_json(const sensor_threshold_config_t *config)
{
    struct json_object *thresholds = json_object_new_object();
    if (!thresholds)
        return NULL;
    if (!config)
        return thresholds;

    struct json_object *temperature = json_object_new_object();
    struct json_object *humidity = json_object_new_object();
    if (temperature) {
        json_object_object_add(temperature, "enable_alarm", json_object_new_boolean(config->temperature.enable_alarm));
        json_object_object_add(temperature, "alarm_low", config->temperature.has_low ? json_object_new_double(config->temperature.alarm_low) : NULL);
        json_object_object_add(temperature, "alarm_high", config->temperature.has_high ? json_object_new_double(config->temperature.alarm_high) : NULL);
        json_object_object_add(thresholds, "temperature", temperature);
    }
    if (humidity) {
        json_object_object_add(humidity, "enable_alarm", json_object_new_boolean(config->humidity.enable_alarm));
        json_object_object_add(humidity, "alarm_low", config->humidity.has_low ? json_object_new_double(config->humidity.alarm_low) : NULL);
        json_object_object_add(humidity, "alarm_high", config->humidity.has_high ? json_object_new_double(config->humidity.alarm_high) : NULL);
        json_object_object_add(thresholds, "humidity", humidity);
    }
    return thresholds;
}

void data_ack_send_relay_result(uint32_t seq,
                                const char *cmd,
                                int ok,
                                const char *reason,
                                const char *message,
                                int slot,
                                int slave_id,
                                int device_id,
                                uint16_t states)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    data_ack_add_common(root, seq, cmd, ok, reason, message);
    json_object_object_add(root, "slot", json_object_new_int(slot));
    json_object_object_add(root, "slave_id", json_object_new_int(slave_id));
    json_object_object_add(root, "deviceId", json_object_new_int(slave_id));
    json_object_object_add(root, "device_id", json_object_new_int(device_id));
    struct json_object *state_array = json_object_new_array();
    if (state_array) {
        for (int bit = 0; bit < 4; ++bit)
            json_object_array_add(state_array, json_object_new_boolean((states & (1u << bit)) != 0));
        json_object_object_add(root, "states", state_array);
    }

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ipc_server_send(payload);
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.priority = 3;
    meta.port_id = data_ack_port_id_from_slot(slot);
    (void)data_ack_publish_port(slot, root, &meta);
    json_object_put(root);
}

void data_ack_send_device_result(uint32_t seq,
                                 const char *cmd,
                                 int ok,
                                 const char *reason,
                                 const char *message,
                                 int slot,
                                 int slave_id,
                                 const char *device_type,
                                 int poll_interval_ms)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    data_ack_add_common(root, seq, cmd, ok, reason, message);
    json_object_object_add(root, "slot", json_object_new_int(slot));
    json_object_object_add(root, "slave_id", json_object_new_int(slave_id));
    json_object_object_add(root, "deviceId", json_object_new_int(slave_id));
    json_object_object_add(root, "deviceType", json_object_new_string(device_type ? device_type : "unknown"));
    json_object_object_add(root, "device_type", json_object_new_string(device_type ? device_type : "unknown"));
    if (poll_interval_ms > 0) {
        json_object_object_add(root, "pollIntervalMs", json_object_new_int(poll_interval_ms));
        json_object_object_add(root, "poll_interval_ms", json_object_new_int(poll_interval_ms));
    }

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ipc_server_send(payload);
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.priority = 3;
    meta.port_id = data_ack_port_id_from_slot(slot);
    (void)data_ack_publish_port(slot, root, &meta);
    json_object_put(root);
}

void data_ack_send_threshold_result(uint32_t seq,
                                    const char *cmd,
                                    int ok,
                                    const char *reason,
                                    const char *message,
                                    int slot,
                                    int slave_id,
                                    const char *device_type,
                                    int threshold_enabled,
                                    const sensor_threshold_config_t *threshold_config)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    data_ack_add_common(root, seq, cmd, ok, reason, message);
    json_object_object_add(root, "slot", json_object_new_int(slot));
    json_object_object_add(root, "slave_id", json_object_new_int(slave_id));
    json_object_object_add(root, "deviceId", json_object_new_int(slave_id));
    json_object_object_add(root, "deviceType", json_object_new_string(device_type ? device_type : "sensor_th"));
    json_object_object_add(root, "device_type", json_object_new_string(device_type ? device_type : "sensor_th"));
    json_object_object_add(root, "threshold_enabled", json_object_new_boolean(threshold_enabled));
    json_object_object_add(root, "thresholdEnabled", json_object_new_boolean(threshold_enabled));
    json_object_object_add(root, "thresholds", data_ack_thresholds_to_json(threshold_config));

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "ack";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.port_id = data_ack_port_id_from_slot(slot);
    meta.priority = 3;
    (void)data_ack_publish_port(slot, root, &meta);
    json_object_put(root);
}

void data_ack_send_offline_cache_config(uint32_t seq,
                                        const char *cmd,
                                        int ok,
                                        const char *reason,
                                        const char *message,
                                        int cache_enabled,
                                        int flush_enabled,
                                        int pending_count)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "boardSeq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "command", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "commandType", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "stage", json_object_new_string("done"));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    json_object_object_add(root, "code", json_object_new_int(ok ? DATA_SEND_OK : data_ack_code_from_reason(reason)));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "timestampMs", json_object_new_int64(data_ack_current_time_ms()));
    json_object_object_add(root, "cache_enabled", json_object_new_boolean(cache_enabled));
    json_object_object_add(root, "cacheEnabled", json_object_new_boolean(cache_enabled));
    json_object_object_add(root, "flush_enabled", json_object_new_boolean(flush_enabled));
    json_object_object_add(root, "flushEnabled", json_object_new_boolean(flush_enabled));
    json_object_object_add(root, "pending_count", json_object_new_int(pending_count));
    json_object_object_add(root, "pendingCount", json_object_new_int(pending_count));

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

const char *data_ack_message_from_reason(const char *reason)
{
    if (!reason || reason[0] == '\0')
        return "";
    if (strcmp(reason, "invalid_request") == 0)
        return "invalid request";
    if (strcmp(reason, "unknown_command") == 0)
        return "unknown command";
    if (strcmp(reason, "invalid_argument") == 0)
        return "invalid argument";
    if (strcmp(reason, "port_not_found") == 0)
        return "port not found";
    if (strcmp(reason, "slave_address_conflict") == 0)
        return "slave address already exists";
    if (strcmp(reason, "unsupported_device_type") == 0)
        return "unsupported device type";
    if (strcmp(reason, "port_already_connected") == 0)
        return "port already connected";
    if (strcmp(reason, "open_failed") == 0 || strcmp(reason, "open_port_failed") == 0)
        return "port open failed";
    if (strcmp(reason, "not_connected") == 0)
        return "not connected";
    if (strcmp(reason, "relay_not_connected") == 0)
        return "relay not connected";
    if (strcmp(reason, "port_not_connected") == 0)
        return "port not connected";
    if (strcmp(reason, "unsupported_device_type") == 0)
        return "unsupported device type";
    if (strcmp(reason, "invalid_poll_interval") == 0)
        return "invalid poll interval";
    if (strcmp(reason, "device_exists") == 0)
        return "device already exists";
    if (strcmp(reason, "device_not_found") == 0)
        return "device not found";
    if (strcmp(reason, "modbus_write_failed") == 0)
        return "modbus write failed";
    if (strcmp(reason, "config_write_failed") == 0)
        return "config write failed";
    return reason;
}
