#include "MainWindow.h"

#include "ui/TopBar.h"
#include "ui/SideBar.h"
#include "ui/CommandTaskPanel.h"
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
#include "core/UiStateStore.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QStatusBar>
#include <QPushButton>
#include <QDebug>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

#include "ipc/ipcclient.h"

namespace {

QString ackCommandName(const QJsonObject &root)
{
    QString command = root.value(QStringLiteral("cmd")).toString();
    if (command.isEmpty()) {
        command = root.value(QStringLiteral("commandType")).toString();
    }
    return command;
}

bool ackSucceeded(const QJsonObject &root)
{
    if (root.value(QStringLiteral("stage")).toString() != QStringLiteral("done")) {
        return false;
    }
    if (root.contains(QStringLiteral("ok"))) {
        return root.value(QStringLiteral("ok")).toBool() &&
               root.value(QStringLiteral("status")).toString() == QStringLiteral("success");
    }

    const QString status = root.value(QStringLiteral("status")).toString();
    return status == QStringLiteral("success");
}

} // namespace

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
    m_uiPullTimer = new QTimer(this);
    m_ipcWatchdogTimer = new QTimer(this);

    connect(m_ipcClient, &IpcClient::connected, this, [this]() {
        qDebug() << "Pc_ui IPC connected";
        if (m_systemSettingPage) {
            m_systemSettingPage->setIpcConnected(true);
        }
        if (m_topBar) {
            m_topBar->setServiceOnline(true);
        }
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_lastIpcMessageMs = now;
        m_lastLatestPointsMs = now;
    });

    connect(m_ipcClient, &IpcClient::disconnected, this, [this]() {
        qDebug() << "Pc_ui IPC disconnected";
        if (m_systemSettingPage) {
            m_systemSettingPage->setIpcConnected(false);
        }
        markIpcDataOffline();
    });

    connect(m_ipcClient, &IpcClient::errorOccured, this, [this](const QString &err) {
        qDebug() << "IPC error:" << err;
        if (m_systemSettingPage) {
            m_systemSettingPage->setIpcConnected(false);
        }
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

        // m_ipcTimer only handles reconnect. Live data is pushed by Pc_data.

    });

    connect(m_uiPullTimer, &QTimer::timeout, this, [this]() {
        requestLatestPoints();
        requestDevices();
        requestGatewayStatus();
        requestPortStatus();
    });

    connect(m_ipcWatchdogTimer, &QTimer::timeout, this, [this]() {
        if (m_stateStore) {
            m_stateStore->refreshOfflineStates(30000);
        }

        if (!m_ipcClient || !m_ipcClient->isConnected()) {
            markIpcDataOffline();
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_lastIpcMessageMs <= 0 || now - m_lastIpcMessageMs > 45000) {
            markIpcDataOffline();
        }
    });

    connect(m_command, &CommandManager::commandMessage, this,
            [this](const QString &, const QString &, const QString &level, const QString &title, const QString &message) {
        const QString text = title + (message.isEmpty() ? QString() : QStringLiteral("：") + message);
        statusBar()->showMessage(text, 8000);
        if (level == QStringLiteral("error") || level == QStringLiteral("warning") || level == QStringLiteral("success")) {
            qDebug() << "command user message:" << level << text;
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
    m_uiPullTimer->start(1000);
    m_ipcWatchdogTimer->start(1000);
    m_ipcClient->connectToServer();
}

void MainWindow::handleIpcMessage(const QByteArray &frame)
{
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(frame, &error);

    if (error.error != QJsonParseError::NoError) {
        qDebug() << "IPC JSON parse error:" << error.errorString()
                 << "bytes:" << frame.size();
        return;
    }

    if (!doc.isObject()) {
        qDebug() << "IPC JSON is not object, size:" << frame.size();
        return;
    }

    const QJsonObject root = doc.object();
    const QString type = root.value("type").toString();
    m_lastIpcMessageMs = QDateTime::currentMSecsSinceEpoch();
    if (m_topBar) {
        m_topBar->setServiceOnline(true);
    }
    qDebug() << "IPC message type:" << (type.isEmpty() ? QStringLiteral("<missing>") : type)
             << "bytes:" << frame.size();

    if (type == "hello") {
        qDebug() << "Pc_data hello:" << root.value("message").toString();
        return;
    }


    if (type == "alarm_event") {
        if (m_alarm) {
            m_alarm->onAlarmMessage(root);
        }
        return;
    }

    if (type == "latest_points" ||
        type == "devices_snapshot" ||
        type == "gateway_status_snapshot" ||
        type == "port_status_snapshot" ||
        type == "state_snapshot" ||
        type == "state_delta") {
        m_lastLatestPointsMs = QDateTime::currentMSecsSinceEpoch();
        if (type == "latest_points") {
            qDebug() << "[DBG_UI_STATE] MainWindow latest_points frameBytes:" << frame.size()
                     << "pointCount:" << root.value(QStringLiteral("points")).toArray().size()
                     << "enterApplyIpcMessage:" << (m_stateStore != nullptr);
        }
        if (m_stateStore) {
            m_stateStore->applyIpcMessage(root);
        }
        if (type == "latest_points") {
            qDebug() << "[DBG_UI_STATE] MainWindow latest_points applyIpcMessage returned";
        }
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
        const QString action = root.value("action").toString(m_pendingDeleteAction);
        if (action == "clear_all_data" && m_systemSettingPage) {
            m_systemSettingPage->onClearAllDataResult(root);
        }
        if (root.value("ok").toBool()) {
            if (action == "delete_master_data") {
                if (m_stateStore) {
                    m_stateStore->removeMasterData(m_pendingDeleteGatewayId, m_pendingDeletePortId);
                }
            } else if (action == "delete_device_data") {
                onRemoveDeviceSucceeded(m_pendingDeleteGatewayId,
                                        m_pendingDeletePortId,
                                        m_pendingDeleteDeviceId);
            } else if (action == "clear_recovered_alarms" && m_alarm) {
                m_alarm->clearRecoveredAlarms();
            } else if (action == "clear_all_data") {
                if (m_stateStore) {
                    m_stateStore->clearRuntimeData();
                }
            }
        }
        m_pendingDeleteAction.clear();
        m_pendingDeleteGatewayId.clear();
        m_pendingDeletePortId.clear();
        m_pendingDeleteDeviceId = 0;
        requestFullSnapshot();
        return;
    }

    if (type == "mqtt_config") {
        if (m_systemSettingPage) {
            m_systemSettingPage->onMqttConfigMessage(root);
        }
        return;
    }

    if (type == "mqtt_config_ack") {
        if (m_systemSettingPage) {
            m_systemSettingPage->onMqttConfigAck(root);
        }
        return;
    }

    if (type == "sync_config_result") {
        if (m_deviceConfigPage) {
            m_deviceConfigPage->onSyncConfigResult(root);
        }
        requestFullSnapshot();
        return;
    }

    if (type == "ack" || type == "command_ack") {
        const QString command = ackCommandName(root);
        const bool ok = ackSucceeded(root);
        const QString stage = root.value(QStringLiteral("stage")).toString();
        m_command->onCommandAck(root);
        if (command == QStringLiteral("remove_device")) {
            if (stage == QStringLiteral("done") && ok) {
                QString gatewayId = root.value(QStringLiteral("gatewayId")).toString();
                QString portId = root.value(QStringLiteral("portId")).toString();
                int deviceId = root.value(QStringLiteral("deviceId")).toInt(
                    root.value(QStringLiteral("slave_id")).toInt(root.value(QStringLiteral("slaveId")).toInt()));
                if (gatewayId.isEmpty() && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                    gatewayId = m_pendingDeleteGatewayId;
                }
                if (portId.isEmpty() && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                    portId = m_pendingDeletePortId;
                }
                if (deviceId <= 0 && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                    deviceId = m_pendingDeleteDeviceId;
                }
                onRemoveDeviceSucceeded(gatewayId, portId, deviceId);
            } else if (!ok) {
                m_pendingDeleteAction.clear();
                m_pendingDeleteGatewayId.clear();
                m_pendingDeletePortId.clear();
                m_pendingDeleteDeviceId = 0;
                requestFullSnapshot();
            }
        } else if (command == QStringLiteral("add_device") && stage == QStringLiteral("done")) {
            if (ok) {
                const QString gatewayId = root.value(QStringLiteral("gatewayId")).toString();
                const QString portId = root.value(QStringLiteral("portId")).toString();
                const int deviceId = root.value(QStringLiteral("deviceId")).toInt(
                    root.value(QStringLiteral("slave_id")).toInt(root.value(QStringLiteral("slaveId")).toInt()));
                onAddDeviceSucceeded(gatewayId, portId, deviceId);
                requestGatewayStatus();
            }
            requestFullSnapshot();
        }
        return;
    }

    if (type == "command_log_update") {
        qDebug() << "command log update:" << root;
        const QString commandType = root.value(QStringLiteral("commandType")).toString();
        if (m_command) {
            m_command->onCommandLogUpdate(root);
        }
        const QString stage = root.value(QStringLiteral("stage")).toString();
        const QString status = root.value(QStringLiteral("status")).toString();
        if (stage == QStringLiteral("done") && status == QStringLiteral("success") &&
            (commandType == QStringLiteral("add_device") ||
             commandType == QStringLiteral("set_relay") ||
             commandType == QStringLiteral("set_threshold") ||
             commandType == QStringLiteral("set_device_threshold") ||
             commandType == QStringLiteral("cache") ||
             commandType == QStringLiteral("sync") ||
             commandType == QStringLiteral("get_config"))) {
            if (commandType == QStringLiteral("add_device")) {
                const QString gatewayId = root.value(QStringLiteral("gatewayId")).toString();
                const QString portId = root.value(QStringLiteral("portId")).toString();
                const int deviceId = root.value(QStringLiteral("deviceId")).toInt(
                    root.value(QStringLiteral("slave_id")).toInt(root.value(QStringLiteral("slaveId")).toInt()));
                onAddDeviceSucceeded(gatewayId, portId, deviceId);
            }
            requestFullSnapshot();
            requestGatewayStatus();
        }
        if (commandType == QStringLiteral("remove_device")) {
            QString gatewayId = root.value(QStringLiteral("gatewayId")).toString();
            QString portId = root.value(QStringLiteral("portId")).toString();
            int deviceId = root.value(QStringLiteral("deviceId")).toInt();

            if (gatewayId.isEmpty() && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                gatewayId = m_pendingDeleteGatewayId;
            }
            if (portId.isEmpty() && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                portId = m_pendingDeletePortId;
            }
            if (deviceId <= 0 && m_pendingDeleteAction == QStringLiteral("delete_device_data")) {
                deviceId = m_pendingDeleteDeviceId;
            }

            if (status == QStringLiteral("success") &&
                !gatewayId.isEmpty() && !portId.isEmpty() && deviceId > 0) {
                onRemoveDeviceSucceeded(gatewayId, portId, deviceId);
            } else {
                requestFullSnapshot();
            }

            m_pendingDeleteAction.clear();
            m_pendingDeleteGatewayId.clear();
            m_pendingDeletePortId.clear();
            m_pendingDeleteDeviceId = 0;
        }
        return;
    }

    qDebug() << "unknown IPC message type:" << type
             << "bytes:" << frame.size();
}

void MainWindow::requestLatestPoints()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }

    m_ipcClient->sendMessage(QString(R"({"type":"get_latest_points"})"));
}

