#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <qjsondocument.h>
#include <qjsonobject.h>
#include <QDebug>
#include <QLabel>
#include "ipc/ipc_client.h"
#include "ui/widget.h"
#include "data/data_protocol.h"
class PageStatus : public QWidget
{
    Q_OBJECT
public:
    explicit PageStatus(QWidget *parent = nullptr);

private:
    void initUI();   // ⭐ 统一初始化
    void onDeviceStatus(const DataPack &pack);
    QWidget *createRow(const QString &text, QLabel* &valueLabel, QWidget* &statusLight);

    // 保存显示控件指针
    QLabel* tempLabel;
    QLabel* humLabel;
    QLabel* relayLabel;
    QLabel* fanLabel;
    QLabel* ledLabel;
    QLabel* buzzerLabel;

    QWidget* tempStatus;
    QWidget* humStatus;
    QWidget* relayStatus;
    QWidget* fanStatus;
    QWidget* ledStatus;
    QWidget* buzzerStatus;

};

