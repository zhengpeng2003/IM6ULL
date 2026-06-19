#ifndef MQTT_MESSAGE_HANDLER_HPP
#define MQTT_MESSAGE_HANDLER_HPP

#include <cstdint>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "model/TelemetryPoint.hpp"

class IpcServer;
class MqttClient;
class PcDataService;
class PcDatabase;

class MqttMessageHandler
{
public:
    MqttMessageHandler(PcDatabase& database,
                       PcDataService& dataService,
                       IpcServer& ipc,
                       MqttClient& mqtt);
    ~MqttMessageHandler();

    void handle(const std::string& topic, const std::string& payload);

private:
    struct TelemetryMessage
    {
        std::string topic;
        std::string payload;
    };

    void enqueueTelemetry(const std::string& topic, const std::string& payload);
    void telemetryWorkerLoop();
    void dbWriterLoop();
    void latestPublisherLoop();
    void handleTelemetryMessage(const std::string& topic, const std::string& payload);
    void enqueueTelemetryStorage(const std::vector<TelemetryPoint>& points);
    void markLatestPointsDirty();

    PcDatabase& m_database;
    PcDataService& m_dataService;
    IpcServer& m_ipc;
    MqttClient& m_mqtt;
    std::unordered_map<std::string, std::int64_t> m_lastConfigRequestMs;

    std::mutex m_telemetryMutex;
    std::condition_variable m_telemetryCv;
    std::deque<TelemetryMessage> m_telemetryQueue;
    bool m_stopTelemetryWorker = false;
    std::thread m_telemetryWorker;

    std::mutex m_dbWriterMutex;
    std::condition_variable m_dbWriterCv;
    std::unordered_map<std::string, TelemetryPoint> m_pendingDbPoints;
    bool m_stopDbWriter = false;
    std::thread m_dbWriter;

    std::mutex m_latestPublisherMutex;
    std::condition_variable m_latestPublisherCv;
    bool m_latestPointsDirty = false;
    bool m_stopLatestPublisher = false;
    std::thread m_latestPublisher;
};

#endif // MQTT_MESSAGE_HANDLER_HPP
