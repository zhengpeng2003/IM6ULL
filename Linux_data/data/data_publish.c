#include "data_publish.h"

#include "alarm_config.h"
#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"

void data_publish_device_status(const device_data_t *dev)
{
    if (!dev)
        return;

    char json[512];
    telemetry_pack_t pack = telemetry_pack_single(dev);
    int len = telemetry_pack_to_json(&pack, json, sizeof(json));
    if (len <= 0)
        return;

    ipc_server_send(json);
    mqtt_send("imx6ull/device/data", json);

    if (dev->type == DEV_SENSOR_TH && dev->valid)
        alarm_config_check_sensor(dev->data.th.temperature, dev->data.th.humidity);
}
