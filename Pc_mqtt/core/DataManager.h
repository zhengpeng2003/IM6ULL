#pragma once
#include <QObject>
#include <QHash>
#include <QMutex>
#include <QJsonObject>
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

public slots:
    void onMqttMessageArrived(const QString &topic, const QByteArray &payload);

signals:
    void realtimeDataUpdated();
    void deviceTreeChanged();
    void telemetryForDb(const TelemetryRecord &record);

private:
    void handleTelemetry(const QJsonObject &obj);
    void handleStatus(const QJsonObject &obj);
    void handleHeartbeat(const QJsonObject &obj);

private:
    DeviceManager *m_deviceManager = nullptr;
    AlarmManager *m_alarmManager = nullptr;
    mutable QMutex m_mutex;
    QHash<QString, RealtimeDeviceData> m_realtimeMap;
};
