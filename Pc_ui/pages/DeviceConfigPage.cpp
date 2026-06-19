#include "DeviceConfigPage.h"

#include "core/ConfigManager.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/UiStateStore.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

namespace {

constexpr int kRefreshDelayMs = 250;
constexpr int kRealtimeRefreshMs = 500;
constexpr int kSlaveColTemperature = 6;
constexpr int kSlaveColHumidity = 7;
constexpr int kSlaveColRelay = 8;
constexpr int kSlaveColLastUpdate = 9;
constexpr int kSlaveColStatus = 10;
constexpr int kSlaveColSource = 11;

QString masterKey(const QString &gatewayId, const QString &portId)
{
    return gatewayId + QStringLiteral("/") + portId;
}

QString defaultPortName(const DeviceNode &node)
{
    if (!node.masterName.isEmpty()) {
        return node.masterName;
    }
    if (!node.port.isEmpty()) {
        return node.port;
    }
    return QStringLiteral("RS485-%1").arg(node.masterSlot + 1);
}

QTableWidgetItem *readonlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

} // namespace

DeviceConfigPage::DeviceConfigPage(DeviceManager *device, ConfigManager *config,
                                   DataManager *dataManager, UiStateStore *stateStore,
                                   QWidget *parent)
    : QWidget(parent), m_device(device), m_config(config), m_dataManager(dataManager), m_stateStore(stateStore)
{
    Q_UNUSED(m_config);
    Q_UNUSED(m_stateStore);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("设备配置"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *masterHeader = new QHBoxLayout;
    auto *masterTitle = new QLabel(QStringLiteral("主站配置"), this);
    masterTitle->setObjectName("PageTitle");
    m_syncButton = new QPushButton(QStringLiteral("配置同步"), this);
    auto *scanMaster = new QPushButton(QStringLiteral("扫描主站"), this);
    scanMaster->setEnabled(false);
    scanMaster->setToolTip(QStringLiteral("后端扫描链路未完整接入，暂时禁用以避免误操作"));
    masterHeader->addWidget(masterTitle);
    masterHeader->addStretch();
    masterHeader->addWidget(m_syncButton);
    masterHeader->addWidget(scanMaster);
    layout->addLayout(masterHeader);

    m_masterTable = new QTableWidget(this);
    m_masterTable->setColumnCount(10);
    m_masterTable->setHorizontalHeaderLabels({
        QStringLiteral("主站名称"),
        QStringLiteral("网关"),
        QStringLiteral("串口设备"),
        QStringLiteral("波特率"),
        QStringLiteral("区域"),
        QStringLiteral("从站数"),
        QStringLiteral("最后更新"),
        QStringLiteral("状态"),
        QStringLiteral("来源"),
        QStringLiteral("操作")
    });
    setupTable(m_masterTable);
    layout->addWidget(m_masterTable, 1);

    auto *slaveHeader = new QHBoxLayout;
    auto *slaveTitle = new QLabel(QStringLiteral("从站配置"), this);
    slaveTitle->setObjectName("PageTitle");
    auto *addSlave = new QPushButton(QStringLiteral("添加从站"), this);
    auto *scanSlave = new QPushButton(QStringLiteral("扫描从站"), this);
    scanSlave->setEnabled(false);
    scanSlave->setToolTip(QStringLiteral("后端扫描链路未完整接入，暂时禁用以避免误操作"));
    slaveHeader->addWidget(slaveTitle);
    slaveHeader->addStretch();
    slaveHeader->addWidget(addSlave);
    slaveHeader->addWidget(scanSlave);
    layout->addLayout(slaveHeader);

    m_slaveTable = new QTableWidget(this);
    m_slaveTable->setColumnCount(13);
    m_slaveTable->setHorizontalHeaderLabels({
        QStringLiteral("主站"),
        QStringLiteral("地址"),
        QStringLiteral("类型"),
        QStringLiteral("名称"),
        QStringLiteral("区域"),
        QStringLiteral("网关"),
        QStringLiteral("温度"),
        QStringLiteral("湿度"),
        QStringLiteral("继电器"),
        QStringLiteral("最后更新"),
        QStringLiteral("状态"),
        QStringLiteral("来源"),
        QStringLiteral("操作")
    });
    setupTable(m_slaveTable);
    layout->addWidget(m_slaveTable, 1);

    connect(scanMaster, &QPushButton::clicked, this, &DeviceConfigPage::showScanPlaceholder);
    connect(m_syncButton, &QPushButton::clicked, this, &DeviceConfigPage::showSyncConfigDialog);
    connect(addSlave, &QPushButton::clicked, this, &DeviceConfigPage::showAddSlaveDialog);
    connect(scanSlave, &QPushButton::clicked, this, &DeviceConfigPage::showScanPlaceholder);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &DeviceConfigPage::refreshTables);

    m_realtimeTimer = new QTimer(this);
    m_realtimeTimer->setInterval(kRealtimeRefreshMs);
    connect(m_realtimeTimer, &QTimer::timeout, this, &DeviceConfigPage::refreshRealtimeColumns);

    if (m_device) {
        connect(m_device, &DeviceManager::deviceConfigChanged,
                this, &DeviceConfigPage::scheduleRefreshTables);
        connect(m_device, &DeviceManager::gatewayStatusChanged,
                this, &DeviceConfigPage::scheduleRefreshTables);
        connect(m_device, &DeviceManager::portStatusChanged,
                this, &DeviceConfigPage::scheduleRefreshTables);
        connect(m_device, &DeviceManager::deviceOnlineStateChanged,
                this, [this](const QString &, bool) { refreshRealtimeColumns(); });
    }
    if (m_dataManager) {
        connect(m_dataManager, &DataManager::deviceTreeChanged,
                this, &DeviceConfigPage::scheduleRefreshTables);
        connect(m_dataManager, &DataManager::realtimeDataUpdated,
                this, [this]() {
            if (isVisible()) {
                refreshRealtimeColumns();
            }
        });
    }

    // DeviceManager owns structure changes. DataManager owns realtime cache updates.

    refreshTables();
}

