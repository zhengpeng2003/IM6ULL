#pragma once
#include <QObject>
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QJsonObject>
#include <QList>
#include "model/TelemetryModel.h"

class DeviceManager;
class AlarmManager;

class DataManager : public QObject
{
    Q_OBJECT
public:
    explicit DataManager(DeviceManager *deviceManager, AlarmManager *alarmManager, QObject *parent = nullptr);

    void loadDemoData();
    QList<DeviceNode> deviceTreeSnapshot() const;
    RealtimeDeviceData deviceData(const QString &deviceKey) const;
    QList<RealtimeDeviceData> allRealtimeData() const;
    void refreshOfflineStates(qint64 timeoutMs = 30000);
    void removeDeviceData(const QString &gatewayId, const QString &portId, int deviceId);
    void removeMasterData(const QString &gatewayId, const QString &portId);
    void forgetRemovedDevice(const QString &gatewayId, const QString &portId, int deviceId);
    void clearRuntimeData();
    void clearAllData();
    void markAllDevicesOffline();

public slots:
    void onLatestPointsMessage(const QJsonObject &obj);
    void onDevicesSnapshotMessage(const QJsonObject &obj);

signals:
    void realtimeDataUpdated();
    void deviceTreeChanged();

private:
    QList<RealtimeDeviceData> parseLatestPoints(const QJsonObject &obj) const;
    QList<DeviceNode> parseDevicesSnapshot(const QJsonObject &obj) const;
    TelemetryPointData parseTelemetryPoint(const QJsonObject &obj) const;
    QHash<QString, QList<TelemetryPointData>> groupPointsByDevice(const QJsonObject &obj) const;
    RealtimeDeviceData buildRealtimeDeviceData(const QList<TelemetryPointData> &points) const;
    void evaluateDeviceStatus(RealtimeDeviceData &data) const;
    void applyPointToTypedFields(RealtimeDeviceData &data, const TelemetryPointData &point) const;
    void upsertRealtimeData(const RealtimeDeviceData &data);
    QString deletedDeviceKey(const QString &gatewayId, const QString &portId, int deviceId) const;
    bool isDeletedDevice(const QString &gatewayId, const QString &portId, int deviceId) const;
    void pruneExpiredDeletedDevicesLocked(qint64 nowMs);
    void rememberDeletedDeviceLocked(const QString &gatewayId, const QString &portId, int deviceId, qint64 nowMs);
    void releaseDeletedDeviceOnNewDataLocked(const QString &gatewayId,
                                             const QString &portId,
                                             int deviceId,
                                             qint64 dataTimeMs);

private:
    DeviceManager *m_deviceManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    mutable QMutex m_mutex;
    QHash<QString, RealtimeDeviceData> m_realtimeMap;
    QHash<QString, qint64> m_deletedDevices;
};
