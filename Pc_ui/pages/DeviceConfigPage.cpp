#include "DeviceConfigPage.h"

#include "core/ConfigManager.h"
#include "core/DeviceManager.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
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
    auto *scanSlave = new QPushButton(QStringLiteral("扫描从站"), this);
    slaveHeader->addWidget(slaveTitle);
    slaveHeader->addStretch();
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
    const QList<MasterRow> masters = buildMasterRows(devices);

    m_masterTable->setRowCount(masters.size());
    for (int row = 0; row < masters.size(); ++row) {
        const MasterRow &m = masters.at(row);
        const QString portName = m.portName.isEmpty()
            ? QStringLiteral("RS485-%1").arg(m.masterSlot + 1)
            : m.portName;

        m_masterTable->setItem(row, 0, readonlyItem(portName));
        m_masterTable->setItem(row, 1, readonlyItem(m.gatewayName.isEmpty() ? m.gatewayId : m.gatewayName));
        m_masterTable->setItem(row, 2, readonlyItem(m.portId));
        m_masterTable->setItem(row, 3, readonlyItem(QString::number(m.baud)));
        m_masterTable->setItem(row, 4, readonlyItem(m.areaName));
        m_masterTable->setItem(row, 5, readonlyItem(QString::number(m.slaveCount)));
        m_masterTable->setItem(row, 6, readonlyItem(displayTime(m.lastUpdateTime)));
        addStatusItem(m_masterTable, row, 7, m.online);
        m_masterTable->setItem(row, 8, readonlyItem(QStringLiteral("设备表")));
        addDeleteButton(m_masterTable, row, true, m.gatewayId, m.portId);
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
        addStatusItem(m_slaveTable, row, 7, d.online);
        m_slaveTable->setItem(row, 8, readonlyItem(QStringLiteral("设备表")));
        addDeleteButton(m_slaveTable, row, false, d.gatewayId, d.port, d.deviceId);
    }
}

QList<DeviceConfigPage::MasterRow> DeviceConfigPage::buildMasterRows(const QList<DeviceNode> &devices) const
{
    QMap<QString, MasterRow> rows;

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
        }

        if (row.gatewayName.isEmpty()) row.gatewayName = d.gatewayName;
        if (row.areaName.isEmpty()) row.areaName = d.areaName;
        if (row.portName.isEmpty()) row.portName = defaultPortName(d);
        if (d.lastUpdateTime > row.lastUpdateTime) row.lastUpdateTime = d.lastUpdateTime;
        row.online = row.online || d.online;
        ++row.slaveCount;

        rows.insert(key, row);
    }

    return rows.values();
}

QString DeviceConfigPage::statusText(bool online) const
{
    return online ? QStringLiteral("在线") : QStringLiteral("离线");
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

void DeviceConfigPage::addStatusItem(QTableWidget *table, int row, int column, bool online) const
{
    auto *item = readonlyItem(statusText(online));
    item->setForeground(online ? QColor(QStringLiteral("#16A34A")) : QColor(QStringLiteral("#6B7280")));
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

void DeviceConfigPage::showScanPlaceholder()
{
    QMessageBox::information(this, QStringLiteral("扫描"), QStringLiteral("扫描功能待实现"));
}
