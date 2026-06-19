#ifndef PC_UI_PUBLISHER_HPP
#define PC_UI_PUBLISHER_HPP

class IpcServer;
class PcDataService;
class PcDatabase;

void sendLatestPoints(IpcServer& ipc, PcDataService& dataService);
void sendLatestPoints(IpcServer& ipc, PcDataService& dataService, PcDatabase& database);
void sendDevicesSnapshot(IpcServer& ipc, PcDataService& dataService);
void sendDevicesSnapshot(IpcServer& ipc, PcDatabase& database);
void sendGatewayStatusSnapshot(IpcServer& ipc, PcDataService& dataService);
void sendGatewayStatusSnapshot(IpcServer& ipc, PcDatabase& database);
void sendPortStatusSnapshot(IpcServer& ipc, PcDataService& dataService);
void sendPortStatusSnapshot(IpcServer& ipc, PcDatabase& database);

#endif // PC_UI_PUBLISHER_HPP
