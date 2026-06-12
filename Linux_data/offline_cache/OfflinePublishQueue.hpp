#pragma once

#include "OfflineCacheDatabase.hpp"
#include "OfflineCachePolicy.hpp"

#include <mutex>
#include <string>

typedef int (*OfflineMqttSender)(const char *topic, const char *payload);

class OfflinePublishQueue {
public:
    bool init(const std::string &dbPath);
    void setSender(OfflineMqttSender sender);

    int publishOrCache(const std::string &topic,
                       const std::string &payload,
                       const PublishMeta &meta);
    bool cacheMessage(const std::string &topic,
                      const std::string &payload,
                      const PublishMeta &meta);
    void flushToMqttOnce();
    int countPending();
    bool clearPending();
    void setCacheEnabled(bool enabled);
    bool cacheEnabled() const;
    void setFlushEnabled(bool enabled);
    bool flushEnabled() const;

private:
    OfflineCacheDatabase db_;
    OfflineCachePolicy policy_;
    OfflineMqttSender sender_ = nullptr;
    bool initialized_ = false;
    bool cache_enabled_ = true;
    bool flush_enabled_ = false;
    mutable std::mutex lock_;
};
