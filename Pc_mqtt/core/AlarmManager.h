#pragma once
#include <QObject>
#include <QHash>
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
    void acknowledgeAlarm(const QString &alarmId);
    void recoverAlarm(const QString &alarmId);

signals:
    void alarmAdded(const AlarmRecord &alarm);
    void alarmUpdated(const AlarmRecord &alarm);
    void activeAlarmCountChanged(int count);
    void alarmForDb(const AlarmRecord &alarm);

private:
    QHash<QString, AlarmRecord> m_alarms;
};
