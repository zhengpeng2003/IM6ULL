#pragma once
#include <QWidget>
class DataManager;
class DeviceManager;
class AlarmManager;
class StatusCard;
class QTimer;
class QTableWidget;

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(DataManager *data, DeviceManager *device, AlarmManager *alarm, QWidget *parent = nullptr);

private slots:
    void refreshView();

private:
    DataManager *m_data = nullptr;
    DeviceManager *m_device = nullptr;
    AlarmManager *m_alarm = nullptr;
    StatusCard *m_gatewayCard = nullptr;
    StatusCard *m_masterCard = nullptr;
    StatusCard *m_slaveCard = nullptr;
    StatusCard *m_alarmCard = nullptr;
    QTableWidget *m_masterTable = nullptr;
    QTableWidget *m_alarmTable = nullptr;
    QTimer *m_timer = nullptr;
};
