#pragma once
#include <QWidget>
class AlarmManager;
class AlarmTableWidget;

class AlarmLogPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmLogPage(AlarmManager *alarm, QWidget *parent = nullptr);

private slots:
    void refreshTable();

private:
    AlarmManager *m_alarm = nullptr;
    AlarmTableWidget *m_table = nullptr;
};
