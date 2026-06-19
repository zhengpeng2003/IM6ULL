#pragma once
#include <QObject>
#include <QHash>
#include "model/DeviceModel.h"

class DeviceManager : public QObject
{
    Q_OBJECT
public:
    explicit DeviceManager(QObject *parent = nullptr);

    void setDevices(const QList<DeviceNode> &devices);
    void setGateways(const QList<GatewayNode> &gateways);
    void setPorts(const QList<PortNode> &ports);
    void clearAll();
    void upsertDevice(const DeviceNode &node);
    void removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId);
    void removeMasterData(const QString &gatewayId, const QString &portId);
    QList<DeviceNode> allDevices() const;
    QList<GatewayNode> allGateways() const;
    QList<PortNode> allPorts() const;
    DeviceNode device(const QString &key) const;
    void updateDeviceOnline(const QString &key, bool online);
    int onlineGatewayCount() const;
    int onlineDeviceCount() const;

signals:
    void deviceConfigChanged();
    void gatewayStatusChanged();
    void portStatusChanged();
    void deviceOnlineStateChanged(const QString &deviceKey, bool online);
    void onlineGatewayCountChanged(int count);
    void onlineDeviceCountChanged(int count);

private:
    void emitOnlineCountsIfChanged();

    QHash<QString, DeviceNode> m_devices;
    QHash<QString, GatewayNode> m_gateways;
    QHash<QString, PortNode> m_ports;
    int m_lastOnlineGatewayCount = -1;
    int m_lastOnlineDeviceCount = -1;
};