void MainWindow::requestDevices()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }
    m_ipcClient->sendMessage(QString(R"({"type":"get_devices"})"));
}

void MainWindow::requestGatewayStatus()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }
    m_ipcClient->sendMessage(QString(R"({"type":"get_gateway_status"})"));
}

void MainWindow::requestPortStatus()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }
    m_ipcClient->sendMessage(QString(R"({"type":"get_port_status"})"));
}

void MainWindow::requestFullSnapshot()
{
    requestLatestPoints();
    requestDevices();
    requestGatewayStatus();
    requestPortStatus();
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

    bool sentDeviceRemove = false;
    if (m_device && m_command) {
        const QList<DeviceNode> devices = m_device->allDevices();
        for (const DeviceNode &device : devices) {
            if (device.gatewayId != gatewayId || device.port != portId || device.deviceId <= 0) {
                continue;
            }

            m_command->sendRemoveDeviceCommand(gatewayId, portId, device.deviceId);
            sentDeviceRemove = true;
        }
    }

    if (sentDeviceRemove) {
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
    if (!m_ipcClient || !m_ipcClient->isConnected() || !m_command ||
        gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        if (m_deviceConfigPage) {
            m_deviceConfigPage->onCommandTargetStateChanged(QString(),
                                                            QStringLiteral("remove_device"),
                                                            gatewayId,
                                                            portId,
                                                            deviceId,
                                                            QStringLiteral("删除从站失败"),
                                                            QStringLiteral("IPC 未连接或请求参数无效"),
                                                            QString());
        }
        statusBar()->showMessage(QStringLiteral("删除从站失败：IPC 未连接或请求参数无效"), 8000);
        return;
    }

    m_pendingDeleteAction = QStringLiteral("delete_device_data");
    m_pendingDeleteGatewayId = gatewayId;
    m_pendingDeletePortId = portId;
    m_pendingDeleteDeviceId = deviceId;

    qDebug() << "Pc_ui send remove_device" << "gatewayId" << gatewayId
             << "portId" << portId << "slaveId" << deviceId;
    m_command->sendRemoveDeviceCommand(gatewayId, portId, deviceId);
}

void MainWindow::onAddDeviceSucceeded(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    if (m_stateStore) {
        m_stateStore->forgetRemovedDevice(gatewayId, portId, deviceId);
    }
}

void MainWindow::onRemoveDeviceSucceeded(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    if (m_stateStore) {
        m_stateStore->removeDeviceData(gatewayId, portId, deviceId);
    }
    if (m_alarm) {
        m_alarm->removeDeviceAlarms(gatewayId, portId, deviceId);
    }
    m_pendingDeleteAction.clear();
    m_pendingDeleteGatewayId.clear();
    m_pendingDeletePortId.clear();
    m_pendingDeleteDeviceId = 0;
    requestFullSnapshot();
}

void MainWindow::sendAddSlaveCommand(const QString &gatewayId, const QString &portId, int deviceId,
                                     const QString &deviceType, int pollIntervalMs,
                                     const QVariantMap &deviceOptions)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || !m_command ||
        gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0 ||
        deviceType.isEmpty() || pollIntervalMs <= 0) {
        if (m_deviceConfigPage) {
            m_deviceConfigPage->onCommandTargetStateChanged(QString(),
                                                            QStringLiteral("add_device"),
                                                            gatewayId,
                                                            portId,
                                                            deviceId,
                                                            QStringLiteral("添加从站失败"),
                                                            QStringLiteral("IPC 未连接或请求参数无效"),
                                                            QString());
        }
        statusBar()->showMessage(QStringLiteral("添加从站失败：IPC 未连接或请求参数无效"), 8000);
        return;
    }

    m_command->sendAddDeviceCommand(gatewayId, portId, deviceId, deviceType, pollIntervalMs, deviceOptions);
}

