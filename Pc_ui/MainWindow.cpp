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
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>

#include "ipc/ipcclient.h"

namespace {

QList<GatewayNode> parseGatewayStatusSnapshot(const QJsonObject &root)
{
    QList<GatewayNode> gateways;
    const QJsonArray rows = root.value(QStringLiteral("gateways")).toArray();
    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject row = value.toObject();
        GatewayNode gateway;
        gateway.gatewayId = row.value(QStringLiteral("gatewayId")).toString();
        gateway.gatewayName = row.value(QStringLiteral("gatewayName")).toString();
        gateway.factoryId = row.value(QStringLiteral("factoryId")).toString();
        gateway.areaId = row.value(QStringLiteral("areaId")).toString();
        gateway.status = row.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        gateway.lastRegisterTimeMs = row.value(QStringLiteral("lastRegisterTimeMs")).toVariant().toLongLong();
        gateway.lastHeartbeatTimeMs = row.value(QStringLiteral("lastHeartbeatTimeMs")).toVariant().toLongLong();
        gateway.updateTimeMs = row.value(QStringLiteral("updateTimeMs")).toVariant().toLongLong();
        if (!gateway.gatewayId.isEmpty()) {
            gateways.append(gateway);
        }
    }
    return gateways;
}

QList<PortNode> parsePortStatusSnapshot(const QJsonObject &root)
{
    QList<PortNode> ports;
    const QJsonArray rows = root.value(QStringLiteral("ports")).toArray();
    for (const QJsonValue &value : rows) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject row = value.toObject();
        PortNode port;
        port.gatewayId = row.value(QStringLiteral("gatewayId")).toString();
        port.portId = row.value(QStringLiteral("portId")).toString();
        port.portName = row.value(QStringLiteral("portName")).toString();
        port.slot = row.value(QStringLiteral("slot")).toInt();
        port.devicePath = row.value(QStringLiteral("devicePath")).toString();
        port.baud = row.value(QStringLiteral("baud")).toInt();
        port.status = row.value(QStringLiteral("status")).toString(QStringLiteral("unknown"));
        port.lastRegisterTimeMs = row.value(QStringLiteral("lastRegisterTimeMs")).toVariant().toLongLong();
        port.updateTimeMs = row.value(QStringLiteral("updateTimeMs")).toVariant().toLongLong();
        if (!port.gatewayId.isEmpty() && !port.portId.isEmpty()) {
            ports.append(port);
        }
    }
    return ports;
}

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
    if (root.contains(QStringLiteral("ok"))) {
        return root.value(QStringLiteral("ok")).toBool();
    }

    const QString status = root.value(QStringLiteral("status")).toString();
    return status == QStringLiteral("ok") || status == QStringLiteral("success");
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
    m_ipcWatchdogTimer = new QTimer(this);
    m_snapshotFallbackTimer = new QTimer(this);

    connect(m_ipcClient, &IpcClient::connected, this, [this]() {
        qDebug() << "Pc_ui IPC connected";
        if (m_systemSettingPage) {
            m_systemSettingPage->setIpcConnected(true);
        }
        if (m_topBar) {
            m_topBar->setServiceOnline(true);
        }
        requestFullSnapshot();
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

    connect(m_snapshotFallbackTimer, &QTimer::timeout, this, [this]() {
        if (m_ipcClient && m_ipcClient->isConnected()) {
            requestFullSnapshot();
        }
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
    m_snapshotFallbackTimer->start(30000);
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
    qDebug() << "IPC message type:" << (type.isEmpty() ? QStringLiteral("<missing>") : type);

    if (type == "hello") {
        qDebug() << "Pc_data hello:" << root.value("message").toString();
        return;
    }


    if (type == "alarm_event") {
        if (m_alarm) {
            m_alarm->onAlarmMessage(root);
        }
        if (m_alarmLogPage) {
            m_alarmLogPage->refreshTable();
        }
        return;
    }

    if (type == "latest_points") {
        m_lastLatestPointsMs = QDateTime::currentMSecsSinceEpoch();
        m_data->onLatestPointsMessage(root);
        return;
    }

    if (type == "devices_snapshot") {
        if (m_data) {
            m_data->onDevicesSnapshotMessage(root);
        }
        return;
    }

    if (type == "gateway_status_snapshot") {
        if (m_device) {
            m_device->setGateways(parseGatewayStatusSnapshot(root));
        }
        return;
    }

    if (type == "port_status_snapshot") {
        if (m_device) {
            m_device->setPorts(parsePortStatusSnapshot(root));
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
        if (root.value("ok").toBool() && m_data) {
            if (action == "delete_master_data") {
                m_data->removeMasterData(m_pendingDeleteGatewayId, m_pendingDeletePortId);
            } else if (action == "delete_device_data") {
                onRemoveDeviceSucceeded(m_pendingDeleteGatewayId,
                                        m_pendingDeletePortId,
                                        m_pendingDeleteDeviceId);
            } else if (action == "clear_recovered_alarms" && m_alarm) {
                m_alarm->clearRecoveredAlarms();
            } else if (action == "clear_all_data") {
                m_data->clearRuntimeData();
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
            if (!ok) {
                m_pendingDeleteAction.clear();
                m_pendingDeleteGatewayId.clear();
                m_pendingDeletePortId.clear();
                m_pendingDeleteDeviceId = 0;
                requestFullSnapshot();
            }
        } else if (command == QStringLiteral("add_device") && stage == QStringLiteral("done") && !ok) {
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
             << "frame:" << frame;
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
    requestGatewayStatus();
    requestPortStatus();
    requestDevices();
    requestLatestPoints();
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

void MainWindow::onRemoveDeviceSucceeded(const QString &gatewayId, const QString &portId, int deviceId)
{
    if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
        return;
    }

    if (m_data) {
        m_data->removeDeviceData(gatewayId, portId, deviceId);
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
                                     const QString &deviceType, int pollIntervalMs)
{
    if (m_command) {
        m_command->sendAddDeviceCommand(gatewayId, portId, deviceId, deviceType, pollIntervalMs);
    }
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
    statusBar()->showMessage(QStringLiteral("就绪"));

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
