#include "data_command.h"

#include "data_protocol.h"
#include "port_manager.h"
#include "service.h"

#include <json-c/json.h>
#include <string.h>

void data_command_process_message(const char *json_str)
{
    if (!json_str)
        return;

    struct json_object *root = json_tokener_parse(json_str);
    if (!root)
        return;

    struct json_object *cmd_obj;
    if (json_object_object_get_ex(root, "cmd", &cmd_obj)) {
        const char *cmd = json_object_get_string(cmd_obj);

        if (strcmp(cmd, "scan_ports") == 0) {
            port_manager_scan_ports();
        } else if (strcmp(cmd, "connect_port") == 0) {
            int slot = 0;
            int baud = 9600;
            const char *port = "";
            const char *device_type = "unknown";
            struct json_object *v;

            if (json_object_object_get_ex(root, "slot", &v))
                slot = json_object_get_int(v);
            if (json_object_object_get_ex(root, "baud", &v))
                baud = json_object_get_int(v);
            if (json_object_object_get_ex(root, "port", &v))
                port = json_object_get_string(v);
            if (json_object_object_get_ex(root, "device_type", &v))
                device_type = json_object_get_string(v);

            port_manager_connect(slot, port, device_type, baud);
        } else if (strcmp(cmd, "disconnect_port") == 0) {
            int slot = 0;
            struct json_object *v;

            if (json_object_object_get_ex(root, "slot", &v))
                slot = json_object_get_int(v);
            port_manager_disconnect(slot);
        }

        json_object_put(root);
        return;
    }

    struct json_object *devices;
    if (json_object_object_get_ex(root, "devices", &devices)) {
        int count = json_object_array_length(devices);
        for (int i = 0; i < count; i++) {
            struct json_object *d = json_object_array_get_idx(devices, i);
            struct json_object *type_obj = json_object_object_get(d, "type");
            int type = json_object_get_int(type_obj);

            switch (type) {
            case DEV_RELAY: {
                device_data_t dev;
                memset(&dev, 0, sizeof(dev));
                dev.device_id = json_object_get_int(json_object_object_get(d, "id"));
                dev.type = DEV_RELAY;
                dev.valid = json_object_get_boolean(json_object_object_get(d, "valid"));
                dev.data.relay.relay_states =
                    json_object_get_int(json_object_object_get(d, "states"));

                service_handle_relay(&dev);
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

                service_handle_sensor(&dev);
                break;
            }

            case DEV_SYSINFO:
                break;

            default:
                break;
            }
        }
    }

    json_object_put(root);
}
