#include "DashboardPage.h"

#include "core/AlarmManager.h"
#include "core/DataManager.h"
#include "core/DeviceManager.h"
#include "core/UiStateStore.h"
#include "ui/StatusCard.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHash>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSet>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace {

constexpr int kRefreshDelayMs = 250;
constexpr int kTrendPointLimit = 24;

QTableWidgetItem *readonlyItem(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void setupDashboardTable(QTableWidget *table)
{
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
}

QFrame *createDashboardCard(const QString &title, QWidget *content, QWidget *parent)
{
    auto *card = new QFrame(parent);
    card->setObjectName("DashboardCard");

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("DashboardCardTitle");
    layout->addWidget(titleLabel);
    layout->addWidget(content, 1);

    return card;
}

} // namespace

DashboardPage::DashboardPage(DataManager *data, DeviceManager *device, AlarmManager *alarm,
                             UiStateStore *stateStore, QWidget *parent)
    : QWidget(parent),
    m_data(data),
    m_device(device),
    m_alarm(alarm),
    m_stateStore(stateStore)
{
    setObjectName("DashboardPage");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(16);

    auto *title = new QLabel(QStringLiteral("首页总览"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *cards = new QHBoxLayout;
    cards->setSpacing(12);

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
    tables->setSpacing(14);

    m_masterTable = new QTableWidget(this);
    m_masterTable->setColumnCount(4);
    m_masterTable->setHorizontalHeaderLabels({
        QStringLiteral("网关"),
        QStringLiteral("主站"),
        QStringLiteral("在线"),
        QStringLiteral("设备数")
    });
    setupDashboardTable(m_masterTable);

    m_alarmTable = new QTableWidget(this);
    m_alarmTable->setColumnCount(4);
    m_alarmTable->setHorizontalHeaderLabels({
        QStringLiteral("报警ID"),
        QStringLiteral("设备"),
        QStringLiteral("等级"),
        QStringLiteral("状态")
    });
    setupDashboardTable(m_alarmTable);

    m_errorTable = new QTableWidget(this);
    m_errorTable->setColumnCount(5);
    m_errorTable->setHorizontalHeaderLabels({
        QStringLiteral("设备"),
        QStringLiteral("主站"),
        QStringLiteral("从站"),
        QStringLiteral("错误状态"),
        QStringLiteral("错误原因")
    });
    setupDashboardTable(m_errorTable);

    auto *alarmErrorTabs = new QTabWidget(this);
    alarmErrorTabs->setObjectName("DashboardAlarmErrorTabs");
    alarmErrorTabs->setDocumentMode(true);
    alarmErrorTabs->addTab(m_alarmTable, QStringLiteral("最新告警"));
    alarmErrorTabs->addTab(m_errorTable, QStringLiteral("异常设备"));

    tables->addWidget(createDashboardCard(QStringLiteral("主站运行状态"), m_masterTable, this), 1);
    tables->addWidget(createDashboardCard(QStringLiteral("报警与异常"), alarmErrorTabs, this), 1);

    layout->addLayout(tables, 2);

    m_trendSeries = new QLineSeries(this);
    m_trendSeries->setName(QStringLiteral("实时测点"));

    m_trendAxisX = new QValueAxis(this);
    m_trendAxisX->setRange(0, 1);
    m_trendAxisX->setTickCount(6);
    m_trendAxisX->setLabelFormat(QStringLiteral("%.0f"));

    m_trendAxisY = new QValueAxis(this);
    m_trendAxisY->setRange(0.0, 1.0);
    m_trendAxisY->setTickCount(5);
    m_trendAxisY->setLabelFormat(QStringLiteral("%.2f"));

    auto *chart = new QChart();
    chart->addSeries(m_trendSeries);
    chart->addAxis(m_trendAxisX, Qt::AlignBottom);
    chart->addAxis(m_trendAxisY, Qt::AlignLeft);

    m_trendSeries->attachAxis(m_trendAxisX);
    m_trendSeries->attachAxis(m_trendAxisY);

    chart->legend()->hide();
    chart->setBackgroundVisible(false);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->setTitle(QString());

    auto *trendView = new QChartView(chart, this);
    trendView->setObjectName("DashboardChart");
    trendView->setRenderHint(QPainter::Antialiasing);

    auto *trendContent = new QWidget(this);
    auto *trendLayout = new QVBoxLayout(trendContent);
    trendLayout->setContentsMargins(0, 0, 0, 0);
    trendLayout->setSpacing(8);

    m_trendHintLabel = new QLabel(QStringLiteral("实时测点快照"), trendContent);
    m_trendHintLabel->setObjectName("DashboardTrendHint");

    trendLayout->addWidget(m_trendHintLabel);
    trendLayout->addWidget(trendView, 1);

    layout->addWidget(createDashboardCard(QStringLiteral("实时趋势"), trendContent, this), 2);

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setSingleShot(true);

    connect(m_refreshTimer, &QTimer::timeout,
            this, &DashboardPage::refreshView);

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
    m_alarmCard->setValue(QString::number(m_alarm ? m_alarm->activeAlarmCount() : 0));

    m_masterTable->setRowCount(masters.size());
    int row = 0;
    for (const auto &m : masters) {
        m_masterTable->setItem(row, 0, readonlyItem(m.section('/', 0, 0)));
        m_masterTable->setItem(row, 1, readonlyItem("RS485-" + QString::number(m.section('/', 1, 1).toInt() + 1)));
        m_masterTable->setItem(row, 2, readonlyItem(serviceOnline ? (onlineMasters.contains(m) ? QStringLiteral("在线") : QStringLiteral("离线")) : QStringLiteral("Pc_data 服务离线")));
        m_masterTable->setItem(row, 3, readonlyItem(QString::number(masterDeviceCounts.value(m))));
        ++row;
    }

    const auto alarms = m_alarm ? m_alarm->latestAlarms(10) : QList<AlarmRecord>();
    m_alarmTable->setRowCount(alarms.size());
    for (int i = 0; i < alarms.size(); ++i) {
        m_alarmTable->setItem(i, 0, readonlyItem(alarms[i].alarmId));
        m_alarmTable->setItem(i, 1, readonlyItem(alarms[i].deviceName));
        m_alarmTable->setItem(i, 2, readonlyItem(alarms[i].level));
        m_alarmTable->setItem(i, 3, readonlyItem(alarms[i].state));
    }

    const auto realtimeDevices = m_data ? m_data->allRealtimeData() : QList<RealtimeDeviceData>();
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

        m_errorTable->setItem(row, 0, readonlyItem(device.node.deviceName));
        m_errorTable->setItem(row, 1, readonlyItem(QStringLiteral("RS485-%1").arg(device.node.masterSlot + 1)));
        m_errorTable->setItem(row, 2, readonlyItem(QString::number(device.node.slaveAddr)));
        m_errorTable->setItem(row, 3, readonlyItem(statusText));
        m_errorTable->setItem(row, 4, readonlyItem(errorMessage));
        ++row;
    }

    refreshTrendChart();
}

void DashboardPage::refreshTrendChart()
{
    if (!m_trendSeries || !m_trendAxisX || !m_trendAxisY) {
        return;
    }

    m_trendSeries->clear();
    const auto realtimeDevices = m_data ? m_data->allRealtimeData() : QList<RealtimeDeviceData>();

    double minValue = 0.0;
    double maxValue = 0.0;
    int pointIndex = 0;

    for (const auto &device : realtimeDevices) {
        if (pointIndex >= kTrendPointLimit) {
            break;
        }
        if (!device.valid || device.serviceOffline || device.mock) {
            continue;
        }

        for (const auto &point : device.points) {
            if (pointIndex >= kTrendPointLimit) {
                break;
            }
            if (!point.valid || point.valueType == QStringLiteral("text")) {
                continue;
            }

            const double value = point.numberValue;
            m_trendSeries->append(pointIndex + 1, value);
            minValue = pointIndex == 0 ? value : qMin(minValue, value);
            maxValue = pointIndex == 0 ? value : qMax(maxValue, value);
            ++pointIndex;
        }
    }

    if (pointIndex == 0) {
        m_trendAxisX->setRange(0, 1);
        m_trendAxisY->setRange(0.0, 1.0);
        if (m_trendHintLabel) {
            m_trendHintLabel->setText(QStringLiteral("暂无有效实时测点"));
        }
        return;
    }

    const double span = maxValue - minValue;
    const double padding = span > 0.0 ? span * 0.12 : qMax(qAbs(maxValue) * 0.12, 1.0);
    m_trendAxisX->setRange(1, qMax(2, pointIndex));
    m_trendAxisY->setRange(minValue - padding, maxValue + padding);
    if (m_trendHintLabel) {
        m_trendHintLabel->setText(QStringLiteral("实时测点快照：%1 个点").arg(pointIndex));
    }
}
