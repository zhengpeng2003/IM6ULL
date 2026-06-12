#include "DeviceConfigPage.h"

#include "core/ConfigManager.h"
#include "core/DeviceManager.h"

#include <QAbstractItemView>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {

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

DeviceConfigPage::DeviceConfigPage(DeviceManager *device, ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_device(device), m_config(config)
{
    Q_UNUSED(m_config);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("设备配置"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *masterHeader = new QHBoxLayout;
    auto *masterTitle = new QLabel(QStringLiteral("主站配置"), this);
    masterTitle->setObjectName("PageTitle");
    auto *scanMaster = new QPushButton(QStringLiteral("扫描主站"), this);
    masterHeader->addWidget(masterTitle);
    masterHeader->addStretch();
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
    slaveHeader->addWidget(slaveTitle);
    slaveHeader->addStretch();
    slaveHeader->addWidget(addSlave);
    slaveHeader->addWidget(scanSlave);
    layout->addLayout(slaveHeader);

    m_slaveTable = new QTableWidget(this);
    m_slaveTable->setColumnCount(10);
    m_slaveTable->setHorizontalHeaderLabels({
        QStringLiteral("主站"),
        QStringLiteral("地址"),
        QStringLiteral("类型"),
        QStringLiteral("名称"),
        QStringLiteral("区域"),
        QStringLiteral("网关"),
        QStringLiteral("最后更新"),
        QStringLiteral("状态"),
        QStringLiteral("来源"),
        QStringLiteral("操作")
    });
    setupTable(m_slaveTable);
    layout->addWidget(m_slaveTable, 1);

    connect(scanMaster, &QPushButton::clicked, this, &DeviceConfigPage::showScanPlaceholder);
    connect(addSlave, &QPushButton::clicked, this, &DeviceConfigPage::showAddSlaveDialog);
    connect(scanSlave, &QPushButton::clicked, this, &DeviceConfigPage::showScanPlaceholder);

    if (m_device) {
        connect(m_device, &DeviceManager::deviceConfigChanged,
                this, &DeviceConfigPage::refreshTables);
        connect(m_device, &DeviceManager::deviceOnlineStateChanged,
                this, &DeviceConfigPage::refreshTables);
    }

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &DeviceConfigPage::refreshTables);
    m_refreshTimer->start(1000);

    refreshTables();
}

void DeviceConfigPage::refreshTables()
{
    const QList<DeviceNode> devices = m_device ? m_device->allDevices() : QList<DeviceNode>();
    const QList<MasterRow> masters = buildMasterRows();

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
        m_slaveTable->setItem(row, 6, readonlyItem(displayTime(d.lastUpdateTime)));
        addStatusItem(m_slaveTable, row, 7, d.online ? QStringLiteral("online") : d.status);
        m_slaveTable->setItem(row, 8, readonlyItem(QStringLiteral("设备表")));
        addDeleteButton(m_slaveTable, row, false, d.gatewayId, d.port, d.deviceId);
    }
}

QList<DeviceConfigPage::MasterRow> DeviceConfigPage::buildMasterRows() const
{
    QMap<QString, MasterRow> rows;
    const QList<DeviceNode> devices = m_device ? m_device->allDevices() : QList<DeviceNode>();
    const QList<GatewayNode> gateways = m_device ? m_device->allGateways() : QList<GatewayNode>();
    const QList<PortNode> ports = m_device ? m_device->allPorts() : QList<PortNode>();
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
            emit deleteDeviceDataRequested(gatewayId, portId, deviceId);
        }
    });
    table->setCellWidget(row, table->columnCount() - 1, button);
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

void DeviceConfigPage::showScanPlaceholder()
{
    QMessageBox::information(this, QStringLiteral("扫描"), QStringLiteral("扫描功能待实现"));
}
