#include "AlarmTableWidget.h"

AlarmTableWidget::AlarmTableWidget(QWidget *parent) : QTableWidget(parent)
{
    setColumnCount(8);
    setHorizontalHeaderLabels({QStringLiteral("报警ID"), QStringLiteral("设备"), QStringLiteral("类型"),
                               QStringLiteral("等级"), QStringLiteral("当前值"), QStringLiteral("阈值"),
                               QStringLiteral("状态"), QStringLiteral("开始时间")});
}

void AlarmTableWidget::setAlarms(const QList<AlarmRecord> &alarms)
{
    setRowCount(alarms.size());
    for (int r = 0; r < alarms.size(); ++r) {
        const auto &a = alarms[r];
        setItem(r, 0, new QTableWidgetItem(a.alarmId));
        setItem(r, 1, new QTableWidgetItem(a.deviceName));
        setItem(r, 2, new QTableWidgetItem(a.alarmType));
        setItem(r, 3, new QTableWidgetItem(a.level));
        setItem(r, 4, new QTableWidgetItem(QString::number(a.value)));
        setItem(r, 5, new QTableWidgetItem(QString::number(a.threshold)));
        setItem(r, 6, new QTableWidgetItem(a.state));
        setItem(r, 7, new QTableWidgetItem(QString::number(a.startTime)));
    }
}
