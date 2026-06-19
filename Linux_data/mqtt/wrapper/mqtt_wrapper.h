#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern const char MQTT_DEFAULT_PUBLISH_TOPIC[];
extern const char MQTT_GATEWAY_REGISTER_TOPIC[];
extern const char MQTT_GATEWAY_COMMAND_WILDCARD_TOPIC[];
int  mqtt_init(void);
void mqtt_poll(void);
int  mqtt_send(const char *topic, const char *payload);
int  mqtt_is_connected(void);
int  mqtt_send_direct_if_connected(const char *topic, const char *payload);
int  mqtt_make_gateway_up_topic(char *buffer, int buffer_size);
int  mqtt_make_port_up_topic(const char *port_id, char *buffer, int buffer_size);
int  mqtt_make_port_up_topic_for_slot(int slot, char *buffer, int buffer_size);
int  mqtt_make_gateway_command_topic(char *buffer, int buffer_size);
int  mqtt_make_port_command_topic(const char *port_id, char *buffer, int buffer_size);
int  mqtt_parse_gateway_command_topic(const char *topic, char *gateway_id, int gateway_size);
int  mqtt_parse_port_command_topic(const char *topic, char *gateway_id, int gateway_size, char *port_id, int port_size);
int  mqtt_parse_port_up_topic(const char *topic, char *gateway_id, int gateway_size, char *port_id, int port_size);
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
