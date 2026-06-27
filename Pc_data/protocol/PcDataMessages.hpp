//构建返回的json文件
#ifndef PC_DATA_MESSAGES_HPP
#define PC_DATA_MESSAGES_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "model/DeviceRecord.hpp"
#include "model/TelemetryPoint.hpp"
#include "mqtt/MqttConfig.hpp"
#include "storage/PcDatabase.hpp"

std::string buildCommandAckJson(const std::string& cmdId,
                                bool ok,
                                const std::string& reason,
                                std::int64_t seq = 0,
                                const std::string& commandType = std::string(),
                                const std::string& stage = std::string(),
                                const std::string& message = std::string(),
                                std::int64_t boardSeq = 0,
                                const std::string& gatewayId = std::string(),
                                const std::string& portId = std::string(),
                                int deviceId = 0);
std::string buildLatestPointsJson(const std::vector<TelemetryPoint>& points);
std::string buildCommandLogUpdateJson(std::int64_t seq,
                                      const std::string& commandType,
                                      const std::string& status,
                                      const std::string& reason,
                                      const std::string& message,
                                      const CommandLogTarget* target = nullptr,
                                      std::int64_t boardSeq = 0);
std::string buildDevicesSnapshotJson(const std::vector<DeviceRecord>& devices);
std::string buildGatewayStatusSnapshotJson(const std::vector<GatewayStatus>& gateways);
std::string buildPortStatusSnapshotJson(const std::vector<GatewayPort>& ports);
std::string buildDeviceRegisterAckJson(std::uint32_t sequence,
                                       const DeviceRecord& device,
                                       bool ok,
                                       const std::string& reason);
std::string buildHistoryPointsJson(const std::string& pointId,
                                   const std::vector<TelemetryPoint>& points,
                                   bool ok = true,
                                   const std::string& reason = std::string(),
                                   const std::string& message = std::string());
std::string buildDeleteDataAckJson(const std::string& action,
                                   bool ok,
                                   const std::string& reason);
std::string buildAlarmActionAckJson(const std::string& action,
                                    bool ok,
                                    const std::string& reason,
                                    const std::string& alarmId = std::string());
std::string buildAlarmEventsSnapshotJson(const std::vector<AlarmEvent>& events);
std::string buildMqttConfigJson(const MqttConfig& config,
                                const std::string& status);
std::string buildMqttConfigAckJson(bool ok,
                                   const std::string& reason,
                                   const MqttConfig& config,
                                   const std::string& status);
std::string buildSyncConfigResultJson(bool success,
                                      const std::string& message,
                                      int portCount,
                                      int deviceCount);

bool parseGatewayRegister(const std::string& payload, GatewayStatus& gateway);
bool parseGatewayHeartbeat(const std::string& payload,
                           std::string& gatewayId,
                           std::int64_t& timestampMs,
                           std::string& status);
bool parsePortRegister(const std::string& payload, GatewayPort& port);
bool parseDeviceRegister(const std::string& payload,
                         DeviceRecord& device,
                         std::uint32_t& sequence,
                         std::string& reason);
bool parseSaveMqttConfigRequest(const std::string& msg,
                                MqttConfig& config,
                                std::string& reason);
bool parseGetMqttConfigRequest(const std::string& msg);
bool parseHistoryQuery(const std::string& msg,
                       std::string& pointId,
                       std::int64_t& startMs,
                       std::int64_t& endMs,
                       int& limit);
bool parseDeleteDeviceRequest(const std::string& msg,
                              std::string& gatewayId,
                              std::string& portId,
                              int& deviceId);
bool parseDeleteMasterRequest(const std::string& msg,
                              std::string& gatewayId,
                              std::string& portId);
bool parseClearRecoveredAlarmsRequest(const std::string& msg);
bool parseClearAcknowledgedAlarmsRequest(const std::string& msg);
bool parseAckAlarmRequest(const std::string& msg, std::string& alarmId);
bool parseGetAlarmEventsRequest(const std::string& msg);
bool parseClearAllDataRequest(const std::string& msg);

#endif // PC_DATA_MESSAGES_HPP
