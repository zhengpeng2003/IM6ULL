#include "MainWindow.h"

#include "ui/TopBar.h"
#include "ui/SideBar.h"
#include "pages/DashboardPage.h"
#include "pages/MonitorPage.h"
#include "pages/TrendPage.h"
#include "pages/DeviceConfigPage.h"
#include "pages/AlarmLogPage.h"
#include "pages/SystemSettingPage.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/AlarmManager.h"
#include "core/CommandManager.h"
#include "core/ConfigManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "ipc/ipcclient.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    initManagers();
    initUi();
    initConnections();
    initIpc();
}
MainWindow::~MainWindow() = default;

void MainWindow::initIpc()
{
    m_ipcClient = new IpcClient(this);
    m_ipcTimer = new QTimer(this);
    m_ipcWatchdogTimer = new QTimer(this);

    connect(m_ipcClient, &IpcClient::connected, this, [this]() {
        qDebug() << "Pc_ui IPC connected";
        requestLatestPoints();
    });

    connect(m_ipcClient, &IpcClient::disconnected, this, [this]() {
        qDebug() << "Pc_ui IPC disconnected";
        markIpcDataOffline();
    });

    connect(m_ipcClient, &IpcClient::errorOccured, this, [this](const QString &err) {
        qDebug() << "IPC error:" << err;
        markIpcDataOffline();
    });

    connect(m_ipcClient, &IpcClient::messageReceived,
            this, &MainWindow::handleIpcMessage);

    connect(m_ipcTimer, &QTimer::timeout, this, [this]() {
        if (!m_ipcClient) {
            return;
        }

        if (!m_ipcClient->isConnected()) {
            m_ipcClient->connectToServer();
            return;
        }

        requestLatestPoints();

    });

    connect(m_ipcWatchdogTimer, &QTimer::timeout, this, [this]() {
        if (m_data) {
            m_data->refreshOfflineStates(30000);
        }

        if (m_lastLatestPointsMs <= 0) {
            markIpcDataOffline();
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastLatestPointsMs > 6000) {
            markIpcDataOffline();
        }
    });

    connect(m_command, &CommandManager::commandReadyForIpc, this, [this](const QByteArray &payload) {
        if (!m_ipcClient || !m_ipcClient->isConnected()) {
            qDebug() << "IPC command send skipped: client is not connected.";
            markIpcDataOffline();
            return;
        }

        m_ipcClient->sendMessage(payload);
    });

    m_ipcTimer->start(2000);
    m_ipcWatchdogTimer->start(1000);
    m_ipcClient->connectToServer();
}

void MainWindow::handleIpcMessage(const QByteArray &frame)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(frame, &error);

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "IPC JSON parse error:" << error.errorString()
                 << "frame:" << frame;
        return;
    }

    if (!doc.isObject()) {
        qDebug() << "IPC JSON is not object, size:" << frame.size();
        return;
    }

    const QJsonObject root = doc.object();
    const QString type = root.value("type").toString();

    if (type == "hello") {
        qDebug() << "Pc_data hello:" << root.value("message").toString();
        return;
    }

    if (type == "latest_points") {
        m_lastLatestPointsMs = QDateTime::currentMSecsSinceEpoch();
        m_data->onLatestPointsMessage(root);
        return;
    }

    if (type == "history_points") {
        if (m_trendPage) {
            m_trendPage->onHistoryPointsMessage(root);
        }
        return;
    }

    if (type == "delete_data_ack") {
        qDebug() << "delete data ack:" << root;
        if (root.value("ok").toBool() && m_data) {
            if (m_pendingDeleteAction == "delete_master_data") {
                m_data->removeMasterData(m_pendingDeleteGatewayId, m_pendingDeletePortId);
            } else if (m_pendingDeleteAction == "delete_device_data") {
                m_data->removeDeviceData(m_pendingDeleteGatewayId, m_pendingDeletePortId, m_pendingDeleteDeviceId);
            }
        }
        m_pendingDeleteAction.clear();
        m_pendingDeleteGatewayId.clear();
        m_pendingDeletePortId.clear();
        m_pendingDeleteDeviceId = 0;
        requestLatestPoints();
        return;
    }

    if (type == "ack" || type == "command_ack") {
        m_command->onCommandAck(root);
        return;
    }

    qDebug() << "unknown IPC message type:" << type
             << "frame:" << frame;
}

void MainWindow::requestLatestPoints()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }

    m_ipcClient->sendMessage(QString(R"({"type":"get_latest_points"})"));
}

void MainWindow::sendHistoryQuery(const QString &pointId, qint64 startMs, qint64 endMs, int limit)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || pointId.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("query_history"));
    payload.insert(QStringLiteral("pointId"), pointId);
    payload.insert(QStringLiteral("startMs"), startMs);
    payload.insert(QStringLiteral("endMs"), endMs);
    payload.insert(QStringLiteral("limit"), limit);

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::sendDeleteMasterData(const QString &gatewayId, const QString &portId)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || gatewayId.isEmpty() || portId.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("delete_master_data"));
    payload.insert(QStringLiteral("gatewayId"), gatewayId);
    payload.insert(QStringLiteral("portId"), portId);
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_pendingDeleteAction = QStringLiteral("delete_master_data");
    m_pendingDeleteGatewayId = gatewayId;
    m_pendingDeletePortId = portId;
    m_pendingDeleteDeviceId = 0;

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::sendDeleteDeviceData(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("delete_device_data"));
    payload.insert(QStringLiteral("gatewayId"), gatewayId);
    payload.insert(QStringLiteral("portId"), portId);
    payload.insert(QStringLiteral("deviceId"), deviceId);
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_pendingDeleteAction = QStringLiteral("delete_device_data");
    m_pendingDeleteGatewayId = gatewayId;
    m_pendingDeletePortId = portId;
    m_pendingDeleteDeviceId = deviceId;

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::markIpcDataOffline()
{
    if (m_data) {
        m_data->markAllDevicesOffline();
    }
}

void MainWindow::initManagers()
{
    m_config = new ConfigManager(this);
    m_device = new DeviceManager(this);
    m_alarm = new AlarmManager(this);
    m_command = new CommandManager(this);
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
    m_trendPage = new TrendPage(m_data, this);
    m_deviceConfigPage = new DeviceConfigPage(m_device, m_config, this);
    m_alarmLogPage = new AlarmLogPage(m_alarm, this);
    m_systemSettingPage = new SystemSettingPage(this);

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

    connect(m_alarm, &AlarmManager::activeAlarmCountChanged,
            m_topBar, &TopBar::setAlarmCount);

    connect(m_device, &DeviceManager::onlineGatewayCountChanged,
            m_topBar, &TopBar::setOnlineGatewayCount);

    connect(m_device, &DeviceManager::onlineDeviceCountChanged,
            m_topBar, &TopBar::setOnlineDeviceCount);

    connect(m_trendPage, &TrendPage::historyQueryRequested,
            this, &MainWindow::sendHistoryQuery);

    connect(m_deviceConfigPage, &DeviceConfigPage::deleteMasterDataRequested,
            this, &MainWindow::sendDeleteMasterData);

    connect(m_deviceConfigPage, &DeviceConfigPage::deleteDeviceDataRequested,
            this, &MainWindow::sendDeleteDeviceData);
}