void MainWindow::sendSyncConfigRequest(const QJsonArray &targets)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || targets.isEmpty()) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("sync_config_request"));
    payload.insert(QStringLiteral("targets"), targets);
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::sendClearRecoveredAlarms()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        qDebug() << "clear_recovered_alarms skipped: IPC is not connected.";
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("clear_recovered_alarms"));
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_pendingDeleteAction = QStringLiteral("clear_recovered_alarms");
    m_pendingDeleteGatewayId.clear();
    m_pendingDeletePortId.clear();
    m_pendingDeleteDeviceId = 0;

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::sendClearAllData()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        qDebug() << "clear_all_data skipped: IPC is not connected.";
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("clear_all_data"));
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_pendingDeleteAction = QStringLiteral("clear_all_data");
    m_pendingDeleteGatewayId.clear();
    m_pendingDeletePortId.clear();
    m_pendingDeleteDeviceId = 0;

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::requestMqttConfig()
{
    if (!m_ipcClient || !m_ipcClient->isConnected()) {
        return;
    }

    m_ipcClient->sendMessage(QString(R"({"type":"get_mqtt_config"})"));
}

void MainWindow::saveMqttConfig(const QString &host, int port)
{
    if (!m_ipcClient || !m_ipcClient->isConnected() || host.trimmed().isEmpty() || port < 1 || port > 65535) {
        return;
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("type"), QStringLiteral("save_mqtt_config"));
    payload.insert(QStringLiteral("host"), host.trimmed());
    payload.insert(QStringLiteral("port"), port);
    payload.insert(QStringLiteral("timestamp"), QDateTime::currentMSecsSinceEpoch());

    m_ipcClient->sendMessage(QJsonDocument(payload).toJson(QJsonDocument::Compact));
}

