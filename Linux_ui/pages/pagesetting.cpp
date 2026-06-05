#include "pagesetting.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QScrollArea>
#include <QVBoxLayout>

PageSetting::PageSetting(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");

    QPushButton *scanButton = new QPushButton("扫描", this);
    scanButton->setObjectName("ActionButton");
    portHintLabel = new QLabel("请先扫描端口", this);
    portHintLabel->setObjectName("HintText");

    QHBoxLayout *scanLayout = new QHBoxLayout;
    scanLayout->setContentsMargins(0, 0, 0, 0);
    scanLayout->setSpacing(6);
    scanLayout->addWidget(scanButton);
    scanLayout->addWidget(portHintLabel, 1);

    QWidget *portPanelA = createPortPanel("端口 A", 0, portA);
    QWidget *portPanelB = createPortPanel("端口 B", 1, portB);

    relayTitle = new QLabel("继电器控制", this);
    relayTitle->setObjectName("SectionTitle");
    relayHintLabel = new QLabel("连接继电器设备后显示 LED / FAN / BUZZER 控制", this);
    relayHintLabel->setObjectName("EmptyHintSmall");
    relayHintLabel->setAlignment(Qt::AlignCenter);
    relayHintLabel->setWordWrap(true);

    QLabel *ledLabel = new QLabel("LED", this);
    QLabel *fanLabel = new QLabel("FAN", this);
    QLabel *buzzerLabel = new QLabel("BUZZER", this);

    ledSwitch = new SwitchButtonWidget(this);
    fanSwitch = new SwitchButtonWidget(this);
    buzzerSwitch = new SwitchButtonWidget(this);
    ledSwitch->setFixedWidth(50);
    fanSwitch->setFixedWidth(50);
    buzzerSwitch->setFixedWidth(50);

    auto makeRelayRow = [this](QLabel *label, SwitchButtonWidget *sw) {
        QWidget *row = new QWidget(this);
        row->setObjectName("Row");
        row->setFixedHeight(40);
        QHBoxLayout *layout = new QHBoxLayout(row);
        layout->setContentsMargins(8, 0, 8, 0);
        layout->addWidget(label);
        layout->addStretch();
        layout->addWidget(sw);
        return row;
    };

    QWidget *content = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 6, 6, 6);
    contentLayout->setSpacing(6);
    contentLayout->addLayout(scanLayout);
    contentLayout->addWidget(portPanelA);
    contentLayout->addWidget(portPanelB);

    ledRow = makeRelayRow(ledLabel, ledSwitch);
    fanRow = makeRelayRow(fanLabel, fanSwitch);
    buzzerRow = makeRelayRow(buzzerLabel, buzzerSwitch);

    contentLayout->addWidget(relayTitle);
    contentLayout->addWidget(relayHintLabel);
    contentLayout->addWidget(ledRow);
    contentLayout->addWidget(fanRow);
    contentLayout->addWidget(buzzerRow);
    contentLayout->addStretch();

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(content);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    connect(scanButton, &QPushButton::clicked, this, &PageSetting::scanPorts);
    connect(portA.connectButton, &QPushButton::clicked, this, &PageSetting::connectSlotA);
    connect(portB.connectButton, &QPushButton::clicked, this, &PageSetting::connectSlotB);
    connect(portA.disconnectButton, &QPushButton::clicked, this, &PageSetting::disconnectSlotA);
    connect(portB.disconnectButton, &QPushButton::clicked, this, &PageSetting::disconnectSlotB);

    connect(ledSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onLedChanged);
    connect(fanSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onFanChanged);
    connect(buzzerSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onBuzzerChanged);

    connect(Widget::_Myclient, &IpcClient::devicesetting, this, &PageSetting::addSetting);
    connect(Widget::_Myclient, &IpcClient::portsUpdated, this, &PageSetting::onPortsUpdated);
    connect(Widget::_Myclient, &IpcClient::portStatusUpdated, this, &PageSetting::onPortStatusUpdated);

    refreshPortBoxes();
    refreshRelayControls();
    scanPorts();
}

QWidget *PageSetting::createPortPanel(const QString &title, int slot, PortControls &controls)
{
    QFrame *panel = new QFrame(this);
    panel->setObjectName("PortPanel");

    QLabel *titleLabel = new QLabel(title, panel);
    titleLabel->setObjectName("SectionTitle");

    controls.portBox = new QComboBox(panel);
    controls.typeBox = new QComboBox(panel);
    controls.baudBox = new QComboBox(panel);
    controls.connectButton = new QPushButton("连接", panel);
    controls.disconnectButton = new QPushButton("断开", panel);
    controls.statusLabel = new QLabel("未连接", panel);

    controls.typeBox->addItem("温湿度", "sensor_th");
    controls.typeBox->addItem("继电器", "relay");
    controls.typeBox->addItem("未知", "unknown");
    controls.typeBox->setCurrentIndex(slot == 0 ? 1 : 0);

    const QList<int> bauds = {9600, 19200, 38400, 57600, 115200};
    for (int baud : bauds)
        controls.baudBox->addItem(QString::number(baud), baud);
    controls.baudBox->setCurrentText(slot == 0 ? "38400" : "9600");

    controls.connectButton->setObjectName("ActionButton");
    controls.disconnectButton->setObjectName("GhostButton");
    controls.disconnectButton->setEnabled(false);
    controls.statusLabel->setObjectName("HintText");
    controls.statusLabel->setWordWrap(true);

    QGridLayout *grid = new QGridLayout(panel);
    grid->setContentsMargins(8, 8, 8, 8);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->addWidget(titleLabel, 0, 0, 1, 2);
    grid->addWidget(new QLabel("端口", panel), 1, 0);
    grid->addWidget(controls.portBox, 1, 1);
    grid->addWidget(new QLabel("设备", panel), 2, 0);
    grid->addWidget(controls.typeBox, 2, 1);
    grid->addWidget(new QLabel("波特率", panel), 3, 0);
    grid->addWidget(controls.baudBox, 3, 1);
    grid->addWidget(controls.connectButton, 4, 0);
    grid->addWidget(controls.disconnectButton, 4, 1);
    grid->addWidget(controls.statusLabel, 5, 0, 1, 2);

    return panel;
}

void PageSetting::scanPorts()
{
    QJsonObject root;
    root["cmd"] = "scan_ports";
    Widget::_Myclient->sendMessage(QJsonDocument(root).toJson(QJsonDocument::Compact));
    portHintLabel->setText("正在扫描...");
}

void PageSetting::onPortsUpdated(const QStringList &ports)
{
    availablePorts = ports;
    portHintLabel->setText(ports.isEmpty()
                           ? "未检测到端口"
                           : QString("检测到 %1 个端口").arg(ports.size()));
    refreshPortBoxes();
}

void PageSetting::refreshPortBoxes()
{
    auto updateBox = [this](PortControls &self, const PortControls &other) {
        const QString current = self.connected ? self.connectedPort : self.portBox->currentText();
        self.portBox->blockSignals(true);
        self.portBox->clear();

        if (availablePorts.isEmpty()) {
            self.portBox->addItem("未检测到端口");
            self.portBox->setEnabled(false);
            self.connectButton->setEnabled(false);
        } else {
            for (const QString &port : availablePorts) {
                if (other.connected && other.connectedPort == port)
                    continue;
                self.portBox->addItem(port);
            }
            self.portBox->setEnabled(!self.connected);
            self.connectButton->setEnabled(!self.connected && self.portBox->count() > 0);
        }

        int index = self.portBox->findText(current);
        if (index >= 0)
            self.portBox->setCurrentIndex(index);

        self.typeBox->setEnabled(!self.connected);
        self.baudBox->setEnabled(!self.connected);
        self.disconnectButton->setEnabled(self.connected);
        self.portBox->blockSignals(false);
    };

    updateBox(portA, portB);
    updateBox(portB, portA);
}

void PageSetting::connectSlotA()
{
    sendConnectCommand(0, portA);
}

void PageSetting::connectSlotB()
{
    sendConnectCommand(1, portB);
}

void PageSetting::disconnectSlotA()
{
    sendDisconnectCommand(0);
}

void PageSetting::disconnectSlotB()
{
    sendDisconnectCommand(1);
}

void PageSetting::sendConnectCommand(int slot, PortControls &controls)
{
    const QString port = controls.portBox->currentText();
    if (port.isEmpty() || port == "未检测到端口")
        return;

    const PortControls &other = (slot == 0) ? portB : portA;
    if (other.connected && other.connectedPort == port) {
        controls.statusLabel->setText("该端口已连接，请选择其他端口");
        QMessageBox::warning(this, "端口重复", "该端口已连接，请选择其他端口");
        return;
    }

    QJsonObject root;
    root["cmd"] = "connect_port";
    root["slot"] = slot;
    root["port"] = port;
    root["device_type"] = deviceTypeFromCombo(controls.typeBox);
    root["baud"] = baudFromCombo(controls.baudBox);
    Widget::_Myclient->sendMessage(QJsonDocument(root).toJson(QJsonDocument::Compact));
    controls.statusLabel->setText("正在连接...");
}

