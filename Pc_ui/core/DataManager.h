#pragma once
#include <QObject>
#include <QHash>
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
    void markAllDevicesOffline();

public slots:
    void onMqttMessageArrived(const QString &topic, const QByteArray &payload);
    void onLatestPointsMessage(const QJsonObject &obj);

signals:
    void realtimeDataUpdated();
    void deviceTreeChanged();
    void telemetryForDb(const TelemetryRecord &record);

private:
    QList<RealtimeDeviceData> parseLatestPoints(const QJsonObject &obj) const;
    TelemetryPointData parseTelemetryPoint(const QJsonObject &obj) const;
    QHash<QString, QList<TelemetryPointData>> groupPointsByDevice(const QJsonObject &obj) const;
    RealtimeDeviceData buildRealtimeDeviceData(const QList<TelemetryPointData> &points) const;
    void evaluateDeviceStatus(RealtimeDeviceData &data) const;
    void applyPointToTypedFields(RealtimeDeviceData &data, const TelemetryPointData &point) const;
    void handleTelemetry(const QJsonObject &obj);
    void handleStatus(const QJsonObject &obj);
    void handleHeartbeat(const QJsonObject &obj);
    void upsertRealtimeData(const RealtimeDeviceData &data);

private:
    DeviceManager *m_deviceManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    mutable QMutex m_mutex;
    QHash<QString, RealtimeDeviceData> m_realtimeMap;
};
