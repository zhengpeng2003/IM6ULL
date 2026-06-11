#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern const char MQTT_DEFAULT_PUBLISH_TOPIC[];
int  mqtt_init(void);
void mqtt_poll(void);
int  mqtt_send(const char *topic, const char *payload);
int  mqtt_is_connected(void);
int  mqtt_send_direct_if_connected(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif
