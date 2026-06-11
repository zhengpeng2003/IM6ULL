#include "PcUiPublisher.hpp"

#include <iostream>
#include <string>
#include <vector>

#include "ipc/IpcServer.hpp"
#include "model/DeviceRecord.hpp"
#include "model/TelemetryPoint.hpp"
#include "protocol/PcDataMessages.hpp"
#include "service/PcDataService.hpp"
#include "storage/PcDatabase.hpp"

void sendLatestPoints(IpcServer& ipc, PcDataService& dataService, PcDatabase& database)
{
    std::vector<TelemetryPoint> points = dataService.getLatestPoints();
    if (points.empty() && database.isOpen()) {
        points = database.queryLatestPoints();
    }

    std::cout << "latest point count: " << points.size() << std::endl;

    std::string json = buildLatestPointsJson(points);

    std::cout << "json build ok, size: " << json.size() << std::endl;

    ipc.sendMessage(json);

    std::cout << "send latest_points done" << std::endl;
}

void sendDevicesSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<DeviceRecord> devices;
    if (database.isOpen()) {
        devices = database.queryDevices();
    }

    ipc.sendMessage(buildDevicesSnapshotJson(devices));
    std::cout << "send devices_snapshot done, count: " << devices.size() << std::endl;
}

void sendGatewayStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<GatewayStatus> gateways;
    if (database.isOpen()) {
        gateways = database.queryGatewayStatuses();
    }

    ipc.sendMessage(buildGatewayStatusSnapshotJson(gateways));
    std::cout << "send gateway_status_snapshot done, count: " << gateways.size() << std::endl;
}

void sendPortStatusSnapshot(IpcServer& ipc, PcDatabase& database)
{
    std::vector<GatewayPort> ports;
    if (database.isOpen()) {
        ports = database.queryGatewayPorts();
    }

    ipc.sendMessage(buildPortStatusSnapshotJson(ports));
    std::cout << "send port_status_snapshot done, count: " << ports.size() << std::endl;
}
