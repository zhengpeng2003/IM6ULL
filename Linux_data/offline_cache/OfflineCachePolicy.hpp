#pragma once

#include <stdint.h>
#include <string>
#include <unordered_map>

struct PublishMeta {
    std::string messageType;
    std::string gatewayId;
    std::string portId;
    int deviceId = -1;
    std::string pointKey;

    int priority = 0;

    bool hasAlarm = false;
    bool hasInvalidData = false;
    bool statusChanged = false;

    int64_t timestampMs = 0;
};

class OfflineCachePolicy {
public:
    bool shouldCache(const PublishMeta &meta);

private:
    bool allowSample(const PublishMeta &meta);

    std::unordered_map<std::string, int64_t> last_sample_time_;
};
