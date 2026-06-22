#include "AlarmLogPage.h"
#include "ui/AlarmTableWidget.h"
#include "core/AlarmManager.h"
#include <algorithm>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

namespace {

qint64 normalizedMs(qint64 timestamp)
{
    if (timestamp <= 0) {
        return 0;
    }

    return timestamp > 100000000000LL ? timestamp : timestamp * 1000;
}

QLabel *filterLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("AlarmFilterLabel"));
    return label;
}

} // namespace

AlarmLogPage::AlarmLogPage(AlarmManager *alarm, QWidget *parent)
    : QWidget(parent), m_alarm(alarm)
{
    setObjectName("AlarmLogPage");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 18);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("报警日志"), this);
    title->setObjectName("PageTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("AlarmLogPanel"));
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(12, 12, 12, 22);
    panelLayout->setSpacing(16);

    auto *filterPanel = new QWidget(panel);
    filterPanel->setObjectName("FilterPanel");
    auto *tools = new QHBoxLayout(filterPanel);
    tools->setContentsMargins(12, 10, 12, 10);
    tools->setSpacing(12);

    m_levelCombo = new QComboBox(this);
    m_levelCombo->addItem(QStringLiteral("全部"), QString());
    m_levelCombo->addItem(QStringLiteral("严重"), QStringLiteral("critical"));
    m_levelCombo->addItem(QStringLiteral("高"), QStringLiteral("high"));
    m_levelCombo->addItem(QStringLiteral("中"), QStringLiteral("medium"));
    m_levelCombo->addItem(QStringLiteral("低"), QStringLiteral("low"));
    m_levelCombo->setMinimumWidth(110);

    m_stateCombo = new QComboBox(this);
    m_stateCombo->addItem(QStringLiteral("全部"), QString());
    m_stateCombo->addItem(QStringLiteral("未确认"), QStringLiteral("active"));
    m_stateCombo->addItem(QStringLiteral("已确认"), QStringLiteral("acknowledged"));
    m_stateCombo->addItem(QStringLiteral("已恢复"), QStringLiteral("recovered"));
    m_stateCombo->setMinimumWidth(110);

    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(QStringLiteral("今天"), QStringLiteral("today"));
    m_rangeCombo->addItem(QStringLiteral("最近24小时"), QStringLiteral("last24h"));
    m_rangeCombo->addItem(QStringLiteral("全部"), QStringLiteral("all"));
    m_rangeCombo->setMinimumWidth(110);

    auto *queryButton = new QPushButton(QStringLiteral("查询"), this);
    queryButton->setObjectName(QStringLiteral("AlarmPrimaryButton"));
    auto *exportButton = new QPushButton(QStringLiteral("导出"), this);
    exportButton->setObjectName(QStringLiteral("AlarmSecondaryButton"));
    exportButton->setEnabled(false);
    exportButton->setToolTip(QStringLiteral("导出功能暂未接入"));

    tools->addWidget(filterLabel(QStringLiteral("报警等级:"), this));
    tools->addWidget(m_levelCombo);
    tools->addSpacing(12);
    tools->addWidget(filterLabel(QStringLiteral("报警状态:"), this));
    tools->addWidget(m_stateCombo);
    tools->addSpacing(12);
    tools->addWidget(filterLabel(QStringLiteral("时间范围:"), this));
    tools->addWidget(m_rangeCombo);
    tools->addWidget(queryButton);
    tools->addWidget(exportButton);
    tools->addStretch();
    panelLayout->addWidget(filterPanel);

    m_table = new AlarmTableWidget(this);
    panelLayout->addWidget(m_table, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto *ackAllButton = new QPushButton(QStringLiteral("确认告警"), this);
    ackAllButton->setObjectName(QStringLiteral("AlarmDangerOutlineButton"));
    auto *clearButton = new QPushButton(QStringLiteral("清空已恢复告警"), this);
    clearButton->setObjectName(QStringLiteral("AlarmSecondaryButton"));
    actions->addWidget(ackAllButton);
    actions->addWidget(clearButton);
    actions->addStretch();
    panelLayout->addLayout(actions);

    layout->addWidget(panel, 1);

    if (m_alarm) {
        connect(m_alarm, &AlarmManager::alarmsChanged, this, &AlarmLogPage::refreshTable);
        connect(m_table, &AlarmTableWidget::acknowledgeRequested,
                m_alarm, &AlarmManager::acknowledgeAlarm);
    }
    connect(queryButton, &QPushButton::clicked, this, &AlarmLogPage::refreshTable);
    connect(ackAllButton, &QPushButton::clicked, this, &AlarmLogPage::acknowledgeVisibleActiveAlarms);
    connect(clearButton, &QPushButton::clicked, this, &AlarmLogPage::clearRecoveredAlarms);
    refreshTable();
}

void AlarmLogPage::refreshTable()
{
    if (!m_alarm || !m_table) {
        return;
    }

    m_table->setAlarms(filteredAlarms());
}

void AlarmLogPage::acknowledgeVisibleActiveAlarms()
{
    if (!m_alarm) {
        return;
    }

    const QList<AlarmRecord> alarms = filteredAlarms();
    for (const AlarmRecord &alarm : alarms) {
        if (alarm.state == "active") {
            m_alarm->acknowledgeAlarm(alarm.alarmId);
        }
    }

    refreshTable();
}

void AlarmLogPage::clearRecoveredAlarms()
{
    emit clearRecoveredAlarmsRequested();
}

QList<AlarmRecord> AlarmLogPage::filteredAlarms() const
{
    QList<AlarmRecord> result;
    if (!m_alarm) {
        return result;
    }

    const QList<AlarmRecord> alarms = m_alarm->alarms();
    for (const AlarmRecord &alarm : alarms) {
        if (matchesLevel(alarm) && matchesState(alarm) && matchesTimeRange(alarm)) {
            result.append(alarm);
        }
    }

    std::sort(result.begin(), result.end(), [](const AlarmRecord &left, const AlarmRecord &right) {
        return normalizedMs(left.startTime) > normalizedMs(right.startTime);
    });
    return result;
}

bool AlarmLogPage::matchesLevel(const AlarmRecord &alarm) const
{
    const QString selected = m_levelCombo ? m_levelCombo->currentData().toString() : QString();
    if (selected.isEmpty()) {
        return true;
    }

    return alarm.level == selected || alarm.level == m_levelCombo->currentText();
}

bool AlarmLogPage::matchesState(const AlarmRecord &alarm) const
{
    const QString selected = m_stateCombo ? m_stateCombo->currentData().toString() : QString();
    return selected.isEmpty() || alarm.state == selected;
}

bool AlarmLogPage::matchesTimeRange(const AlarmRecord &alarm) const
{
    const QString range = m_rangeCombo ? m_rangeCombo->currentData().toString() : QStringLiteral("today");
    if (range == "all") {
        return true;
    }

    const qint64 alarmMs = normalizedMs(alarm.startTime);
    if (alarmMs <= 0) {
        return false;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (range == "last24h") {
        return alarmMs >= now - 24LL * 60LL * 60LL * 1000LL && alarmMs <= now;
    }

    const qint64 todayStart = QDateTime(QDate::currentDate(), QTime(0, 0)).toMSecsSinceEpoch();
    return alarmMs >= todayStart && alarmMs <= now;
}