void DeviceConfigPage::scheduleRefreshTables()
{
    m_refreshDirty = true;
    const bool willStart = isVisible() && m_refreshTimer && !m_refreshTimer->isActive();
    qDebug() << "[DBG_PAGE] DeviceConfigPage scheduleRefreshTables visible:" << isVisible()
             << "startDebounce250ms:" << willStart;
    if (!isVisible()) {
        return;
    }

    if (m_refreshTimer && !m_refreshTimer->isActive()) {
        m_refreshTimer->start(kRefreshDelayMs);
    }
}

void DeviceConfigPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_realtimeTimer && !m_realtimeTimer->isActive()) {
        m_realtimeTimer->start();
    }
    if (m_refreshDirty) {
        scheduleRefreshTables();
    }
    refreshRealtimeColumns();
}

void DeviceConfigPage::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    if (m_realtimeTimer) {
        m_realtimeTimer->stop();
    }
}

void DeviceConfigPage::refreshTables()
{
    m_refreshDirty = false;
    const QList<DeviceNode> devices = m_dataManager ? m_dataManager->deviceTreeSnapshot() : QList<DeviceNode>();
    const QList<MasterRow> masters = buildMasterRows();
    m_slaveRowByDeviceKey.clear();
    qDebug() << "[DBG_PAGE] DeviceConfigPage refreshTables executed deviceCount:"
             << devices.size()
             << "masterRowCount:" << masters.size()
             << "slaveRowCount:" << devices.size();

    m_masterTable->setRowCount(masters.size());
    for (int row = 0; row < masters.size(); ++row) {
        const MasterRow &m = masters.at(row);
        const QString portName = m.portName.isEmpty()
            ? QStringLiteral("RS485-%1").arg(m.masterSlot + 1)
            : m.portName;

        m_masterTable->setItem(row, 0, readonlyItem(portName));
        m_masterTable->setItem(row, 1, readonlyItem(m.gatewayName.isEmpty() ? m.gatewayId : m.gatewayName));
        m_masterTable->setItem(row, 2, readonlyItem(m.devicePath.isEmpty() ? m.portId : m.devicePath));
        m_masterTable->setItem(row, 3, readonlyItem(QString::number(m.baud)));
        m_masterTable->setItem(row, 4, readonlyItem(m.areaName));
        m_masterTable->setItem(row, 5, readonlyItem(QString::number(m.slaveCount)));
        m_masterTable->setItem(row, 6, readonlyItem(displayTime(m.lastUpdateTime)));
        addStatusItem(m_masterTable, row, 7, m.status);
        m_masterTable->setItem(row, 8, readonlyItem(m.portId.isEmpty()
            ? QStringLiteral("暂无已连接端口，请先在板端扫描并连接端口")
            : QStringLiteral("端口注册")));
        if (m.portId.isEmpty()) {
            m_masterTable->setItem(row, 9, readonlyItem(QStringLiteral("-")));
        } else {
            addDeleteButton(m_masterTable, row, true, m.gatewayId, m.portId);
        }
    }

    m_slaveTable->setRowCount(devices.size());
    for (int row = 0; row < devices.size(); ++row) {
        const DeviceNode &d = devices.at(row);
        const QString portName = defaultPortName(d);

        m_slaveTable->setItem(row, 0, readonlyItem(portName));
        m_slaveTable->setItem(row, 1, readonlyItem(QString::number(d.slaveAddr)));
        m_slaveTable->setItem(row, 2, readonlyItem(d.deviceType));
        m_slaveTable->setItem(row, 3, readonlyItem(d.deviceName));
        m_slaveTable->setItem(row, 4, readonlyItem(d.areaName));
        m_slaveTable->setItem(row, 5, readonlyItem(d.gatewayName.isEmpty() ? d.gatewayId : d.gatewayName));
        m_slaveTable->setItem(row, kSlaveColTemperature, readonlyItem(QStringLiteral("-")));
        m_slaveTable->setItem(row, kSlaveColHumidity, readonlyItem(QStringLiteral("-")));
        m_slaveTable->setItem(row, kSlaveColRelay, readonlyItem(QStringLiteral("-")));
        m_slaveTable->setItem(row, kSlaveColLastUpdate, readonlyItem(displayTime(d.lastUpdateTime)));
        addStatusItem(m_slaveTable, row, kSlaveColStatus, d.online ? QStringLiteral("online") : d.status);
        m_slaveTable->setItem(row, kSlaveColSource, readonlyItem(QStringLiteral("设备表")));
        m_slaveRowByDeviceKey.insert(d.key(), row);
        addDeleteButton(m_slaveTable, row, false, d.gatewayId, d.port, d.slaveAddr > 0 ? d.slaveAddr : d.deviceId);
    }
    refreshRealtimeColumns();
}

