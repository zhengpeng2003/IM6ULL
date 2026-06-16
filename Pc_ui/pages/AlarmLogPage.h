#pragma once
#include <QList>
#include <QWidget>
#include "model/AlarmModel.h"

class AlarmManager;
class AlarmTableWidget;
class QComboBox;

class AlarmLogPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmLogPage(AlarmManager *alarm, QWidget *parent = nullptr);

public slots:
    void refreshTable();
    void acknowledgeVisibleActiveAlarms();
    void clearRecoveredAlarms();
signals:
    void clearRecoveredAlarmsRequested();

private:
    QList<AlarmRecord> filteredAlarms() const;
    bool matchesLevel(const AlarmRecord &alarm) const;
    bool matchesState(const AlarmRecord &alarm) const;
    bool matchesTimeRange(const AlarmRecord &alarm) const;

    AlarmManager *m_alarm = nullptr;
    AlarmTableWidget *m_table = nullptr;
    QComboBox *m_levelCombo = nullptr;
    QComboBox *m_stateCombo = nullptr;
    QComboBox *m_rangeCombo = nullptr;
};
