#pragma once
#include <QWidget>
class AlarmManager;
class DatabaseManager;
class AlarmTableWidget;

class AlarmLogPage : public QWidget
{
    Q_OBJECT
public:
    explicit AlarmLogPage(AlarmManager *alarm, DatabaseManager *database, QWidget *parent = nullptr);

private slots:
    void refreshTable();

private:
    AlarmManager *m_alarm = nullptr;
    DatabaseManager *m_database = nullptr;
    AlarmTableWidget *m_table = nullptr;
};
