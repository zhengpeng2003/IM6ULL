#include "PcDataService.hpp"

#include <chrono>
#include <iostream>

#include "model/ModelConverter.hpp"

PcDataService::PcDataService()
{
}

void PcDataService::handleTelemetryPack(const TelemetryPack& pack)
{
    std::vector<TelemetryPoint> points = ModelConverter::toTelemetryPoints(pack);

    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& point : points) {
        if (point.pointId.empty()) {
            continue;
        }

        /*
         * Same pointId means the same telemetry point.
         * When new data arrives, overwrite the old value.
         * This is the latest snapshot.
         */
        m_snapshot[point.pointId] = point;
    }
}

std::vector<TelemetryPoint> PcDataService::getLatestPoints() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<TelemetryPoint> result;
    result.reserve(m_snapshot.size());

    for (const auto& item : m_snapshot) {
        result.push_back(item.second);
    }

    return result;
}

bool PcDataService::getPointById(const std::string& pointId, TelemetryPoint& outPoint) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_snapshot.find(pointId);
    if (it == m_snapshot.end()) {
        return false;
    }

    outPoint = it->second;
    return true;
}

void PcDataService::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snapshot.clear();
}

static std::int64_t currentTimeMs()
{
    using namespace std::chrono;

    return duration_cast<milliseconds>(
               system_clock::now().time_since_epoch()
               ).count();
}
