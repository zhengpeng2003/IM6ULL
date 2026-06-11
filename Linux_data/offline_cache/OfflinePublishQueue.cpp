#include "OfflinePublishQueue.hpp"

#include <stdio.h>
#include <time.h>

namespace {

const int kFlushBatchSize = 50;
const int kSendFailedNotReady = -5;

int64_t currentTimeMs()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000;

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

} // namespace

bool OfflinePublishQueue::init(const std::string &dbPath)
{
    std::lock_guard<std::mutex> guard(lock_);

    if (initialized_)
        return true;

    if (!db_.open(dbPath)) {
        printf("[OfflineCache] disabled because database open failed\n");
        return false;
    }

    if (!db_.initSchema()) {
        printf("[OfflineCache] disabled because schema init failed\n");
        return false;
    }

    initialized_ = true;
    printf("[OfflineCache] initialized\n");
    return true;
}

void OfflinePublishQueue::setSender(OfflineMqttSender sender)
{
    std::lock_guard<std::mutex> guard(lock_);
    sender_ = sender;
}

int OfflinePublishQueue::publishOrCache(const std::string &topic,
                                        const std::string &payload,
                                        const PublishMeta &meta)
{
    OfflineMqttSender sender = nullptr;
    int send_ret = kSendFailedNotReady;
    {
        std::lock_guard<std::mutex> guard(lock_);
        sender = sender_;
    }

    if (sender) {
        send_ret = sender(topic.c_str(), payload.c_str());
        if (send_ret == 0)
            return 0;

        printf("[OfflineCache] MQTT offline or direct send failed ret=%d type=%s\n",
               send_ret, meta.messageType.c_str());
    }

    if (cacheMessage(topic, payload, meta))
        return 0;

    return send_ret;
}

bool OfflinePublishQueue::cacheMessage(const std::string &topic,
                                       const std::string &payload,
                                       const PublishMeta &meta)
{
    std::lock_guard<std::mutex> guard(lock_);

    if (!initialized_) {
        printf("[OfflineCache] skip cache because queue is not initialized type=%s\n",
               meta.messageType.c_str());
        return false;
    }

    if (!policy_.shouldCache(meta))
        return false;

    if (!db_.insert(topic, payload, meta))
        return false;

    db_.enforceMaxRecords();
    return true;
}

void OfflinePublishQueue::flushToMqttOnce()
{
    OfflineMqttSender sender = nullptr;
    {
        std::lock_guard<std::mutex> guard(lock_);
        if (!initialized_ || !sender_)
            return;
        sender = sender_;
    }

    std::vector<OfflinePendingMessage> rows;
    {
        std::lock_guard<std::mutex> guard(lock_);
        rows = db_.loadPending(kFlushBatchSize);
    }

    if (rows.empty())
        return;

    printf("[OfflineCache] flushing offline messages batch=%zu\n", rows.size());

    for (const OfflinePendingMessage &row : rows) {
        int ret = sender(row.topic.c_str(), row.payload.c_str());
        if (ret == 0) {
            std::lock_guard<std::mutex> guard(lock_);
            db_.remove(row.id);
        } else {
            std::lock_guard<std::mutex> guard(lock_);
            db_.markSendFailed(row.id, currentTimeMs());
            printf("[OfflineCache] stop current flush because send failed id=%lld ret=%d\n",
                   (long long)row.id, ret);
            break;
        }
    }
}

int OfflinePublishQueue::countPending()
{
    std::lock_guard<std::mutex> guard(lock_);
    if (!initialized_)
        return 0;

    return db_.countPending();
}
