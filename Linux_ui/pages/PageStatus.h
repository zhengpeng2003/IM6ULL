#pragma once

#include <QLabel>
#include <QString>
#include <QWidget>

#include "data/data_protocol.h"
#include "ipc/ipc_client.h"
#include "ui/widget.h"

class PageStatus : public QWidget
{
    Q_OBJECT
public:
    explicit PageStatus(QWidget *parent = nullptr);

private:
    void initUI();
    void onDeviceStatus(const DataPack &pack);
    void onPortStatusUpdated(int slot,
                             const QString &port,
                             const QString &deviceType,
                             int baud,
                             bool connected,
                             const QString &message);
    QWidget *createRow(const QString &text, QLabel* &valueLabel, QWidget* &statusLight);
    void refreshVisibleRows();

    QLabel* emptyHintLabel = nullptr;
    QLabel* tempLabel = nullptr;
    QLabel* humLabel = nullptr;
    QLabel* relayLabel = nullptr;
    QLabel* fanLabel = nullptr;
    QLabel* ledLabel = nullptr;
    QLabel* buzzerLabel = nullptr;

    QWidget* tempStatus = nullptr;
    QWidget* humStatus = nullptr;
    QWidget* relayStatus = nullptr;
    QWidget* fanStatus = nullptr;
    QWidget* ledStatus = nullptr;
    QWidget* buzzerStatus = nullptr;

    QWidget* tempRow = nullptr;
    QWidget* humRow = nullptr;
    QWidget* relayRow = nullptr;
    QWidget* fanRow = nullptr;
    QWidget* ledRow = nullptr;
    QWidget* buzzerRow = nullptr;

    bool sensorConnected = false;
    bool relayConnected = false;
    QString slotDeviceTypes[2];
};
