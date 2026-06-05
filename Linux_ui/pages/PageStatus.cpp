#include "PageStatus.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

PageStatus::PageStatus(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();

    connect(Widget::_Myclient, &IpcClient::deviceStatusUpdated,
            this, &PageStatus::onDeviceStatus);
    connect(Widget::_Myclient, &IpcClient::portStatusUpdated,
            this, &PageStatus::onPortStatusUpdated);
}

static void setStatusLight(QWidget *w, bool on)
{
    w->setObjectName(on ? "StatusGreen" : "StatusGray");
    w->style()->unpolish(w);
    w->style()->polish(w);
}

void PageStatus::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 6, 6, 6);
    mainLayout->setSpacing(6);

    emptyHintLabel = new QLabel("未连接设备\n请到设置页扫描端口并连接设备", this);
    emptyHintLabel->setObjectName("EmptyHint");
    emptyHintLabel->setAlignment(Qt::AlignCenter);
    emptyHintLabel->setWordWrap(true);
    emptyHintLabel->setMinimumHeight(120);

    tempRow = createRow("温度", tempLabel, tempStatus);
    humRow = createRow("湿度", humLabel, humStatus);
    relayRow = createRow("继电器", relayLabel, relayStatus);
    fanRow = createRow("风扇", fanLabel, fanStatus);
    ledRow = createRow("LED", ledLabel, ledStatus);
    buzzerRow = createRow("蜂鸣器", buzzerLabel, buzzerStatus);

    mainLayout->addWidget(emptyHintLabel);
    mainLayout->addWidget(tempRow);
    mainLayout->addWidget(humRow);
    mainLayout->addWidget(relayRow);
    mainLayout->addWidget(fanRow);
    mainLayout->addWidget(ledRow);
    mainLayout->addWidget(buzzerRow);
    mainLayout->addStretch();

    refreshVisibleRows();
}

QWidget *PageStatus::createRow(const QString &text, QLabel* &valueLabel, QWidget* &statusLight)
{
    QWidget *row = new QWidget(this);
    row->setObjectName("Row");
    row->setFixedHeight(44);

    QLabel *label = new QLabel(text, row);

    QWidget *valueBox = new QWidget(row);
    valueBox->setObjectName("ValueBox");
    valueBox->setFixedHeight(28);

    valueLabel = new QLabel("...", valueBox);
    valueLabel->setObjectName("ValueText");
    valueLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *valueLayout = new QHBoxLayout(valueBox);
    valueLayout->setContentsMargins(6, 0, 6, 0);
    valueLayout->addWidget(valueLabel);

    statusLight = new QWidget(row);
    statusLight->setObjectName("StatusGray");
    statusLight->setFixedSize(10, 10);

    QHBoxLayout *layout = new QHBoxLayout(row);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->addWidget(label);
    layout->addStretch();
    layout->addWidget(valueBox);
    layout->addSpacing(8);
    layout->addWidget(statusLight);

    return row;
}

void PageStatus::refreshVisibleRows()
{
    tempRow->setVisible(sensorConnected);
    humRow->setVisible(sensorConnected);
    relayRow->setVisible(relayConnected);
    fanRow->setVisible(relayConnected);
    ledRow->setVisible(relayConnected);
    buzzerRow->setVisible(relayConnected);
    emptyHintLabel->setVisible(!sensorConnected && !relayConnected);
}

void PageStatus::onPortStatusUpdated(int slot,
                                     const QString &port,
                                     const QString &deviceType,
                                     int baud,
                                     bool connected,
                                     const QString &message)
{
    Q_UNUSED(port)
    Q_UNUSED(baud)
    Q_UNUSED(message)

    if (slot >= 0 && slot < 2)
        slotDeviceTypes[slot] = connected ? deviceType : QString();

    sensorConnected = (slotDeviceTypes[0] == "sensor_th") ||
                      (slotDeviceTypes[1] == "sensor_th");
    relayConnected = (slotDeviceTypes[0] == "relay") ||
                     (slotDeviceTypes[1] == "relay");

    if (!connected) {
        if (deviceType == "sensor_th") {
            tempLabel->setText("...");
            humLabel->setText("...");
            setStatusLight(tempStatus, false);
            setStatusLight(humStatus, false);
        } else if (deviceType == "relay") {
            relayLabel->setText("...");
            fanLabel->setText("...");
            ledLabel->setText("...");
            buzzerLabel->setText("...");
            setStatusLight(relayStatus, false);
            setStatusLight(fanStatus, false);
            setStatusLight(ledStatus, false);
            setStatusLight(buzzerStatus, false);
        }
    }

    refreshVisibleRows();
}

void PageStatus::onDeviceStatus(const DataPack &pack)
{
    for (const auto &dev : pack.devices) {
        switch (dev.type) {
        case DEV_SENSOR_TH:
            if (!sensorConnected)
                break;

            tempLabel->setText(dev.valid ? QString::number(dev.temperature, 'f', 1) : "无响应");
            humLabel->setText(dev.valid ? QString::number(dev.humidity, 'f', 1) : "无响应");
            setStatusLight(tempStatus, dev.valid);
            setStatusLight(humStatus, dev.valid);
            emit Widget::_Myclient->devicetrend(pack);
            break;

        case DEV_RELAY: {
            if (!relayConnected)
                break;

            const quint16 states = dev.relayStates;
            const bool ledOn = states & (1 << 0);
            const bool fanOn = states & (1 << 1);
            const bool buzzerOn = states & (1 << 2);

            relayLabel->setText(dev.valid ? "在线" : "无响应");
            ledLabel->setText(dev.valid ? (ledOn ? "ON" : "OFF") : "无响应");
            fanLabel->setText(dev.valid ? (fanOn ? "ON" : "OFF") : "无响应");
            buzzerLabel->setText(dev.valid ? (buzzerOn ? "ON" : "OFF") : "无响应");

            setStatusLight(relayStatus, dev.valid);
            setStatusLight(ledStatus, dev.valid && ledOn);
            setStatusLight(fanStatus, dev.valid && fanOn);
            setStatusLight(buzzerStatus, dev.valid && buzzerOn);
            emit Widget::_Myclient->devicesetting(pack);
            break;
        }

        case DEV_SYSINFO:
            emit Widget::_Myclient->deviceinfo(pack);
            break;

        default:
            break;
        }
    }
}
