#include "DashboardPage.h"
#include "ui/StatusCard.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/AlarmManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTabWidget>
#include <QHeaderView>
#include <QTimer>
#include <QLabel>
#include <QHash>
#include <QSet>

DashboardPage::DashboardPage(DataManager *data, DeviceManager *device, AlarmManager *alarm, QWidget *parent)
    : QWidget(parent), m_data(data), m_device(device), m_alarm(alarm)
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

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &DashboardPage::refreshView);
    m_timer->start(1000);
    refreshView();
}

void DashboardPage::refreshView()
{
    const auto devices = m_device->allDevices();
    QSet<QString> gateways, masters;
    QSet<QString> onlineMasters;
    QHash<QString, int> masterDeviceCounts;
    int onlineSlaveCount = 0;
    for (const auto &d : devices) {
        const QString masterKey = d.gatewayId + "/" + QString::number(d.masterSlot);
        if (d.online) {
            gateways.insert(d.gatewayId);
            onlineMasters.insert(masterKey);
            ++onlineSlaveCount;
        }
        masters.insert(masterKey);
        masterDeviceCounts[masterKey] += 1;
    }
    m_gatewayCard->setValue(QString::number(gateways.size()));
    m_masterCard->setValue(QString::number(onlineMasters.size()));
    m_slaveCard->setValue(QString::number(onlineSlaveCount));
    m_alarmCard->setValue(QString::number(m_alarm->activeAlarmCount()));

    m_masterTable->setRowCount(masters.size());
    int row = 0;
    for (const auto &m : masters) {
        m_masterTable->setItem(row, 0, new QTableWidgetItem(m.section('/', 0, 0)));
        m_masterTable->setItem(row, 1, new QTableWidgetItem("RS485-" + QString::number(m.section('/', 1, 1).toInt() + 1)));
        m_masterTable->setItem(row, 2, new QTableWidgetItem(onlineMasters.contains(m) ? QStringLiteral("在线") : QStringLiteral("离线")));
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
        if (!device.valid) {
            ++errorCount;
        }
    }

    m_errorTable->setRowCount(errorCount);
    row = 0;
    for (const auto &device : realtimeDevices) {
        if (device.valid) {
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
