#pragma once
#include <QObject>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include "model/AlarmModel.h"

class AlarmManager : public QObject
{
    Q_OBJECT
public:
    explicit AlarmManager(QObject *parent = nullptr);
    QList<AlarmRecord> alarms() const;
    QList<AlarmRecord> latestAlarms(int limit = 10) const;
    int activeAlarmCount() const;

public slots:
    void onAlarmMessage(const QJsonObject &obj);
    void onAlarmSnapshot(const QJsonArray &alarms);
    void acknowledgeAlarm(const QString &alarmId);
    void recoverAlarm(const QString &alarmId);
    void removeDeviceAlarms(const QString &gatewayId, const QString &portId, int deviceId);
    void clearRecoveredAlarms();
    void clearAllAlarms();

signals:
    void alarmAdded(const AlarmRecord &alarm);
    void alarmUpdated(const AlarmRecord &alarm);
    void alarmsChanged();
    void activeAlarmCountChanged(int count);

private:
    QHash<QString, AlarmRecord> m_alarms;
};
