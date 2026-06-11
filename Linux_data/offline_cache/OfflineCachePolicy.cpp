#include "OfflineCachePolicy.hpp"

#include <stdio.h>

namespace {

const int64_t kTelemetrySampleIntervalMs = 60000;

std::string sampleKey(const PublishMeta &meta)
{
    return meta.gatewayId + "." +
           meta.portId + "." +
           std::to_string(meta.deviceId) + "." +
           meta.pointKey;
}

} // namespace

bool OfflineCachePolicy::shouldCache(const PublishMeta &meta)
{
    if (meta.messageType == "alarm_event")
        return true;

    if (meta.messageType == "ack")
        return true;

    if (meta.messageType == "device_status")
        return meta.statusChanged;

    if (meta.messageType == "telemetry_pack") {
        if (meta.hasAlarm || meta.hasInvalidData || meta.statusChanged)
            return true;

        return allowSample(meta);
    }

    return false;
}

bool OfflineCachePolicy::allowSample(const PublishMeta &meta)
{
    const std::string key = sampleKey(meta);
    const int64_t now_ms = meta.timestampMs;
    auto it = last_sample_time_.find(key);
    if (it == last_sample_time_.end() ||
        now_ms <= 0 ||
        now_ms - it->second >= kTelemetrySampleIntervalMs) {
        last_sample_time_[key] = now_ms;
        return true;
    }

    printf("[OfflineCache] drop normal telemetry by sample interval key=%s delta_ms=%lld\n",
           key.c_str(),
           (long long)(now_ms - it->second));
    return false;
}
