#include "AlarmTableWidget.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QFont>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidgetItem>

namespace {

qint64 normalizedMs(qint64 timestamp)
{
    if (timestamp <= 0) {
        return 0;
    }

    return timestamp > 100000000000LL ? timestamp : timestamp * 1000;
}

QString displayTime(qint64 timestamp)
{
    const qint64 ms = normalizedMs(timestamp);
    if (ms <= 0) {
        return QStringLiteral("--");
    }

    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("HH:mm:ss"));
}

QString displayMaster(const AlarmRecord &alarm)
{
    if (alarm.masterSlot >= 0) {
        return QStringLiteral("RS485-%1").arg(alarm.masterSlot + 1);
    }

    return alarm.gatewayId.isEmpty() ? QStringLiteral("--") : alarm.gatewayId;
}

QString displaySlave(const AlarmRecord &alarm)
{
    return alarm.slaveAddr > 0 ? QString::number(alarm.slaveAddr) : QStringLiteral("--");
}

QString displayContent(const AlarmRecord &alarm)
{
    if (!alarm.message.isEmpty()) {
        return alarm.message;
    }

    return QStringLiteral("当前值 %1，阈值 %2")
        .arg(alarm.value, 0, 'f', 2)
        .arg(alarm.threshold, 0, 'f', 2);
}

QString displayState(const QString &state)
{
    if (state == "active") {
        return QStringLiteral("未确认");
    }
    if (state == "acked" || state == "acknowledged") {
        return QStringLiteral("已确认");
    }
    if (state == "recovered") {
        return QStringLiteral("已恢复");
    }

    return state.isEmpty() ? QStringLiteral("--") : state;
}

QTableWidgetItem *makeItem(const QString &text, Qt::Alignment alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(alignment);
    return item;
}

} // namespace

AlarmTableWidget::AlarmTableWidget(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(8);
    setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("主站"), QStringLiteral("从站"),
                               QStringLiteral("类型"), QStringLiteral("内容"), QStringLiteral("状态"),
                               QStringLiteral("确认时间"), QStringLiteral("操作")});
    horizontalHeader()->setStretchLastSection(false);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setVisible(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setAlternatingRowColors(false);
    setShowGrid(true);
    setWordWrap(false);
    setFocusPolicy(Qt::NoFocus);
}

void AlarmTableWidget::setAlarms(const QList<AlarmRecord> &alarms)
{
    setRowCount(0);
    setRowCount(alarms.size());
    for (int r = 0; r < alarms.size(); ++r) {
        const auto &a = alarms[r];
        setRowHeight(r, 40);
        setItem(r, 0, makeItem(displayTime(a.startTime), Qt::AlignCenter));
        setItem(r, 1, makeItem(displayMaster(a), Qt::AlignCenter));
        setItem(r, 2, makeItem(displaySlave(a), Qt::AlignCenter));
        setItem(r, 3, makeItem(a.alarmType.isEmpty() ? QStringLiteral("--") : a.alarmType));
        setItem(r, 4, makeItem(displayContent(a)));

        auto *stateItem = makeItem(displayState(a.state), Qt::AlignCenter);
        if (a.state == "active") {
            stateItem->setForeground(QColor(QStringLiteral("#EF4444")));
            QFont stateFont = stateItem->font();
            stateFont.setBold(true);
            stateItem->setFont(stateFont);
        }
        setItem(r, 5, stateItem);
        setItem(r, 6, makeItem(displayTime(a.ackTime), Qt::AlignCenter));

        if (a.state == "active") {
            auto *button = new QPushButton(QStringLiteral("确认"), this);
            button->setObjectName(QStringLiteral("AlarmInlineButton"));
            button->setCursor(Qt::PointingHandCursor);
            const QString alarmId = a.alarmId;
            connect(button, &QPushButton::clicked, this, [this, alarmId]() {
                emit acknowledgeRequested(alarmId);
            });
            setCellWidget(r, 7, button);
        } else {
            setItem(r, 7, makeItem(QStringLiteral("--"), Qt::AlignCenter));
        }
    }
}
