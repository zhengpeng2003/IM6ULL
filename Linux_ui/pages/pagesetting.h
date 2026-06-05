#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QWidget>

#include "data/data_protocol.h"
#include "ipc/ipc_client.h"
#include "ui/switchbuttonwidget.h"
#include "ui/widget.h"

class PageSetting : public QWidget
{
    Q_OBJECT
public:
    explicit PageSetting(QWidget *parent = nullptr);

public slots:
    void addSetting(const DataPack &pack);
    void onPortsUpdated(const QStringList &ports);
    void onPortStatusUpdated(int slot,
                             const QString &port,
                             const QString &deviceType,
                             int baud,
                             bool connected,
                             const QString &message);

private slots:
    void onLedChanged(bool state);
    void onFanChanged(bool state);
    void onBuzzerChanged(bool state);
    void scanPorts();
    void connectSlotA();
    void connectSlotB();
    void disconnectSlotA();
    void disconnectSlotB();

private:
    struct PortControls {
        QComboBox *portBox = nullptr;
        QComboBox *typeBox = nullptr;
        QComboBox *baudBox = nullptr;
        QPushButton *connectButton = nullptr;
        QPushButton *disconnectButton = nullptr;
        QLabel *statusLabel = nullptr;
        QString connectedPort;
        bool connected = false;
    };

    QWidget *createPortPanel(const QString &title, int slot, PortControls &controls);
    void sendConnectCommand(int slot, PortControls &controls);
    void sendDisconnectCommand(int slot);
    void refreshPortBoxes();
    QString deviceTypeFromCombo(const QComboBox *box) const;
    QString deviceTypeText(const QString &type) const;
    QString statusText(const QString &message) const;
    int baudFromCombo(const QComboBox *box) const;
    void refreshRelayControls();
    void sendRelayStates();

    PortControls portA;
    PortControls portB;
    QStringList availablePorts;
    QLabel *portHintLabel = nullptr;
    QString portADeviceType;
    QString portBDeviceType;
    QLabel *relayTitle = nullptr;
    QLabel *relayHintLabel = nullptr;
    QWidget *ledRow = nullptr;
    QWidget *fanRow = nullptr;
    QWidget *buzzerRow = nullptr;

    SwitchButtonWidget *ledSwitch = nullptr;
    SwitchButtonWidget *fanSwitch = nullptr;
    SwitchButtonWidget *buzzerSwitch = nullptr;

    quint16 relayStates = 0;
};
