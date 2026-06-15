#include "data_command.h"

/* Parses incoming JSON commands and dispatches them to the service/port layer. */
#include "data_ack.h"
#include "data_protocol.h"
#include "data_publish.h"
#include "data_telemetry.h"
#include "mqtt_wrapper.h"
#include "port_manager.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

typedef int (*command_handler_t)(uint32_t seq,
                                 struct json_object *root,
                                 const char *cmd);

typedef struct {
    const char *cmd;
    command_handler_t handler;
} command_entry_t;

static device_type_t parse_device_type(const char *type)
{
    if (!type) return DEV_UNKNOWN;
    if (strcmp(type, "sensor_th") == 0) return DEV_SENSOR_TH;
    if (strcmp(type, "relay") == 0) return DEV_RELAY;
    if (strcmp(type, "sysinfo") == 0) return DEV_SYSINFO;
    return DEV_UNKNOWN;
}

static int slot_from_port_id(const char *port_id)
{
    if (!port_id)
        return -1;
    if (strcmp(port_id, "port_001") == 0 || strcmp(port_id, "RS485-1") == 0)
        return 0;
    if (strcmp(port_id, "port_002") == 0 || strcmp(port_id, "RS485-2") == 0)
        return 1;
    return -1;
}

static int parse_point_threshold(struct json_object *obj, point_threshold_config_t *threshold)
{
    struct json_object *v;

    if (!obj || !threshold)
        return 0;

    memset(threshold, 0, sizeof(*threshold));
    if (json_object_object_get_ex(obj, "enable_alarm", &v) ||
        json_object_object_get_ex(obj, "enableAlarm", &v)) {
        threshold->enable_alarm = json_object_get_boolean(v);
    }

    if (json_object_object_get_ex(obj, "alarm_low", &v) ||
        json_object_object_get_ex(obj, "alarmLow", &v)) {
        if (!json_object_is_type(v, json_type_null)) {
            threshold->has_low = 1;
            threshold->alarm_low = (float)json_object_get_double(v);
        }
    }

    if (json_object_object_get_ex(obj, "alarm_high", &v) ||
        json_object_object_get_ex(obj, "alarmHigh", &v)) {
        if (!json_object_is_type(v, json_type_null)) {
            threshold->has_high = 1;
            threshold->alarm_high = (float)json_object_get_double(v);
        }
    }

    return 1;
}

static int parse_sensor_threshold_config(struct json_object *root,
                                         sensor_threshold_config_t *config)
{
    struct json_object *thresholds;
    struct json_object *point;
    int has_config = 0;

    if (!root || !config)
        return 0;

    memset(config, 0, sizeof(*config));
    if (json_object_object_get_ex(root, "threshold_enabled", &point) ||
        json_object_object_get_ex(root, "thresholdEnabled", &point)) {
        config->threshold_enabled = json_object_get_boolean(point);
        has_config = 1;
    }

    if (!json_object_object_get_ex(root, "thresholds", &thresholds))
        return has_config;

    has_config = 1;
    if (json_object_object_get_ex(thresholds, "temperature", &point))
        parse_point_threshold(point, &config->temperature);
    if (json_object_object_get_ex(thresholds, "humidity", &point))
        parse_point_threshold(point, &config->humidity);

    return has_config;
}

static int handle_scan_ports(uint32_t seq, struct json_object *root, const char *cmd)
{
    (void)root;

    port_manager_scan_ports(seq, cmd);
    return CMD_PROCESS_HANDLED;
}

static int handle_get_runtime_state(uint32_t seq, struct json_object *root, const char *cmd)
{
    (void)root;

    return port_manager_send_runtime_state(seq, cmd) == 0
        ? CMD_PROCESS_HANDLED
        : CMD_PROCESS_ERROR;
}