void PageSetting::sendDisconnectCommand(int slot)
{
    QJsonObject root;
    root["cmd"] = "disconnect_port";
    root["slot"] = slot;
    Widget::_Myclient->sendMessage(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

void PageSetting::onPortStatusUpdated(int slot,
                                      const QString &port,
                                      const QString &deviceType,
                                      int baud,
                                      bool connected,
                                      const QString &message)
{
    PortControls &controls = (slot == 0) ? portA : portB;
    controls.connected = connected;
    controls.connectedPort = connected ? port : QString();

    if (slot == 0)
        portADeviceType = connected ? deviceType : QString();
    else
        portBDeviceType = connected ? deviceType : QString();

    if (connected) {
        controls.statusLabel->setText(QString("%1 已连接 %2 @ %3")
                                      .arg(port, deviceTypeText(deviceType))
                                      .arg(baud));
    } else {
        controls.statusLabel->setText(statusText(message));
        if (message == "port_already_connected")
            QMessageBox::warning(this, "端口重复", "该端口已连接，请选择其他端口");
    }

    refreshPortBoxes();
    refreshRelayControls();
}

QString PageSetting::deviceTypeFromCombo(const QComboBox *box) const
{
    return box->currentData().toString();
}

QString PageSetting::deviceTypeText(const QString &type) const
{
    if (type == "sensor_th") return "温湿度";
    if (type == "relay") return "继电器";
    return "未知";
}

QString PageSetting::statusText(const QString &message) const
{
    if (message == "port_already_connected") return "该端口已连接，请选择其他端口";
    if (message == "open_failed") return "端口打开失败";
    if (message == "invalid_request") return "连接参数错误";
    if (message == "disconnected") return "已断开";
    if (message == "connected") return "已连接";
    return message.isEmpty() ? "未连接" : message;
}

int PageSetting::baudFromCombo(const QComboBox *box) const
{
    return box->currentData().toInt();
}

void PageSetting::refreshRelayControls()
{
    const bool showRelay = (portA.connected && portADeviceType == "relay") ||
                           (portB.connected && portBDeviceType == "relay");

    relayTitle->setVisible(showRelay);
    relayHintLabel->setVisible(!showRelay);
    ledRow->setVisible(showRelay);
    fanRow->setVisible(showRelay);
    buzzerRow->setVisible(showRelay);
}

void PageSetting::addSetting(const DataPack &pack)
{
    if (pack.devices.isEmpty()) return;

    const DeviceData &dev = pack.devices.first();
    if (dev.type != DEV_RELAY) return;

    relayStates = dev.relayStates;
    ledSwitch->setChecked(relayStates & (1 << 0));
    fanSwitch->setChecked(relayStates & (1 << 1));
    buzzerSwitch->setChecked(relayStates & (1 << 2));
}

void PageSetting::onLedChanged(bool state)
{
    if (state) relayStates |= (1 << 0); else relayStates &= ~(1 << 0);
    sendRelayStates();
}

void PageSetting::onFanChanged(bool state)
{
    if (state) relayStates |= (1 << 1); else relayStates &= ~(1 << 1);
    sendRelayStates();
}

void PageSetting::onBuzzerChanged(bool state)
{
    if (state) relayStates |= (1 << 2); else relayStates &= ~(1 << 2);
    sendRelayStates();
}

void PageSetting::sendRelayStates()
{
    DeviceData dev;
    dev.deviceId = 1;
    dev.type = DEV_RELAY;
    dev.valid = true;
    dev.relayStates = relayStates;

    QJsonObject root;
    root["seq"] = static_cast<qint64>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
    root["time"] = QDateTime::currentSecsSinceEpoch();

    QJsonArray devices;
    QJsonObject o;
    o["id"] = dev.deviceId;
    o["type"] = dev.type;
    o["valid"] = dev.valid;
    o["states"] = dev.relayStates;
    devices.append(o);
    root["devices"] = devices;

    Widget::_Myclient->sendMessage(QJsonDocument(root).toJson(QJsonDocument::Compact));
}