void MainWindow::markIpcDataOffline()
{
    if (m_topBar) {
        m_topBar->setServiceOnline(false);
    }
    if (m_stateStore) {
        m_stateStore->markAllDevicesOffline();
    }
}

void MainWindow::initManagers()
{
    m_config = new ConfigManager(this);
    m_device = new DeviceManager(this);
    m_alarm = new AlarmManager(this);
    m_command = new CommandManager(this);
    m_data = new DataManager(m_device, m_alarm, this);
    m_stateStore = new UiStateStore(m_data, m_device, m_alarm, this);
}

void MainWindow::initUi()
{
    setWindowTitle(QStringLiteral("Pc_mqtt 工业物联网监控平台"));
    statusBar()->showMessage(QStringLiteral("就绪"));
    m_commandTaskButton = new QPushButton(QStringLiteral("命令任务：0 个进行中"), this);
    m_commandTaskButton->setObjectName(QStringLiteral("CommandTaskStatusLabel"));
    statusBar()->addPermanentWidget(m_commandTaskButton);

    auto *central = new QWidget(this);
    central->setObjectName("MainCentral");
    auto *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_sideBar = new SideBar(this);

    auto *rightArea = new QWidget(this);
    rightArea->setObjectName("MainRightArea");
    auto *rightLayout = new QVBoxLayout(rightArea);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_topBar = new TopBar(this);
    m_stack = new QStackedWidget(this);
    m_stack->setObjectName("MainStack");

    m_dashboardPage = new DashboardPage(m_data, m_device, m_alarm, m_stateStore, this);
    m_monitorPage = new MonitorPage(m_data, m_command, m_stateStore, this);
    m_trendPage = new TrendPage(m_data, m_stateStore, this);
    m_deviceConfigPage = new DeviceConfigPage(m_device, m_config, m_data, m_stateStore, this);
    m_alarmLogPage = new AlarmLogPage(m_alarm, this);
    m_systemSettingPage = new SystemSettingPage(this);
    m_commandTaskPanel = new CommandTaskPanel(this);
    m_commandTaskPanel->hide();

    m_stack->addWidget(m_dashboardPage);
    m_stack->addWidget(m_monitorPage);
    m_stack->addWidget(m_trendPage);
    m_stack->addWidget(m_deviceConfigPage);
    m_stack->addWidget(m_alarmLogPage);
    m_stack->addWidget(m_systemSettingPage);

    rightLayout->addWidget(m_topBar);
    rightLayout->addWidget(m_stack, 1);

    mainLayout->addWidget(m_sideBar);
    mainLayout->addWidget(rightArea, 1);

    setCentralWidget(central);
}