static int handle_connect_port(uint32_t seq, struct json_object *root, const char *cmd)
{
    int slot = 0;
    int baud = 9600;
    const char *port = "";
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;

    if (json_object_object_get_ex(root, "slot", &v))
        slot = json_object_get_int(v);
    if (json_object_object_get_ex(root, "baud", &v))
        baud = json_object_get_int(v);
    if (json_object_object_get_ex(root, "port", &v))
        port = json_object_get_string(v);

    int ret = port_manager_connect(slot, port, baud, reason, sizeof(reason));
    data_ack_send_port_result(seq,
                              cmd,
                              ret == 0,
                              ret == 0 ? "" : reason,
                              ret == 0 ? "connected" : data_ack_message_from_reason(reason),
                              slot,
                              port,
                              "unknown",
                              baud,
                              ret == 0);
    if (ret == 0)
        data_publish_port_register(seq, slot, port, baud, "connected");
    return ret == 0 ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_disconnect_port(uint32_t seq, struct json_object *root, const char *cmd)
{
    int slot = 0;
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;

    if (json_object_object_get_ex(root, "slot", &v))
        slot = json_object_get_int(v);

    char old_port[128] = "";
    int old_baud = 0;
    int was_connected = 0;
    (void)port_manager_get_port_info(slot, old_port, sizeof(old_port), &old_baud, &was_connected);

    int ret = port_manager_disconnect(slot, reason, sizeof(reason));
    data_ack_send_port_result(seq,
                              cmd,
                              ret == 0,
                              ret == 0 ? "" : reason,
                              ret == 0 ? "port disconnected" : data_ack_message_from_reason(reason),
                              slot,
                              old_port,
                              "unknown",
                              old_baud,
                              0);
    if (ret == 0)
        data_publish_port_register(seq, slot, old_port, old_baud, "disconnected");
    return ret == 0 ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_add_device(uint32_t seq, struct json_object *root, const char *cmd)
{
    int slot = 0;
    int slave_id = 0;
    int poll_interval_ms = 0;
    const char *device_type = "unknown";
    sensor_threshold_config_t threshold_config;
    sensor_threshold_config_t *threshold_config_ptr = NULL;
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;
    struct json_object *target = NULL;
    struct json_object *device = NULL;

    if (json_object_object_get_ex(root, "target", &target) && target) {
        const char *gateway_id = "";
        const char *port_id = "";
        if (json_object_object_get_ex(target, "gatewayId", &v))
            gateway_id = json_object_get_string(v);
        if (json_object_object_get_ex(target, "portId", &v))
            port_id = json_object_get_string(v);
        if (gateway_id && gateway_id[0] != '\0' && strcmp(gateway_id, DEFAULT_GATEWAY_ID) != 0) {
            snprintf(reason, sizeof(reason), "invalid_argument");
            data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
            return CMD_PROCESS_ERROR;
        }
        slot = slot_from_port_id(port_id);
        if (slot < 0) {
            snprintf(reason, sizeof(reason), "port_not_found");
            data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
            return CMD_PROCESS_ERROR;
        }
    } else if (json_object_object_get_ex(root, "slot", &v)) {
        slot = json_object_get_int(v);
    }

    if (!json_object_object_get_ex(root, "device", &device))
        device = root;

    if (json_object_object_get_ex(device, "deviceId", &v))
        slave_id = json_object_get_int(v);
    if (slave_id <= 0 && json_object_object_get_ex(device, "slaveAddress", &v))
        slave_id = json_object_get_int(v);
    if (slave_id <= 0 && json_object_object_get_ex(device, "slave_id", &v))
        slave_id = json_object_get_int(v);
    if (json_object_object_get_ex(device, "deviceType", &v))
        device_type = json_object_get_string(v);
    if (strcmp(device_type, "unknown") == 0 && json_object_object_get_ex(device, "device_type", &v))
        device_type = json_object_get_string(v);
    if (json_object_object_get_ex(device, "pollIntervalMs", &v))
        poll_interval_ms = json_object_get_int(v);
    if (poll_interval_ms <= 0 && json_object_object_get_ex(device, "poll_interval_ms", &v))
        poll_interval_ms = json_object_get_int(v);
    if (parse_sensor_threshold_config(device, &threshold_config))
        threshold_config_ptr = &threshold_config;

    int ret = port_manager_add_device_ex(slot,
                                         slave_id,
                                         device_type,
                                         poll_interval_ms,
                                         threshold_config_ptr,
                                         reason,
                                         sizeof(reason));
    data_ack_send_device_result(seq,
                                cmd,
                                ret == 0,
                                ret == 0 ? "" : reason,
                                ret == 0 ? "device added" : data_ack_message_from_reason(reason),
                                slot,
                                slave_id,
                                device_type,
                                poll_interval_ms);
    if (ret == 0) {
        int publish_ret = data_publish_device_register(seq,
                                                       slot,
                                                       slave_id,
                                                       device_type,
                                                       poll_interval_ms);
        if (publish_ret != DATA_SEND_OK) {
            printf("device_register publish failed, code=%d\n", publish_ret);
        }
    }
    return ret == 0 ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_set_device_threshold(uint32_t seq, struct json_object *root, const char *cmd)
{
    int slot = 0;
    int slave_id = 0;
    sensor_threshold_config_t threshold_config;
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;

    if (json_object_object_get_ex(root, "slot", &v))
        slot = json_object_get_int(v);
    if (json_object_object_get_ex(root, "slave_id", &v))
        slave_id = json_object_get_int(v);

    if (!parse_sensor_threshold_config(root, &threshold_config)) {
        snprintf(reason, sizeof(reason), "invalid_request");
        data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
        return CMD_PROCESS_ERROR;
    }

    int ret = port_manager_set_device_threshold(slot,
                                                slave_id,
                                                &threshold_config,
                                                reason,
                                                sizeof(reason));
    data_ack_send_threshold_result(seq,
                                   cmd,
                                   ret == 0,
                                   ret == 0 ? "" : reason,
                                   ret == 0 ? "threshold updated" : data_ack_message_from_reason(reason),
                                   slot,
                                   slave_id,
                                   "sensor_th",
                                   threshold_config.threshold_enabled,
                                   &threshold_config);
    return ret == 0 ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_remove_device(uint32_t seq, struct json_object *root, const char *cmd)
{
    int slot = 0;
    int slave_id = 0;
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;
    struct json_object *target = NULL;

    if (json_object_object_get_ex(root, "target", &target) && target) {
        const char *gateway_id = "";
        const char *port_id = "";
        if (json_object_object_get_ex(target, "gatewayId", &v))
            gateway_id = json_object_get_string(v);
        if (json_object_object_get_ex(target, "portId", &v))
            port_id = json_object_get_string(v);
        if (gateway_id && gateway_id[0] != '\0' && strcmp(gateway_id, DEFAULT_GATEWAY_ID) != 0) {
            snprintf(reason, sizeof(reason), "invalid_argument");
            data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
            return CMD_PROCESS_ERROR;
        }
        slot = slot_from_port_id(port_id);
        if (slot < 0) {
            snprintf(reason, sizeof(reason), "port_not_found");
            data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
            return CMD_PROCESS_ERROR;
        }
    }

    if (json_object_object_get_ex(root, "slot", &v))
        slot = json_object_get_int(v);
    if (json_object_object_get_ex(root, "slave_id", &v))
        slave_id = json_object_get_int(v);
    if (slave_id <= 0 && json_object_object_get_ex(root, "deviceId", &v))
        slave_id = json_object_get_int(v);

    const char *device_type = "unknown";
    if (json_object_object_get_ex(root, "deviceType", &v))
        device_type = json_object_get_string(v);
    if (strcmp(device_type, "unknown") == 0 && json_object_object_get_ex(root, "device_type", &v))
        device_type = json_object_get_string(v);

    int ret = port_manager_remove_device(slot, slave_id, reason, sizeof(reason));
    data_ack_send_device_result(seq,
                                cmd,
                                ret == 0,
                                ret == 0 ? "" : reason,
                                ret == 0 ? "device removed" : data_ack_message_from_reason(reason),
                                slot,
                                slave_id,
                                device_type,
                                0);
    return ret == 0 ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_set_relay(uint32_t seq, struct json_object *root, const char *cmd)
{
    char reason[MAX_ACK_MSG_LEN] = "";
    struct json_object *v;
    int slot = 0;
    int slave_id = 1;

    device_data_t dev;
    memset(&dev, 0, sizeof(dev));
    dev.device_id = 1;
    dev.type = DEV_RELAY;
    dev.valid = 1;

    if (json_object_object_get_ex(root, "slot", &v))
        slot = json_object_get_int(v);
    if (json_object_object_get_ex(root, "slave_id", &v))
        slave_id = json_object_get_int(v);
    if (json_object_object_get_ex(root, "device_id", &v))
        dev.device_id = json_object_get_int(v);
    else
        dev.device_id = slave_id;

    if (!json_object_object_get_ex(root, "states", &v)) {
        snprintf(reason, sizeof(reason), "invalid_request");
        data_ack_send_relay_result(seq, cmd, 0, reason, "invalid relay command", slot, slave_id, dev.device_id, 0);
        return CMD_PROCESS_ERROR;
    }
    if (json_object_is_type(v, json_type_array)) {
        uint16_t mask = 0;
        const int len = json_object_array_length(v);
        if (len <= 0) {
            snprintf(reason, sizeof(reason), "invalid_request");
            data_ack_send_relay_result(seq, cmd, 0, reason, "invalid relay command", slot, slave_id, dev.device_id, 0);
            return CMD_PROCESS_ERROR;
        }
        for (int i = 0; i < len && i < 16; ++i) {
            if (json_object_get_boolean(json_object_array_get_idx(v, i)))
                mask |= (uint16_t)(1u << i);
        }
        dev.data.relay.relay_states = mask;
    } else {
        dev.data.relay.relay_states = (uint16_t)json_object_get_int(v);
    }

    int ret = port_manager_handle_relay(slot, slave_id, &dev, reason, sizeof(reason));
    if (ret != 0 && strcmp(reason, "relay_not_connected") == 0)
        snprintf(reason, sizeof(reason), "relay_not_found");
    const char *ack_message = ret == 0 ? "relay write success" : data_ack_message_from_reason(reason);
    if (ret != 0 && strcmp(reason, "relay_not_found") == 0)
        ack_message = "relay device not found";
    if (ret != 0 && strcmp(reason, "port_not_connected") == 0)
        ack_message = "port is not connected";
    if (ret != 0 && strcmp(reason, "modbus_write_failed") == 0)
        ack_message = "relay write failed";
    data_ack_send_relay_result(seq,
                               cmd,
                               ret == 0,
                               ret == 0 ? "" : reason,
                               ack_message,
                               slot,
                               slave_id,
                               dev.device_id,
                               dev.data.relay.relay_states);
    return ret == 0 ? CMD_PROCESS_FORWARD_MQTT : CMD_PROCESS_ERROR;
}

static int handle_get_config(uint32_t seq, struct json_object *root, const char *cmd)
{
    char snapshot[65536];
    char reason[MAX_ACK_MSG_LEN] = "";
    const char *gateway_id = DEFAULT_GATEWAY_ID;
    const char *target_json = "";
    struct json_object *target = NULL;
    struct json_object *v = NULL;

    if (json_object_object_get_ex(root, "target", &target) && target) {
        if (json_object_object_get_ex(target, "gatewayId", &v))
            gateway_id = json_object_get_string(v);
        target_json = json_object_to_json_string_ext(target, JSON_C_TO_STRING_PLAIN);
    }

    if (gateway_id && gateway_id[0] != '\0' && strcmp(gateway_id, DEFAULT_GATEWAY_ID) != 0) {
        snprintf(reason, sizeof(reason), "invalid_argument");
        data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
        return CMD_PROCESS_ERROR;
    }

    if (port_manager_export_config_snapshot(seq,
                                            DEFAULT_GATEWAY_ID,
                                            target_json,
                                            snapshot,
                                            sizeof(snapshot)) != 0) {
        snprintf(reason, sizeof(reason), "config_snapshot_failed");
        data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
        return CMD_PROCESS_ERROR;
    }

    int ret = mqtt_send(MQTT_DEFAULT_PUBLISH_TOPIC, snapshot);
    if (ret != DATA_SEND_OK) {
        snprintf(reason, sizeof(reason), "mqtt_publish_failed");
        data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
        return CMD_PROCESS_ERROR;
    }

    return CMD_PROCESS_HANDLED;
}

static int handle_get_offline_cache_config(uint32_t seq, struct json_object *root, const char *cmd)
{
    (void)root;
    int cache_enabled = 0;
    int flush_enabled = 0;
    if (port_manager_load_offline_cache_config(&cache_enabled, &flush_enabled) == 0) {
        mqtt_set_offline_cache_enabled(cache_enabled);
        mqtt_set_offline_cache_flush_enabled(flush_enabled);
    }

    data_ack_send_offline_cache_config(seq,
                                       cmd,
                                       1,
                                       "",
                                       "offline cache config loaded",
                                       mqtt_offline_cache_enabled(),
                                       mqtt_offline_cache_flush_enabled(),
                                       mqtt_offline_cache_pending_count());
    return CMD_PROCESS_HANDLED;
}

static int handle_set_offline_cache_config(uint32_t seq, struct json_object *root, const char *cmd)
{
    struct json_object *v = NULL;
    int cache_enabled = mqtt_offline_cache_enabled();
    int flush_enabled = mqtt_offline_cache_flush_enabled();

    if (json_object_object_get_ex(root, "cache_enabled", &v) ||
        json_object_object_get_ex(root, "cacheEnabled", &v)) {
        cache_enabled = json_object_get_boolean(v) ? 1 : 0;
    }

    if (json_object_object_get_ex(root, "flush_enabled", &v) ||
        json_object_object_get_ex(root, "flushEnabled", &v)) {
        flush_enabled = json_object_get_boolean(v) ? 1 : 0;
    }

    if (!cache_enabled)
        flush_enabled = 0;

    const int saved = port_manager_save_offline_cache_config(cache_enabled, flush_enabled) == 0;
    if (saved) {
        mqtt_set_offline_cache_enabled(cache_enabled);
        mqtt_set_offline_cache_flush_enabled(flush_enabled);
    }

    data_ack_send_offline_cache_config(seq,
                                       cmd,
                                       saved,
                                       saved ? "" : "offline_cache_config_save_failed",
                                       saved ? "offline cache config saved" : "offline cache config save failed",
                                       mqtt_offline_cache_enabled(),
                                       mqtt_offline_cache_flush_enabled(),
                                       mqtt_offline_cache_pending_count());
    return saved ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_clear_offline_cache(uint32_t seq, struct json_object *root, const char *cmd)
{
    (void)root;
    const int ok = mqtt_clear_offline_cache() == 0;
    data_ack_send_offline_cache_config(seq,
                                       cmd,
                                       ok,
                                       ok ? "" : "offline_cache_clear_failed",
                                       ok ? "pending offline cache cleared" : "offline cache clear failed",
                                       mqtt_offline_cache_enabled(),
                                       mqtt_offline_cache_flush_enabled(),
                                       mqtt_offline_cache_pending_count());
    return ok ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static int handle_flush_offline_cache(uint32_t seq, struct json_object *root, const char *cmd)
{
    (void)root;
    int ret = mqtt_flush_offline_cache_once();
    const int ok = ret == DATA_SEND_OK;
    const char *reason = "offline_cache_flush_failed";
    const char *message = "offline cache flush failed";
    if (ret == DATA_SEND_MQTT_NOT_READY) {
        reason = "mqtt_disconnected";
        message = "MQTT is disconnected";
    } else if (ret == DATA_SEND_OFFLINE_CACHE_DISABLED) {
        reason = "offline_cache_flush_disabled";
        message = "offline cache flush is disabled";
    }

    data_ack_send_offline_cache_config(seq,
                                       cmd,
                                       ok,
                                       ok ? "" : reason,
                                       ok ? "offline cache flush once triggered" : message,
                                       mqtt_offline_cache_enabled(),
                                       mqtt_offline_cache_flush_enabled(),
                                       mqtt_offline_cache_pending_count());
    return ok ? CMD_PROCESS_HANDLED : CMD_PROCESS_ERROR;
}

static const command_entry_t command_table[] = {
    {"scan_ports", handle_scan_ports},
    {"get_runtime_state", handle_get_runtime_state},
    {"connect_port", handle_connect_port},
    {"disconnect_port", handle_disconnect_port},
    {"add_device", handle_add_device},
    {"set_device_threshold", handle_set_device_threshold},
    {"remove_device", handle_remove_device},
    {"set_relay", handle_set_relay},
    {"get_config", handle_get_config},
    {"get_offline_cache_config", handle_get_offline_cache_config},
    {"set_offline_cache_config", handle_set_offline_cache_config},
    {"clear_offline_cache", handle_clear_offline_cache},
    {"flush_offline_cache", handle_flush_offline_cache},
};

static int process_command_message(uint32_t seq, struct json_object *root, const char *cmd)
{
    if (!cmd) {
        data_ack_send(seq, "", 0, "invalid_request", data_ack_message_from_reason("invalid_request"));
        return CMD_PROCESS_ERROR;
    }

    for (size_t i = 0; i < sizeof(command_table) / sizeof(command_table[0]); ++i) {
        if (strcmp(cmd, command_table[i].cmd) == 0)
            return command_table[i].handler(seq, root, cmd);
    }

    char reason[MAX_ACK_MSG_LEN] = "";
    snprintf(reason, sizeof(reason), "unknown_command");
    data_ack_send(seq, cmd, 0, reason, data_ack_message_from_reason(reason));
    return CMD_PROCESS_ERROR;
}

static int process_device_message(uint32_t seq, struct json_object *root)
{
    int ret = CMD_PROCESS_HANDLED;
    int handled_relay = 0;
    char reason[MAX_ACK_MSG_LEN] = "";

    struct json_object *devices;
    if (json_object_object_get_ex(root, "devices", &devices)) {
        int count = json_object_array_length(devices);
        for (int i = 0; i < count; i++) {
            struct json_object *d = json_object_array_get_idx(devices, i);
            struct json_object *type_obj = json_object_object_get(d, "type");
            device_type_t type = parse_device_type(json_object_get_string(type_obj));

            switch (type) {
            case DEV_RELAY: {
                int slot = 0;
                int slave_id = json_object_get_int(json_object_object_get(d, "id"));
                struct json_object *slot_obj = json_object_object_get(d, "slot");
                struct json_object *slave_obj = json_object_object_get(d, "slave_id");
                if (slot_obj)
                    slot = json_object_get_int(slot_obj);
                if (slave_obj)
                    slave_id = json_object_get_int(slave_obj);

                device_data_t dev;
                memset(&dev, 0, sizeof(dev));
                dev.device_id = slave_id;
                dev.type = DEV_RELAY;
                dev.valid = json_object_get_boolean(json_object_object_get(d, "valid"));
                dev.data.relay.relay_states =
                    json_object_get_int(json_object_object_get(d, "states"));

                handled_relay = 1;
                if (port_manager_handle_relay(slot, slave_id, &dev, reason, sizeof(reason)) != 0)
                    ret = CMD_PROCESS_ERROR;
                break;
            }

            case DEV_SENSOR_TH: {
                device_data_t dev;
                memset(&dev, 0, sizeof(dev));
                dev.device_id = json_object_get_int(json_object_object_get(d, "id"));
                dev.type = DEV_SENSOR_TH;
                dev.valid = json_object_get_boolean(json_object_object_get(d, "valid"));
                dev.data.th.temperature =
                    json_object_get_double(json_object_object_get(d, "temp"));
                dev.data.th.humidity =
                    json_object_get_double(json_object_object_get(d, "humi"));

                (void)data_publish_device_status(&dev);
                break;
            }

            case DEV_SYSINFO:
                break;

            default:
                break;
            }
        }
    }

    if (handled_relay) {
        data_ack_send(seq,
                      "relay",
                      ret != CMD_PROCESS_ERROR,
                      ret != CMD_PROCESS_ERROR ? "" : reason,
                      ret != CMD_PROCESS_ERROR ? "relay write success" : data_ack_message_from_reason(reason));
        if (ret != CMD_PROCESS_ERROR)
            ret = CMD_PROCESS_FORWARD_MQTT;
    }

    return ret;
}

int data_command_process_message(const char *json_str)
{
    if (!json_str)
        return CMD_PROCESS_ERROR;

    struct json_object *root = json_tokener_parse(json_str);
    if (!root)
        return CMD_PROCESS_ERROR;

    uint32_t seq = 0;
    struct json_object *seq_obj;
    if (json_object_object_get_ex(root, "seq", &seq_obj))
        seq = (uint32_t)json_object_get_int64(seq_obj);

    int ret = CMD_PROCESS_ERROR;
    const char *msg_type = "";
    struct json_object *type_obj;
    if (json_object_object_get_ex(root, "type", &type_obj))
        msg_type = json_object_get_string(type_obj);

    struct json_object *cmd_obj = NULL;
    if (!json_object_object_get_ex(root, "cmd", &cmd_obj))
        json_object_object_get_ex(root, "commandType", &cmd_obj);
    if (cmd_obj) {
        if (msg_type && msg_type[0] != '\0' && strcmp(msg_type, "command") != 0) {
            data_ack_send(seq, json_object_get_string(cmd_obj), 0,
                          "invalid_request",
                          data_ack_message_from_reason("invalid_request"));
            json_object_put(root);
            return CMD_PROCESS_ERROR;
        }
        ret = process_command_message(seq, root, json_object_get_string(cmd_obj));
    } else {
        if (msg_type && msg_type[0] != '\0') {
            data_ack_send(seq, "", 0,
                          "invalid_request",
                          data_ack_message_from_reason("invalid_request"));
            ret = CMD_PROCESS_ERROR;
        } else {
            ret = process_device_message(seq, root);
        }
    }

    json_object_put(root);
    return ret;
}