void DeviceConfigPage::refreshRealtimeColumns()
{
    if (!m_slaveTable || !m_dataManager) {
        return;
    }

    const QList<DeviceNode> devices = m_dataManager->deviceTreeSnapshot();
    int updatedRows = 0;
    for (const DeviceNode &device : devices) {
        const int row = m_slaveRowByDeviceKey.value(device.key(), -1);
        if (row < 0 || row >= m_slaveTable->rowCount()) {
            continue;
        }

        const RealtimeDeviceData realtime = m_dataManager
            ? m_dataManager->deviceData(device.key())
            : RealtimeDeviceData();
        const bool hasRealtimeNode = !realtime.node.gatewayId.isEmpty() &&
                                     !realtime.node.port.isEmpty() &&
                                     realtime.node.deviceId > 0;
        const DeviceNode displayNode = hasRealtimeNode ? realtime.node : device;

        m_slaveTable->setItem(row, kSlaveColTemperature, readonlyItem(realtimeTemperatureText(device.key())));
        m_slaveTable->setItem(row, kSlaveColHumidity, readonlyItem(realtimeHumidityText(device.key())));
        m_slaveTable->setItem(row, kSlaveColRelay, readonlyItem(realtimeRelayText(device.key())));
        m_slaveTable->setItem(row, kSlaveColLastUpdate, readonlyItem(displayTime(displayNode.lastUpdateTime)));
        addStatusItem(m_slaveTable, row, kSlaveColStatus, displayNode.online ? QStringLiteral("online") : displayNode.status);
        m_slaveTable->setItem(row, kSlaveColSource, readonlyItem(realtimeSourceText(device.key())));
        ++updatedRows;
    }

    qDebug() << "[DBG_PAGE] DeviceConfigPage refreshRealtimeColumns updatedRows:"
             << updatedRows
             << "slaveRowCount:" << m_slaveTable->rowCount();
}

