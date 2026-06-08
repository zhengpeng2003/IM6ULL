#pragma once
#include <QTableWidget>
#include "model/AlarmModel.h"

class AlarmTableWidget : public QTableWidget
{
    Q_OBJECT
public:
    explicit AlarmTableWidget(QWidget *parent = nullptr);
    void setAlarms(const QList<AlarmRecord> &alarms);
};
