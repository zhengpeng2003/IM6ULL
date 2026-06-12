#include "OfflinePublishQueueC.h"

#include "OfflinePublishQueue.hpp"

#include <string>

namespace {

const char kDefaultOfflineDbPath[] = "/etc/qt_object/offline_cache.db";

OfflinePublishQueue &queue()
{
    static OfflinePublishQueue instance;
    return instance;
}

std::string safeString(const char *value)
{
    return value ? value : "";
}

PublishMeta toPublishMeta(const offline_publish_meta_t *meta)
{
    PublishMeta out;
    if (!meta)
        return out;

    out.messageType = safeString(meta->message_type);
    out.gatewayId = safeString(meta->gateway_id);
    out.portId = safeString(meta->port_id);
    out.deviceId = meta->device_id;
    out.pointKey = safeString(meta->point_key);
    out.priority = meta->priority;
    out.hasAlarm = meta->has_alarm != 0;
    out.hasInvalidData = meta->has_invalid_data != 0;
    out.statusChanged = meta->status_changed != 0;
    out.timestampMs = meta->timestamp_ms;
    return out;
}

} // namespace

int offline_publish_queue_init(const char *db_path)
{
    return queue().init(db_path && db_path[0] ? db_path : kDefaultOfflineDbPath) ? 0 : -1;
}

void offline_publish_queue_set_sender(offline_mqtt_sender_fn sender)
{
    queue().setSender(sender);
}

int offline_publish_or_cache(const char *topic,
                             const char *payload,
                             const offline_publish_meta_t *meta)
{
    if (!topic || !payload)
        return -1;

    return queue().publishOrCache(topic, payload, toPublishMeta(meta));
}

int offline_publish_cache_message(const char *topic,
                                  const char *payload,
                                  const offline_publish_meta_t *meta)
{
    if (!topic || !payload)
        return -1;

    return queue().cacheMessage(topic, payload, toPublishMeta(meta)) ? 0 : -1;
}

void offline_publish_flush_once(void)
{
    queue().flushToMqttOnce();
}

int offline_publish_pending_count(void)
{
    return queue().countPending();
}

int offline_publish_clear_pending(void)
{
    return queue().clearPending() ? 0 : -1;
}

void offline_publish_set_cache_enabled(int enabled)
{
    queue().setCacheEnabled(enabled != 0);
}

int offline_publish_cache_enabled(void)
{
    return queue().cacheEnabled() ? 1 : 0;
}

void offline_publish_set_flush_enabled(int enabled)
{
    queue().setFlushEnabled(enabled != 0);
}

int offline_publish_flush_enabled(void)
{
    return queue().flushEnabled() ? 1 : 0;
}
