#include "mqtt_wrapper.h"
#include "mqttclient.h"      // 底层库头文件
#include "mqtt_log.h"
#include <stdio.h>
#include <string.h>

static mqtt_client_t *client = NULL;

int mqtt_wrapper_init(void)
{
    mqtt_log_init();

    client = mqtt_lease();
    if (!client) {
        printf("mqtt_lease failed\n");
        return -1;
    }

    mqtt_set_host(client, "broker.emqx.io");
    mqtt_set_port(client, "1883");
    mqtt_set_client_id(client, "imx6ull-test");
    mqtt_set_clean_session(client, 1);

    if (mqtt_connect(client) < 0) {
        printf("mqtt_connect failed\n");
        return -1;
    }

    printf("MQTT wrapper: connected to broker.emqx.io:1883\n");
    return 0;
}

int mqtt_wrapper_publish(const char *topic, const char *json_payload)
{
    if (!client || !topic || !json_payload) {
        return -1;
    }

    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.qos = QOS0;
    msg.payload = (void *)json_payload;
    msg.payloadlen = strlen(json_payload);

    return mqtt_publish(client, topic, &msg);
}

void mqtt_wrapper_deinit(void)
{
    if (client) {
        mqtt_release(client);
        client = NULL;
    }
}
