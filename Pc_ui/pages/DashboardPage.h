#pragma once
#include <QWidget>
class QShowEvent;
class DataManager;
class DeviceManager;
class AlarmManager;
class UiStateStore;
class StatusCard;
class QTabWidget;
class QTableWidget;
class QTimer;

class DashboardPage : public QWidget
{
    Q_OBJECT
public:
    explicit DashboardPage(DataManager *data, DeviceManager *device, AlarmManager *alarm,
                           UiStateStore *stateStore, QWidget *parent = nullptr);

private slots:
    void scheduleRefreshView();
    void refreshView();

protected:
    void showEvent(QShowEvent *event) override;

private:
    DataManager *m_data = nullptr;
    DeviceManager *m_device = nullptr;
    AlarmManager *m_alarm = nullptr;
    UiStateStore *m_stateStore = nullptr;
    StatusCard *m_gatewayCard = nullptr;
    StatusCard *m_masterCard = nullptr;
    StatusCard *m_slaveCard = nullptr;
    StatusCard *m_alarmCard = nullptr;
    QTableWidget *m_masterTable = nullptr;
    QTabWidget *m_infoTabWidget = nullptr;
    QTableWidget *m_alarmTable = nullptr;
    QTableWidget *m_errorTable = nullptr;
    QTimer *m_refreshTimer = nullptr;
    bool m_refreshDirty = false;
};
