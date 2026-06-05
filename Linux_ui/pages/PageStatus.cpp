#include "PageStatus.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
PageStatus::PageStatus(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();   // ⭐ 构造函数只干这一件事
    connect(Widget::_Myclient, &IpcClient::deviceStatusUpdated,
            this, &PageStatus::onDeviceStatus);

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

    mainLayout->addWidget(createRow("温度", tempLabel, tempStatus));
    mainLayout->addWidget(createRow("湿度", humLabel, humStatus));
    mainLayout->addWidget(createRow("继电器", relayLabel, relayStatus));
    mainLayout->addWidget(createRow("风扇", fanLabel, fanStatus));
    mainLayout->addWidget(createRow("LED", ledLabel, ledStatus));
    mainLayout->addWidget(createRow("蜂鸣器", buzzerLabel, buzzerStatus));

    mainLayout->addStretch();
}


QWidget* PageStatus::createRow(const QString &text, QLabel* &valueLabel, QWidget* &statusLight)
{
    QWidget *row = new QWidget(this);
    row->setObjectName("Row");
    row->setFixedHeight(44);

    QLabel *label = new QLabel(text, row);

    // ===== 值显示框 =====
    QWidget *valueBox = new QWidget(row);
    valueBox->setObjectName("ValueBox");
    valueBox->setFixedHeight(28);

    valueLabel = new QLabel("...", valueBox);   // 这里把指针传出
    valueLabel->setObjectName("ValueText");
    valueLabel->setAlignment(Qt::AlignCenter);

    QHBoxLayout *valueLayout = new QHBoxLayout(valueBox);
    valueLayout->setContentsMargins(6, 0, 6, 0);
    valueLayout->addWidget(valueLabel);

    // ===== 状态灯 =====
    statusLight = new QWidget(row);
    statusLight->setObjectName("StatusGreen");
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
#include <QDebug>

void PageStatus::onDeviceStatus(const DataPack &pack)
{
    qDebug() << "===== DeviceStatus Update =====";
    qDebug() << "seq:" << pack.seq
             << "time:" << pack.time.toString("yyyy-MM-dd hh:mm:ss")
             << "device count:" << pack.devices.size();

    for (const auto &dev : pack.devices) {

        qDebug() << "---- device ----";
        qDebug() << "id:" << dev.deviceId
                 << "type:" << dev.type
                 << "valid:" << dev.valid;

        switch (dev.type) {

        case DEV_SENSOR_TH:
            qDebug() << "[SENSOR_TH]"
                     << "temp:" << dev.temperature
                     << "humi:" << dev.humidity;

            tempLabel->setText(QString::number(dev.temperature, 'f', 1));
            humLabel->setText(QString::number(dev.humidity, 'f', 1));
            emit Widget::_Myclient->devicetrend(pack);
            break;

        case DEV_RELAY: {
            quint16 states = dev.relayStates;

            qDebug() << "[RELAY] raw states(bitmask):"
                     << QString("0x%1").arg(states, 4, 16, QLatin1Char('0'));

            bool ledOn    = states & (1 << 0);
            bool fanOn    = states & (1 << 1);
            bool buzzerOn = states & (1 << 2);

            qDebug() << " LED:" << ledOn
                     << " FAN:" << fanOn
                     << " BUZZER:" << buzzerOn;

            setStatusLight(ledStatus, ledOn);
            setStatusLight(fanStatus, fanOn);
            setStatusLight(buzzerStatus, buzzerOn);

            ledLabel->setText(ledOn ? "ON" : "OFF");
            fanLabel->setText(fanOn ? "ON" : "OFF");
            buzzerLabel->setText(buzzerOn ? "ON" : "OFF");
            emit Widget::_Myclient->devicesetting(pack);
            break;
        }

        case DEV_SYSINFO: {
            const auto &sys = dev.sys;
            qDebug() << "[SYSINFO]"
                     << "kernel:" << sys.kernel
                     << "arch:"   << sys.arch
                     << "os:"     << sys.os
                     << "screen:" << sys.screenW << "x" << sys.screenH;
            emit Widget::_Myclient->deviceinfo(pack);

            break;
        }

        default:
            qDebug() << "Unknown device type:" << dev.type;
            break;
        }
    }

    qDebug() << "===============================";
}





