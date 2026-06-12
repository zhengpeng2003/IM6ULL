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
void mqtt_set_offline_cache_enabled(int enabled);
int  mqtt_offline_cache_enabled(void);
void mqtt_set_offline_cache_flush_enabled(int enabled);
int  mqtt_offline_cache_flush_enabled(void);
int  mqtt_offline_cache_pending_count(void);
int  mqtt_clear_offline_cache(void);
int  mqtt_flush_offline_cache_once(void);

#ifdef __cplusplus
}
#endif
