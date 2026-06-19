#include "DashboardPage.h"
#include "ui/StatusCard.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/AlarmManager.h"
#include "core/UiStateStore.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTabWidget>
#include <QHeaderView>
#include <QLabel>
#include <QShowEvent>
#include <QHash>
#include <QSet>
#include <QTimer>

namespace {

constexpr int kRefreshDelayMs = 250;

} // namespace

DashboardPage::DashboardPage(DataManager *data, DeviceManager *device, AlarmManager *alarm,
                             UiStateStore *stateStore, QWidget *parent)
    : QWidget(parent), m_data(data), m_device(device), m_alarm(alarm), m_stateStore(stateStore)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("首页总览"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *cards = new QHBoxLayout;
    m_gatewayCard = new StatusCard(QStringLiteral("在线网关"), this);
    m_masterCard = new StatusCard(QStringLiteral("在线主站"), this);
    m_slaveCard = new StatusCard(QStringLiteral("在线从站"), this);
    m_alarmCard = new StatusCard(QStringLiteral("当前报警"), this);
    cards->addWidget(m_gatewayCard);
    cards->addWidget(m_masterCard);
    cards->addWidget(m_slaveCard);
    cards->addWidget(m_alarmCard);
    layout->addLayout(cards);

    auto *tables = new QHBoxLayout;
    m_masterTable = new QTableWidget(this);
    m_masterTable->setColumnCount(4);
    m_masterTable->setHorizontalHeaderLabels({QStringLiteral("网关"), QStringLiteral("主站"), QStringLiteral("在线"), QStringLiteral("设备数")});
    m_masterTable->horizontalHeader()->setStretchLastSection(true);

    m_alarmTable = new QTableWidget(this);
    m_alarmTable->setColumnCount(4);
    m_alarmTable->setHorizontalHeaderLabels({QStringLiteral("报警ID"), QStringLiteral("设备"), QStringLiteral("等级"), QStringLiteral("状态")});
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);

    m_errorTable = new QTableWidget(this);
    m_errorTable->setColumnCount(5);
    m_errorTable->setHorizontalHeaderLabels({QStringLiteral("设备"), QStringLiteral("主站"), QStringLiteral("从站"),
                                             QStringLiteral("错误状态"), QStringLiteral("错误原因")});
    m_errorTable->horizontalHeader()->setStretchLastSection(true);

    m_infoTabWidget = new QTabWidget(this);
    m_infoTabWidget->addTab(m_alarmTable, QStringLiteral("报警信息"));
    m_infoTabWidget->addTab(m_errorTable, QStringLiteral("错误信息"));

    tables->addWidget(m_masterTable, 1);
    tables->addWidget(m_infoTabWidget, 1);
    layout->addLayout(tables, 1);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);
    connect(m_refreshTimer, &QTimer::timeout, this, &DashboardPage::refreshView);

    if (m_stateStore) {
        connect(m_stateStore, &UiStateStore::stateChanged,
                this, &DashboardPage::scheduleRefreshView);
    }
    refreshView();
}

void DashboardPage::scheduleRefreshView()
{
    m_refreshDirty = true;
    if (!isVisible()) {
        return;
    }

    if (m_refreshTimer && !m_refreshTimer->isActive()) {
        m_refreshTimer->start(kRefreshDelayMs);
    }
}

void DashboardPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_refreshDirty) {
        scheduleRefreshView();
    }
}

void DashboardPage::refreshView()
{
    m_refreshDirty = false;
    const auto devices = m_data ? m_data->deviceTreeSnapshot() : QList<DeviceNode>();
    const bool serviceOnline = m_data ? m_data->isServiceOnline() : false;
    QSet<QString> gateways, masters;
    QSet<QString> onlineMasters;
    QHash<QString, int> masterDeviceCounts;
    int onlineSlaveCount = 0;
    for (const DeviceNode &d : devices) {
        const QString masterKey = d.gatewayId + "/" + QString::number(d.masterSlot);
        if (d.online || d.status == QStringLiteral("online")) {
            gateways.insert(d.gatewayId);
            onlineMasters.insert(masterKey);
            ++onlineSlaveCount;
        }
        masters.insert(masterKey);
        masterDeviceCounts[masterKey] += 1;
    }
    m_gatewayCard->setValue(serviceOnline ? QString::number(gateways.size()) : QStringLiteral("服务离线"));
    m_masterCard->setValue(QString::number(onlineMasters.size()));
    m_slaveCard->setValue(QString::number(onlineSlaveCount));
    m_alarmCard->setValue(QString::number(m_alarm->activeAlarmCount()));

    m_masterTable->setRowCount(masters.size());
    int row = 0;
    for (const auto &m : masters) {
        m_masterTable->setItem(row, 0, new QTableWidgetItem(m.section('/', 0, 0)));
        m_masterTable->setItem(row, 1, new QTableWidgetItem("RS485-" + QString::number(m.section('/', 1, 1).toInt() + 1)));
        m_masterTable->setItem(row, 2, new QTableWidgetItem(serviceOnline ? (onlineMasters.contains(m) ? QStringLiteral("在线") : QStringLiteral("离线")) : QStringLiteral("Pc_data 服务离线")));
        m_masterTable->setItem(row, 3, new QTableWidgetItem(QString::number(masterDeviceCounts.value(m))));
        ++row;
    }

    const auto alarms = m_alarm->latestAlarms(10);
    m_alarmTable->setRowCount(alarms.size());
    for (int i = 0; i < alarms.size(); ++i) {
        m_alarmTable->setItem(i, 0, new QTableWidgetItem(alarms[i].alarmId));
        m_alarmTable->setItem(i, 1, new QTableWidgetItem(alarms[i].deviceName));
        m_alarmTable->setItem(i, 2, new QTableWidgetItem(alarms[i].level));
        m_alarmTable->setItem(i, 3, new QTableWidgetItem(alarms[i].state));
    }

    const auto realtimeDevices = m_data->allRealtimeData();
    int errorCount = 0;
    for (const auto &device : realtimeDevices) {
        if (!device.valid || device.dataState == QStringLiteral("stale") ||
            device.dataState == QStringLiteral("offline") || device.serviceOffline || device.mock) {
            ++errorCount;
        }
    }

    m_errorTable->setRowCount(errorCount);
    row = 0;
    for (const auto &device : realtimeDevices) {
        if (device.valid && device.dataState != QStringLiteral("stale") &&
            device.dataState != QStringLiteral("offline") && !device.serviceOffline && !device.mock) {
            continue;
        }

        const QString statusText = device.statusText.isEmpty()
            ? QStringLiteral("数据无效")
            : device.statusText;
        const QString errorMessage = device.errorMessage.isEmpty()
            ? QStringLiteral("unknown")
            : device.errorMessage;

        m_errorTable->setItem(row, 0, new QTableWidgetItem(device.node.deviceName));
        m_errorTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("RS485-%1").arg(device.node.masterSlot + 1)));
        m_errorTable->setItem(row, 2, new QTableWidgetItem(QString::number(device.node.slaveAddr)));
        m_errorTable->setItem(row, 3, new QTableWidgetItem(statusText));
        m_errorTable->setItem(row, 4, new QTableWidgetItem(errorMessage));
        ++row;
    }
}
