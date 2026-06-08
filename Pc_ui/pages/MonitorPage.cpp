#include "MonitorPage.h"
#include "ui/DeviceTreeWidget.h"
#include "core/DataManager.h"
#include "core/CommandManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>

MonitorPage::MonitorPage(DataManager *data, CommandManager *command, QWidget *parent)
    : QWidget(parent), m_data(data), m_command(command)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("实时监控"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *body = new QHBoxLayout;
    m_tree = new DeviceTreeWidget(this);
    m_detail = new QLabel(QStringLiteral("请选择左侧设备"), this);
    m_detail->setObjectName("DetailPanel");
    m_detail->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_detail->setMinimumWidth(500);

    auto *right = new QVBoxLayout;
    right->addWidget(m_detail, 1);
    auto *btnLayout = new QHBoxLayout;
    m_fanOn = new QPushButton(QStringLiteral("FAN 开"), this);
    m_fanOff = new QPushButton(QStringLiteral("FAN 关"), this);
    btnLayout->addWidget(m_fanOn);
    btnLayout->addWidget(m_fanOff);
    btnLayout->addStretch();
    right->addLayout(btnLayout);

    body->addWidget(m_tree, 1);
    body->addLayout(right, 2);
    layout->addLayout(body, 1);

    connect(m_tree, &DeviceTreeWidget::deviceSelected, this, &MonitorPage::onDeviceSelected);
    connect(m_data, &DataManager::deviceTreeChanged, this, &MonitorPage::refreshDeviceTree);
    connect(m_fanOn, &QPushButton::clicked, this, [this](){ sendFanCommand(true); });
    connect(m_fanOff, &QPushButton::clicked, this, [this](){ sendFanCommand(false); });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MonitorPage::refreshDetail);
    m_timer->start(1000);

    refreshDeviceTree();
}

void MonitorPage::refreshDeviceTree()
{
    m_tree->setDevices(m_data->deviceTreeSnapshot());
}

void MonitorPage::onDeviceSelected(const QString &deviceKey)
{
    m_currentKey = deviceKey;
    refreshDetail();
}

void MonitorPage::refreshDetail()
{
    if (m_currentKey.isEmpty()) return;
    const auto d = m_data->deviceData(m_currentKey);
    const auto &n = d.node;
    QString text;
    text += QStringLiteral("工厂: %1\n厂房: %2\n网关: %3\n主站: RS485-%4 %5\n从站地址: %6\n设备名称: %7\n设备类型: %8\n在线状态: %9\n更新时间: %10\n\n")
        .arg(n.factoryId, n.areaName, n.gatewayId)
        .arg(n.masterSlot + 1).arg(n.masterName)
        .arg(n.slaveAddr).arg(n.deviceName, n.deviceType)
        .arg(n.online ? QStringLiteral("在线") : QStringLiteral("离线"))
        .arg(d.timestamp);

    if (n.deviceType == "sensor_th") {
        text += QStringLiteral("温度: %1 ℃\n湿度: %2 %\n").arg(d.sensorTh.temperature).arg(d.sensorTh.humidity);
    } else if (n.deviceType == "relay") {
        text += QStringLiteral("LED: %1\nFAN: %2\nBUZZER: %3\n")
            .arg(d.relay.led ? "ON" : "OFF", d.relay.fan ? "ON" : "OFF", d.relay.buzzer ? "ON" : "OFF");
    } else if (n.deviceType == "meter") {
        text += QStringLiteral("电压: %1 V\n电流: %2 A\n功率: %3 W\n电能: %4 kWh\n")
            .arg(d.meter.voltage).arg(d.meter.current).arg(d.meter.power).arg(d.meter.energy);
    }
    setDetailText(text);
}

void MonitorPage::sendFanCommand(bool on)
{
    if (m_currentKey.isEmpty()) return;
    const auto d = m_data->deviceData(m_currentKey);
    if (d.node.deviceType != "relay") return;
    m_command->sendRelayCommand(d.node, "fan", on);
}

void MonitorPage::setDetailText(const QString &text)
{
    m_detail->setText(text);
}
