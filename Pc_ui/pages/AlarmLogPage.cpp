#include "AlarmLogPage.h"
#include "ui/AlarmTableWidget.h"
#include "core/AlarmManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>

AlarmLogPage::AlarmLogPage(AlarmManager *alarm, DatabaseManager *database, QWidget *parent)
    : QWidget(parent), m_alarm(alarm), m_database(database)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("报警日志"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *tools = new QHBoxLayout;
    auto *state = new QComboBox(this);
    state->addItems({QStringLiteral("全部"), "active", "acknowledged", "recovered"});
    tools->addWidget(state);
    tools->addWidget(new QPushButton(QStringLiteral("查询"), this));
    tools->addWidget(new QPushButton(QStringLiteral("确认报警"), this));
    tools->addWidget(new QPushButton(QStringLiteral("清空已恢复"), this));
    tools->addWidget(new QPushButton(QStringLiteral("导出日志"), this));
    tools->addStretch();
    layout->addLayout(tools);

    m_table = new AlarmTableWidget(this);
    layout->addWidget(m_table, 1);

    connect(m_alarm, &AlarmManager::alarmAdded, this, &AlarmLogPage::refreshTable);
    connect(m_alarm, &AlarmManager::alarmUpdated, this, &AlarmLogPage::refreshTable);
    refreshTable();
}

void AlarmLogPage::refreshTable()
{
    m_table->setAlarms(m_alarm->alarms());
}
