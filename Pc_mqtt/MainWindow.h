#pragma once

#include <QMainWindow>
#include <QStackedWidget>

class TopBar;
class SideBar;
class DashboardPage;
class MonitorPage;
class TrendPage;
class DeviceConfigPage;
class AlarmLogPage;
class SystemSettingPage;

class MqttClientManager;
class DataManager;
class DeviceManager;
class AlarmManager;
class CommandManager;
class DatabaseManager;
class ConfigManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void initManagers();
    void initUi();
    void initConnections();
    void loadInitialConfig();

private:
    TopBar *m_topBar = nullptr;
    SideBar *m_sideBar = nullptr;
    QStackedWidget *m_stack = nullptr;

    DashboardPage *m_dashboardPage = nullptr;
    MonitorPage *m_monitorPage = nullptr;
    TrendPage *m_trendPage = nullptr;
    DeviceConfigPage *m_deviceConfigPage = nullptr;
    AlarmLogPage *m_alarmLogPage = nullptr;
    SystemSettingPage *m_systemSettingPage = nullptr;

    MqttClientManager *m_mqtt = nullptr;
    DataManager *m_data = nullptr;
    DeviceManager *m_device = nullptr;
    AlarmManager *m_alarm = nullptr;
    CommandManager *m_command = nullptr;
    DatabaseManager *m_database = nullptr;
    ConfigManager *m_config = nullptr;
};