void MainWindow::initConnections()
{
    connect(m_sideBar, &SideBar::pageChanged, m_stack, &QStackedWidget::setCurrentIndex);

    connect(m_commandTaskButton, &QPushButton::clicked, this, [this]() {
        if (!m_commandTaskPanel) {
            return;
        }
        m_commandTaskPanel->show();
        m_commandTaskPanel->raise();
        m_commandTaskPanel->activateWindow();
    });

    connect(m_commandTaskPanel, &CommandTaskPanel::runningTaskCountChanged,
            this, [this](int count) {
        if (m_commandTaskButton) {
            m_commandTaskButton->setText(QStringLiteral("命令任务：%1 个进行中").arg(count));
        }
    });

    connect(m_command, &CommandManager::commandTargetStateChanged,
            m_commandTaskPanel, &CommandTaskPanel::upsertCommandTask);

    connect(m_command, &CommandManager::commandTargetStateChanged,
            m_deviceConfigPage, &DeviceConfigPage::onCommandTargetStateChanged);

    connect(m_stateStore, &UiStateStore::stateChanged, this, [this]() {
        if (!m_topBar || !m_stateStore) {
            return;
        }
        m_topBar->setOnlineGatewayCount(m_stateStore->onlineGatewayCount());
        m_topBar->setOnlineDeviceCount(m_stateStore->onlineDeviceCount());
        m_topBar->setAlarmCount(m_stateStore->activeAlarmCount());
    });

    connect(m_trendPage, &TrendPage::historyQueryRequested,
            this, &MainWindow::sendHistoryQuery);

    connect(m_deviceConfigPage, &DeviceConfigPage::deleteMasterDataRequested,
            this, &MainWindow::sendDeleteMasterData);

    connect(m_deviceConfigPage, &DeviceConfigPage::deleteDeviceDataRequested,
            this, &MainWindow::sendDeleteDeviceData);

    connect(m_deviceConfigPage, &DeviceConfigPage::addSlaveRequested,
            this, &MainWindow::sendAddSlaveCommand);

    connect(m_deviceConfigPage, &DeviceConfigPage::syncConfigRequested,
            this, &MainWindow::sendSyncConfigRequest);

    connect(m_alarmLogPage, &AlarmLogPage::clearRecoveredAlarmsRequested,
            this, &MainWindow::sendClearRecoveredAlarms);

    connect(m_systemSettingPage, &SystemSettingPage::mqttConfigRequested,
            this, &MainWindow::requestMqttConfig);

    connect(m_systemSettingPage, &SystemSettingPage::mqttConfigSaveRequested,
            this, &MainWindow::saveMqttConfig);

    connect(m_systemSettingPage, &SystemSettingPage::clearAllDataRequested,
            this, &MainWindow::sendClearAllData);
}
