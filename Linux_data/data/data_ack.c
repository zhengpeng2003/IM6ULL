#include "data_ack.h"

#include "ipc_server.h"

#include <json-c/json.h>
#include <string.h>

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
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

void data_ack_send_alarm_config(uint32_t seq,
                                const char *cmd,
                                int ok,
                                const char *reason,
                                const char *message,
                                float temp_high,
                                float humi_high)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("ack"));
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "temp_high", json_object_new_double(temp_high));
    json_object_object_add(root, "humi_high", json_object_new_double(humi_high));

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
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
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : "scan_ports"));
    json_object_object_add(root, "status", json_object_new_string("ok"));
    json_object_object_add(root, "reason", json_object_new_string(""));
    json_object_object_add(root, "message", json_object_new_string("scan ports success"));
    json_object_object_add(root, "ports", port_array);

    ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
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
    json_object_object_add(root, "cmd", json_object_new_string(cmd ? cmd : ""));
    json_object_object_add(root, "status", json_object_new_string(ok ? "ok" : "failed"));
    json_object_object_add(root, "reason", json_object_new_string(reason ? reason : ""));
    json_object_object_add(root, "message", json_object_new_string(message ? message : ""));
    json_object_object_add(root, "slot", json_object_new_int(slot));
    json_object_object_add(root, "port", json_object_new_string(port ? port : ""));
    json_object_object_add(root, "device_type", json_object_new_string(device_type ? device_type : "unknown"));
    json_object_object_add(root, "baud", json_object_new_int(baud));
    json_object_object_add(root, "connected", json_object_new_boolean(connected));

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
    if (strcmp(reason, "port_already_connected") == 0)
        return "port already connected";
    if (strcmp(reason, "open_failed") == 0)
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
