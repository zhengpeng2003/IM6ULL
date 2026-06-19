#include "PcUiPublisher.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "ipc/IpcServer.hpp"
#include "model/DeviceRecord.hpp"
#include "model/TelemetryPoint.hpp"
#include "protocol/PcDataMessages.hpp"
#include "service/PcDataService.hpp"

void sendLatestPoints(IpcServer& ipc, PcDataService& dataService)
{
    std::vector<TelemetryPoint> points = dataService.getLatestPointsSnapshot();

    std::cout << "latest point count: " << points.size() << std::endl;

    std::string json = buildLatestPointsJson(points);

    std::cout << "json build ok, size: " << json.size() << std::endl;
    std::cout << "[DBG_TELEMETRY] latest_points pointCount=" << points.size()
              << " jsonBytes=" << json.size() << std::endl;

    const bool ok = ipc.sendMessage(json);
    std::cout << "[DBG_IPC_SEND] latest_points send ok="
              << (ok ? "true" : "false")
              << " pointCount=" << points.size()
              << " jsonBytes=" << json.size() << std::endl;

    std::cout << "send latest_points done" << std::endl;
}

void sendLatestPoints(IpcServer& ipc, PcDataService& dataService, PcDatabase& database)
{
    (void)database;
    sendLatestPoints(ipc, dataService);
}

void sendDevicesSnapshot(IpcServer& ipc, PcDataService& dataService)
{
    std::vector<DeviceRecord> devices = dataService.getDeviceRegistrySnapshot();

    const std::string json = buildDevicesSnapshotJson(devices);
    const bool ok = ipc.sendMessage(json);
    std::cout << "send devices_snapshot done count=" << devices.size()
              << " ok=" << (ok ? "true" : "false")
              << " jsonBytes=" << json.size() << std::endl;
}

void sendDevicesSnapshot(IpcServer& ipc, PcDatabase& database)
{
    (void)database;
    std::cout << "send devices_snapshot skipped: legacy database-backed path is disabled" << std::endl;
}

void sendGatewayStatusSnapshot(IpcServer& ipc, PcDataService& dataService)
{
    std::vector<GatewayStatus> gateways = dataService.getGatewayStatusSnapshot();

    const std::string json = buildGatewayStatusSnapshotJson(gateways);
    const bool ok = ipc.sendMessage(json);
    std::cout << "send gateway_status_snapshot done count=" << gateways.size()
              << " ok=" << (ok ? "true" : "false")
              << " jsonBytes=" << json.size() << std::endl;
}

void sendGatewayStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    (void)database;
    std::cout << "send gateway_status_snapshot skipped: legacy database-backed path is disabled" << std::endl;
}

void sendPortStatusSnapshot(IpcServer& ipc, PcDataService& dataService)
{
    std::vector<GatewayPort> ports = dataService.getGatewayPortSnapshot();

    const std::string json = buildPortStatusSnapshotJson(ports);
    const bool ok = ipc.sendMessage(json);
    std::cout << "send port_status_snapshot done count=" << ports.size()
              << " ok=" << (ok ? "true" : "false")
              << " jsonBytes=" << json.size() << std::endl;
}

void sendPortStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    (void)database;
    std::cout << "send port_status_snapshot skipped: legacy database-backed path is disabled" << std::endl;
}
