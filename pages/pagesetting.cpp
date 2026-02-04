#include "pagesetting.h"
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

PageSetting::PageSetting(QWidget *parent)
    : QWidget(parent)
    , relayStates(0)
{
    // 标签
    QLabel *ledLabel    = new QLabel("LED", this);
    QLabel *fanLabel    = new QLabel("FAN", this);
    QLabel *buzzerLabel = new QLabel("BUZZER", this);

    // 按钮
    ledSwitch    = new SwitchButtonWidget(this);
    fanSwitch    = new SwitchButtonWidget(this);
    buzzerSwitch = new SwitchButtonWidget(this);

    ledSwitch->setFixedWidth(50);
    fanSwitch->setFixedWidth(50);
    buzzerSwitch->setFixedWidth(50);

    // 水平布局：标签 + spacer + 按钮
    QHBoxLayout *ledLayout = new QHBoxLayout;
    ledLayout->addWidget(ledLabel);
    ledLayout->addStretch();        // 填充中间空间
    ledLayout->addWidget(ledSwitch);

    QHBoxLayout *fanLayout = new QHBoxLayout;
    fanLayout->addWidget(fanLabel);
    fanLayout->addStretch();
    fanLayout->addWidget(fanSwitch);

    QHBoxLayout *buzzerLayout = new QHBoxLayout;
    buzzerLayout->addWidget(buzzerLabel);
    buzzerLayout->addStretch();
    buzzerLayout->addWidget(buzzerSwitch);

    // 垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addLayout(ledLayout);
    mainLayout->addLayout(fanLayout);
    mainLayout->addLayout(buzzerLayout);
    mainLayout->addStretch();       // 填充底部空间

    setLayout(mainLayout);

    // 连接槽
    connect(ledSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onLedChanged);
    connect(fanSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onFanChanged);
    connect(buzzerSwitch, &SwitchButtonWidget::stateChanged, this, &PageSetting::onBuzzerChanged);

    // 从板子来的消息
    connect(Widget::_Myclient, &IpcClient::devicesetting, this, &PageSetting::addSetting);
}

// 板子消息过来更新 UI
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

// 按钮改变槽
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

// 生成 DataPack 并通过 IPC 发送
void PageSetting::sendRelayStates()
{
    DeviceData dev;
    dev.deviceId = 1;
    dev.type = DEV_RELAY;
    dev.valid = true;
    dev.relayStates = relayStates;

    DataPack pack;
    pack.seq = QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF;
    pack.time = QDateTime::currentDateTime();
    pack.devices.append(dev);

    // 转 JSON
    QJsonObject root;
    root["seq"] = static_cast<qint64>(pack.seq);
    root["time"] = pack.time.toSecsSinceEpoch();

    QJsonArray devices;
    QJsonObject o;
    o["id"] = dev.deviceId;
    o["type"] = dev.type;
    o["valid"] = dev.valid;
    o["states"] = dev.relayStates;
    devices.append(o);
    root["devices"] = devices;

    QJsonDocument doc(root);
    QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

    // 发送到板子
    Widget::_Myclient->sendMessage(jsonData);  // 假设 IPC 客户端有 send(QByteArray) 方法
    qDebug() << "发送 relay 状态到板子:" << jsonData;
}
