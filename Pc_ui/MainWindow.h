#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QJsonObject>
#include <QString>
#include <QStackedWidget>

class TopBar;
class SideBar;
class DashboardPage;
class MonitorPage;
class TrendPage;
class DeviceConfigPage;
class AlarmLogPage;
class SystemSettingPage;

class DataManager;
class DeviceManager;
class AlarmManager;
class CommandManager;
class ConfigManager;
class IpcClient;
class QTimer;

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
    void initIpc();
    void handleIpcMessage(const QByteArray &frame);
    void requestLatestPoints();
    void requestDevices();
    void requestGatewayStatus();
    void requestPortStatus();
    void sendHistoryQuery(const QString &pointId, qint64 startMs, qint64 endMs, int limit);
    void sendDeleteMasterData(const QString &gatewayId, const QString &portId);
    void sendDeleteDeviceData(const QString &gatewayId, const QString &portId, int deviceId);
    void sendClearRecoveredAlarms();
    void requestMqttConfig();
    void saveMqttConfig(const QString &host, int port);
    void markIpcDataOffline();

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

    DataManager *m_data = nullptr;
    DeviceManager *m_device = nullptr;
    AlarmManager *m_alarm = nullptr;
    CommandManager *m_command = nullptr;
    ConfigManager *m_config = nullptr;


private:
    IpcClient *m_ipcClient = nullptr;
    QTimer *m_ipcTimer = nullptr;
    QTimer *m_ipcWatchdogTimer = nullptr;
    qint64 m_lastLatestPointsMs = 0;
    QString m_pendingDeleteAction;
    QString m_pendingDeleteGatewayId;
    QString m_pendingDeletePortId;
    int m_pendingDeleteDeviceId = 0;
};