QList<DeviceConfigPage::MasterRow> DeviceConfigPage::buildMasterRows() const
{
    QMap<QString, MasterRow> rows;
    const QList<DeviceNode> devices = m_dataManager ? m_dataManager->deviceTreeSnapshot() : QList<DeviceNode>();
    const QList<GatewayNode> gateways = m_dataManager ? m_dataManager->gatewaySnapshot() : QList<GatewayNode>();
    const QList<PortNode> ports = m_dataManager ? m_dataManager->portSnapshot() : QList<PortNode>();
    QMap<QString, GatewayNode> gatewayMap;

    for (const GatewayNode &gateway : gateways) {
        gatewayMap.insert(gateway.gatewayId, gateway);
    }

    for (const PortNode &p : ports) {
        const QString key = masterKey(p.gatewayId, p.portId);
        MasterRow row;
        const GatewayNode gateway = gatewayMap.value(p.gatewayId);
        row.gatewayId = p.gatewayId;
        row.gatewayName = gateway.gatewayName;
        row.portId = p.portId;
        row.portName = p.portName.isEmpty() ? p.portId : p.portName;
        row.devicePath = p.devicePath;
        row.masterSlot = p.slot;
        row.baud = p.baud;
        row.areaName = gateway.areaId;
        row.lastUpdateTime = p.updateTimeMs;
        row.status = p.status;
        rows.insert(key, row);
    }

    for (const GatewayNode &gateway : gateways) {
        bool hasPort = false;
        for (const PortNode &port : ports) {
            if (port.gatewayId == gateway.gatewayId) {
                hasPort = true;
                break;
            }
        }
        if (!hasPort) {
            MasterRow row;
            row.gatewayId = gateway.gatewayId;
            row.gatewayName = gateway.gatewayName;
            row.portName = QStringLiteral("暂无端口");
            row.areaName = gateway.areaId;
            row.lastUpdateTime = gateway.updateTimeMs;
            row.status = gateway.status;
            rows.insert(masterKey(gateway.gatewayId, QString()), row);
        }
    }

    for (const DeviceNode &d : devices) {
        const QString portId = d.port.isEmpty()
            ? QStringLiteral("RS485-%1").arg(d.masterSlot + 1)
            : d.port;
        const QString key = masterKey(d.gatewayId, portId);
        MasterRow row = rows.value(key);
        if (row.gatewayId.isEmpty()) {
            row.gatewayId = d.gatewayId;
            row.gatewayName = d.gatewayName;
            row.portId = portId;
            row.portName = defaultPortName(d);
            row.masterSlot = d.masterSlot;
            row.baud = d.baud;
            row.areaName = d.areaName;
            row.status = d.online ? QStringLiteral("connected") : QStringLiteral("unknown");
        }
        if (d.lastUpdateTime > row.lastUpdateTime) row.lastUpdateTime = d.lastUpdateTime;
        ++row.slaveCount;
        rows.insert(key, row);
    }

    return rows.values();
}

