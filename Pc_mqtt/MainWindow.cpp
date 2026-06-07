#include "MainWindow.h"

#include "ui/TopBar.h"
#include "ui/SideBar.h"
#include "pages/DashboardPage.h"
#include "pages/MonitorPage.h"
#include "pages/TrendPage.h"
#include "pages/DeviceConfigPage.h"
#include "pages/AlarmLogPage.h"
#include "pages/SystemSettingPage.h"
#include "core/MqttClientManager.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/AlarmManager.h"
#include "core/CommandManager.h"
#include "core/DatabaseManager.h"
#include "core/ConfigManager.h"
#include "model/ConfigModel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    initManagers();
    initUi();
    initConnections();
    loadInitialConfig();
}

MainWindow::~MainWindow() = default;

void MainWindow::initManagers()
{
    m_config = new ConfigManager(this);
    m_mqtt = new MqttClientManager(this);
    m_device = new DeviceManager(this);
    m_alarm = new AlarmManager(this);
    m_command = new CommandManager(this);
    m_database = new DatabaseManager(this);
    m_data = new DataManager(m_device, m_alarm, this);
}

void MainWindow::initUi()
{
    setWindowTitle(QStringLiteral("Pc_mqtt 工业物联网监控平台"));
    statusBar()->hide();

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_topBar = new TopBar(this);
    rootLayout->addWidget(m_topBar);

    auto *body = new QWidget(this);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_sideBar = new SideBar(this);
    m_stack = new QStackedWidget(this);

    m_dashboardPage = new DashboardPage(m_data, m_device, m_alarm, this);
    m_monitorPage = new MonitorPage(m_data, m_command, this);
    m_trendPage = new TrendPage(m_database, this);
    m_deviceConfigPage = new DeviceConfigPage(m_device, m_config, this);
    m_alarmLogPage = new AlarmLogPage(m_alarm, m_database, this);
    m_systemSettingPage = new SystemSettingPage(m_config, m_mqtt, this);

    m_stack->addWidget(m_dashboardPage);
    m_stack->addWidget(m_monitorPage);
    m_stack->addWidget(m_trendPage);
    m_stack->addWidget(m_deviceConfigPage);
    m_stack->addWidget(m_alarmLogPage);
    m_stack->addWidget(m_systemSettingPage);

    bodyLayout->addWidget(m_sideBar);
    bodyLayout->addWidget(m_stack, 1);
    rootLayout->addWidget(body, 1);

    setCentralWidget(central);
}

void MainWindow::initConnections()
{
    connect(m_sideBar, &SideBar::pageChanged, m_stack, &QStackedWidget::setCurrentIndex);

    connect(m_mqtt, &MqttClientManager::connected, m_topBar, [this](){
        m_topBar->setMqttState(true);
    });
    connect(m_mqtt, &MqttClientManager::disconnected, m_topBar, [this](){
        m_topBar->setMqttState(false);
    });

    connect(m_mqtt, &MqttClientManager::messageArrived,
            m_data, &DataManager::onMqttMessageArrived);

    connect(m_data, &DataManager::telemetryForDb,
            m_database, &DatabaseManager::enqueueTelemetry);

    connect(m_alarm, &AlarmManager::alarmForDb,
            m_database, &DatabaseManager::enqueueAlarm);

    connect(m_alarm, &AlarmManager::activeAlarmCountChanged,
            m_topBar, &TopBar::setAlarmCount);

    connect(m_device, &DeviceManager::onlineGatewayCountChanged,
            m_topBar, &TopBar::setOnlineGatewayCount);

    connect(m_device, &DeviceManager::onlineDeviceCountChanged,
            m_topBar, &TopBar::setOnlineDeviceCount);

    connect(m_command, &CommandManager::commandReadyToPublish,
            m_mqtt, &MqttClientManager::publishMessage);

    connect(m_command, &CommandManager::commandForDb,
            m_database, &DatabaseManager::enqueueCommand);
}

void MainWindow::loadInitialConfig()
{
    m_database->openDatabase(m_config->loadDatabasePath());
    m_database->initTables();

    m_data->loadDemoData();

    const MqttConfig cfg = m_config->loadMqttConfig();
    if (cfg.autoConnect) {
        m_mqtt->connectToBroker(cfg);
    }
}
