#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct offline_publish_meta {
    const char *message_type;
    const char *gateway_id;
    const char *port_id;
    int device_id;
    const char *point_key;
    int priority;
    int has_alarm;
    int has_invalid_data;
    int status_changed;
    int64_t timestamp_ms;
} offline_publish_meta_t;

typedef int (*offline_mqtt_sender_fn)(const char *topic, const char *payload);

int offline_publish_queue_init(const char *db_path);
void offline_publish_queue_set_sender(offline_mqtt_sender_fn sender);
int offline_publish_or_cache(const char *topic,
                             const char *payload,
                             const offline_publish_meta_t *meta);
int offline_publish_cache_message(const char *topic,
                                  const char *payload,
                                  const offline_publish_meta_t *meta);
void offline_publish_flush_once(void);
int offline_publish_pending_count(void);

#ifdef __cplusplus
}
#endif