QString DeviceConfigPage::statusText(const QString &status) const
{
    if (status == QStringLiteral("online") || status == QStringLiteral("connected")) {
        return QStringLiteral("在线");
    }
    if (status == QStringLiteral("stale")) {
        return QStringLiteral("心跳超时");
    }
    if (status == QStringLiteral("disconnected")) {
        return QStringLiteral("已断开");
    }
    if (status == QStringLiteral("offline")) {
        return QStringLiteral("离线");
    }
    if (status == QStringLiteral("service_offline")) {
        return QStringLiteral("服务离线，禁止配置操作");
    }
    return QStringLiteral("未知");
}

QString DeviceConfigPage::displayTime(qint64 timestampMs) const
{
    if (timestampMs <= 0) {
        return QStringLiteral("-");
    }

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

void DeviceConfigPage::setupTable(QTableWidget *table) const
{
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(table->columnCount() - 1, QHeaderView::Stretch);
    table->setAlternatingRowColors(true);
}

void DeviceConfigPage::addStatusItem(QTableWidget *table, int row, int column, const QString &status) const
{
    auto *item = readonlyItem(statusText(status));
    const bool ok = status == QStringLiteral("online") || status == QStringLiteral("connected");
    item->setForeground(ok ? QColor(QStringLiteral("#16A34A")) : QColor(QStringLiteral("#6B7280")));
    table->setItem(row, column, item);
}

void DeviceConfigPage::addDeleteButton(QTableWidget *table, int row, bool master, const QString &gatewayId,
                                       const QString &portId, int deviceId)
{
    auto *button = new QPushButton(QStringLiteral("删除"), table);
    button->setProperty("danger", true);
    bool serviceOffline = false;
    if (m_dataManager) {
        const QList<DeviceNode> devices = m_dataManager->deviceTreeSnapshot();
        for (const DeviceNode &node : devices) {
            if (node.gatewayId != gatewayId || (!portId.isEmpty() && node.port != portId)) {
                continue;
            }
            const int nodeSlaveId = node.slaveAddr > 0 ? node.slaveAddr : node.deviceId;
            if (!master && nodeSlaveId != deviceId) {
                continue;
            }
            if (node.status == QStringLiteral("service_offline")) {
                serviceOffline = true;
                break;
            }
        }
    }
    if (serviceOffline) {
        button->setEnabled(false);
        button->setToolTip(QStringLiteral("服务离线，禁止配置操作"));
    }
    connect(button, &QPushButton::clicked, this, [this, master, gatewayId, portId, deviceId]() {
        const QString target = master
            ? QStringLiteral("该主站及其下所有从站数据")
            : QStringLiteral("该从站设备的所有数据");
        const int ret = QMessageBox::question(
            this,
            QStringLiteral("确认删除"),
            QStringLiteral("确定要删除%1吗？删除后历史、配置、报警和最新数据都会被清除。").arg(target));

        if (ret != QMessageBox::Yes) {
            return;
        }

        if (master) {
            emit deleteMasterDataRequested(gatewayId, portId);
        } else {
            qDebug() << "DeviceConfigPage delete slave clicked"
                     << "gatewayId" << gatewayId
                     << "portId" << portId
                     << "slaveAddr/deviceId" << deviceId;
            emit deleteDeviceDataRequested(gatewayId, portId, deviceId);
        }
    });
    table->setCellWidget(row, table->columnCount() - 1, button);
}

QString DeviceConfigPage::realtimeTemperatureText(const QString &deviceKey) const
{
    if (!m_dataManager) {
        return QStringLiteral("-");
    }
    const RealtimeDeviceData data = m_dataManager->deviceData(deviceKey);
    if (data.points.isEmpty() || data.node.deviceType != QStringLiteral("sensor_th")) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 ℃").arg(data.sensorTh.temperature, 0, 'f', 1);
}

QString DeviceConfigPage::realtimeHumidityText(const QString &deviceKey) const
{
    if (!m_dataManager) {
        return QStringLiteral("-");
    }
    const RealtimeDeviceData data = m_dataManager->deviceData(deviceKey);
    if (data.points.isEmpty() || data.node.deviceType != QStringLiteral("sensor_th")) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 %").arg(data.sensorTh.humidity, 0, 'f', 1);
}

QString DeviceConfigPage::realtimeRelayText(const QString &deviceKey) const
{
    if (!m_dataManager) {
        return QStringLiteral("-");
    }
    const RealtimeDeviceData data = m_dataManager->deviceData(deviceKey);
    if (data.points.isEmpty() || data.node.deviceType != QStringLiteral("relay")) {
        return QStringLiteral("-");
    }
    QStringList channels;
    for (auto it = data.relay.channels.cbegin(); it != data.relay.channels.cend(); ++it) {
        channels << QStringLiteral("%1:%2").arg(it.key(), it.value() ? QStringLiteral("ON") : QStringLiteral("OFF"));
    }
    return channels.isEmpty() ? QStringLiteral("-") : channels.join(QStringLiteral(" "));
}

QString DeviceConfigPage::realtimeSourceText(const QString &deviceKey) const
{
    if (!m_dataManager) {
        return QStringLiteral("设备表");
    }
    const RealtimeDeviceData data = m_dataManager->deviceData(deviceKey);
    return data.points.isEmpty() ? QStringLiteral("设备表") : QStringLiteral("实时缓存");
}

void DeviceConfigPage::showAddSlaveDialog()
{
    const QList<MasterRow> masters = buildMasterRows();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("添加从站"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;

    auto *masterCombo = new QComboBox(&dialog);
    for (const MasterRow &m : masters) {
        if (m.gatewayId.isEmpty() || m.portId.isEmpty()) {
            continue;
        }
        if (m.status != QStringLiteral("connected") && m.status != QStringLiteral("online")) {
            continue;
        }

        const QString gatewayName = m.gatewayName.isEmpty() ? m.gatewayId : m.gatewayName;
        const QString portName = m.portName.isEmpty() ? m.portId : m.portName;
        masterCombo->addItem(QStringLiteral("%1 / %2").arg(gatewayName, portName),
                             masterKey(m.gatewayId, m.portId));
    }

    auto *slaveAddr = new QSpinBox(&dialog);
    slaveAddr->setRange(1, 247);
    slaveAddr->setValue(1);

    auto *deviceType = new QComboBox(&dialog);
    deviceType->addItem(QStringLiteral("温湿度传感器"), QStringLiteral("sensor_th"));
    deviceType->addItem(QStringLiteral("继电器"), QStringLiteral("relay"));

    auto *pollInterval = new QSpinBox(&dialog);
    pollInterval->setRange(100, 600000);
    pollInterval->setSingleStep(100);
    pollInterval->setValue(1000);
    pollInterval->setSuffix(QStringLiteral(" ms"));

    form->addRow(QStringLiteral("主站"), masterCombo);
    form->addRow(QStringLiteral("从站地址"), slaveAddr);
    form->addRow(QStringLiteral("设备类型"), deviceType);
    form->addRow(QStringLiteral("轮询周期"), pollInterval);
    layout->addLayout(form);

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(box);

    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (masterCombo->count() == 0) {
        QMessageBox::information(this,
                                 QStringLiteral("添加从站"),
                                 QStringLiteral("暂无已连接主站，请先确认板端端口已连接并已上报。"));
        return;
    }

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString selected = masterCombo->currentData().toString();
    const int split = selected.indexOf(QStringLiteral("/"));
    if (split <= 0 || split >= selected.size() - 1) {
        QMessageBox::warning(this, QStringLiteral("添加从站"), QStringLiteral("主站信息无效"));
        return;
    }

    emit addSlaveRequested(selected.left(split),
                           selected.mid(split + 1),
                           slaveAddr->value(),
                           deviceType->currentData().toString(),
                           pollInterval->value());
}

void DeviceConfigPage::showSyncConfigDialog()
{
    const QList<DeviceNode> devices = m_dataManager ? m_dataManager->deviceTreeSnapshot() : QList<DeviceNode>();
    if (devices.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("配置同步"),
                                 QStringLiteral("当前没有可选择的设备。"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("选择同步设备"));
    dialog.resize(520, 420);

    auto *layout = new QVBoxLayout(&dialog);
    auto *tip = new QLabel(QStringLiteral("请选择需要从板端同步配置的网关/端口/设备："), &dialog);
    layout->addWidget(tip);

    auto *tree = new QTreeWidget(&dialog);
    tree->setColumnCount(1);
    tree->setHeaderLabel(QStringLiteral("设备"));
    layout->addWidget(tree, 1);

    QMap<QString, QTreeWidgetItem *> gatewayItems;
    QMap<QString, QTreeWidgetItem *> portItems;
    bool updating = false;

    for (const DeviceNode &device : devices) {
        if (device.gatewayId.isEmpty() || device.port.isEmpty() || device.deviceId <= 0) {
            continue;
        }

        QTreeWidgetItem *gatewayItem = gatewayItems.value(device.gatewayId, nullptr);
        if (!gatewayItem) {
            const QString gatewayText = device.gatewayName.isEmpty()
                ? device.gatewayId
                : QStringLiteral("%1 (%2)").arg(device.gatewayName, device.gatewayId);
            gatewayItem = new QTreeWidgetItem(tree, QStringList{gatewayText});
            gatewayItem->setFlags(gatewayItem->flags() | Qt::ItemIsUserCheckable);
            gatewayItem->setCheckState(0, Qt::Unchecked);
            gatewayItem->setData(0, Qt::UserRole, QStringLiteral("gateway"));
            gatewayItems.insert(device.gatewayId, gatewayItem);
        }

        const QString portKey = masterKey(device.gatewayId, device.port);
        QTreeWidgetItem *portItem = portItems.value(portKey, nullptr);
        if (!portItem) {
            const QString portText = QStringLiteral("%1 (%2)").arg(defaultPortName(device), device.port);
            portItem = new QTreeWidgetItem(gatewayItem, QStringList{portText});
            portItem->setFlags(portItem->flags() | Qt::ItemIsUserCheckable);
            portItem->setCheckState(0, Qt::Unchecked);
            portItem->setData(0, Qt::UserRole, QStringLiteral("port"));
            portItem->setData(0, Qt::UserRole + 1, device.gatewayId);
            portItem->setData(0, Qt::UserRole + 2, device.port);
            portItems.insert(portKey, portItem);
        }

        const QString deviceText = QStringLiteral("%1  地址:%2  类型:%3")
            .arg(device.deviceName.isEmpty() ? QStringLiteral("Device %1").arg(device.deviceId) : device.deviceName)
            .arg(device.deviceId)
            .arg(device.deviceType);
        auto *deviceItem = new QTreeWidgetItem(portItem, QStringList{deviceText});
        deviceItem->setFlags(deviceItem->flags() | Qt::ItemIsUserCheckable);
        deviceItem->setCheckState(0, Qt::Unchecked);
        deviceItem->setData(0, Qt::UserRole, QStringLiteral("device"));
        deviceItem->setData(0, Qt::UserRole + 1, device.gatewayId);
        deviceItem->setData(0, Qt::UserRole + 2, device.port);
        deviceItem->setData(0, Qt::UserRole + 3, device.deviceId);
    }

    connect(tree, &QTreeWidget::itemChanged, &dialog, [&](QTreeWidgetItem *item, int column) {
        if (updating || column != 0) {
            return;
        }

        updating = true;
        const Qt::CheckState state = item->checkState(0);
        for (int i = 0; i < item->childCount(); ++i) {
            item->child(i)->setCheckState(0, state);
            for (int j = 0; j < item->child(i)->childCount(); ++j) {
                item->child(i)->child(j)->setCheckState(0, state);
            }
        }

        QTreeWidgetItem *parent = item->parent();
        while (parent) {
            int checked = 0;
            int partial = 0;
            for (int i = 0; i < parent->childCount(); ++i) {
                const Qt::CheckState childState = parent->child(i)->checkState(0);
                if (childState == Qt::Checked) ++checked;
                if (childState == Qt::PartiallyChecked) ++partial;
            }
            if (checked == parent->childCount()) {
                parent->setCheckState(0, Qt::Checked);
            } else if (checked > 0 || partial > 0) {
                parent->setCheckState(0, Qt::PartiallyChecked);
            } else {
                parent->setCheckState(0, Qt::Unchecked);
            }
            parent = parent->parent();
        }
        updating = false;
    });

    auto *buttonRow = new QHBoxLayout;
    auto *selectAll = new QPushButton(QStringLiteral("全部选中"), &dialog);
    auto *clearAll = new QPushButton(QStringLiteral("清空选择"), &dialog);
    buttonRow->addWidget(selectAll);
    buttonRow->addWidget(clearAll);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    connect(selectAll, &QPushButton::clicked, &dialog, [&]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            tree->topLevelItem(i)->setCheckState(0, Qt::Checked);
        }
    });
    connect(clearAll, &QPushButton::clicked, &dialog, [&]() {
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            tree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
        }
    });

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(box);
    connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    tree->expandAll();
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QMap<QString, QJsonArray> devicesByGateway;
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        QTreeWidgetItem *gatewayItem = tree->topLevelItem(g);
        for (int p = 0; p < gatewayItem->childCount(); ++p) {
            QTreeWidgetItem *portItem = gatewayItem->child(p);
            for (int d = 0; d < portItem->childCount(); ++d) {
                QTreeWidgetItem *deviceItem = portItem->child(d);
                if (deviceItem->checkState(0) != Qt::Checked) {
                    continue;
                }

                const QString gatewayId = deviceItem->data(0, Qt::UserRole + 1).toString();
                const QString portId = deviceItem->data(0, Qt::UserRole + 2).toString();
                const int deviceId = deviceItem->data(0, Qt::UserRole + 3).toInt();
                if (gatewayId.isEmpty() || portId.isEmpty() || deviceId <= 0) {
                    continue;
                }

                QJsonObject deviceObj;
                deviceObj.insert(QStringLiteral("portId"), portId);
                deviceObj.insert(QStringLiteral("deviceId"), deviceId);
                QJsonArray array = devicesByGateway.value(gatewayId);
                array.append(deviceObj);
                devicesByGateway.insert(gatewayId, array);
            }
        }
    }

    QJsonArray targets;
    for (auto it = devicesByGateway.cbegin(); it != devicesByGateway.cend(); ++it) {
        QJsonObject target;
        target.insert(QStringLiteral("gatewayId"), it.key());
        target.insert(QStringLiteral("devices"), it.value());
        targets.append(target);
    }

    if (targets.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("配置同步"), QStringLiteral("请至少选择一个设备。"));
        return;
    }

    if (m_syncButton) {
        m_syncButton->setEnabled(false);
        m_syncButton->setText(QStringLiteral("同步中..."));
    }
    emit syncConfigRequested(targets);
}

void DeviceConfigPage::onSyncConfigResult(const QJsonObject &root)
{
    if (m_syncButton) {
        m_syncButton->setEnabled(true);
        m_syncButton->setText(QStringLiteral("配置同步"));
    }

    const bool success = root.value(QStringLiteral("success")).toBool(false);
    const int portCount = root.value(QStringLiteral("portCount")).toInt();
    const int deviceCount = root.value(QStringLiteral("deviceCount")).toInt();
    const QString message = root.value(QStringLiteral("message")).toString();

    if (success) {
        QMessageBox::information(this,
                                 QStringLiteral("配置同步"),
                                 QStringLiteral("同步成功：已同步 %1 个端口、%2 个设备")
                                     .arg(portCount)
                                     .arg(deviceCount));
    } else {
        QMessageBox::warning(this,
                             QStringLiteral("配置同步"),
                             message.isEmpty() ? QStringLiteral("同步失败") : message);
    }
}

void DeviceConfigPage::showScanPlaceholder()
{
    QMessageBox::information(this, QStringLiteral("扫描"), QStringLiteral("扫描功能待实现"));
}
