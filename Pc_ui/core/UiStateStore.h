#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include "model/DeviceModel.h"
#include "model/TelemetryModel.h"

class DataManager;
class DeviceManager;
class AlarmManager;

struct DeviceState
{
    DeviceNode node;
    RealtimeDeviceData realtime;
    QString lifecycleState;
    QString lastMutationSource;
    QString lastCommandStatus;
    qint64 lastStateUpdateMs = 0;

    QString key() const { return node.key(); }
};

class UiStateStore : public QObject
{
    Q_OBJECT
public:
    explicit UiStateStore(DataManager *dataManager,
                          DeviceManager *deviceManager,
                          AlarmManager *alarmManager,
                          QObject *parent = nullptr);

    bool applyIpcMessage(const QJsonObject &root);

    void refreshOfflineStates(qint64 timeoutMs = 30000);
    void markAllDevicesOffline(const QString &reason = QStringLiteral("service_offline"));
    void removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId);
    void removeMasterData(const QString &gatewayId, const QString &portId);
    void forgetRemovedDevice(const QString &gatewayId, const QString &portId, int deviceId);
    void clearRuntimeData();

    QList<DeviceState> deviceStates() const;
    DeviceState deviceState(const QString &deviceKey) const;
    int onlineGatewayCount() const;
    int onlineDeviceCount() const;
    int onlineMasterCount() const;
    int activeAlarmCount() const;
    bool isServiceOnline() const;

signals:
    void stateChanged();

private:
    QList<GatewayNode> parseGatewayStatusSnapshot(const QJsonObject &root) const;
    QList<PortNode> parsePortStatusSnapshot(const QJsonObject &root) const;
    void rebuildStateMirror(const QString &source);
    QString lifecycleFor(const DeviceState &state) const;

private:
    DataManager *m_dataManager = nullptr;
    DeviceManager *m_deviceManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    QHash<QString, DeviceState> m_devices;
};
