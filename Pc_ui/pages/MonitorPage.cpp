#include "MonitorPage.h"
#include "ui/DeviceTreeWidget.h"
#include "core/DataManager.h"
#include "core/CommandManager.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <algorithm>

namespace {

QString displayPointValue(const TelemetryPointData &point)
{
    if (!point.valid) {
        return point.errorMessage.isEmpty()
            ? QStringLiteral("无效")
            : QStringLiteral("无效(%1)").arg(point.errorMessage);
    }

    if (point.valueType == "text") {
        return point.textValue;
    }

    if (point.valueType == "boolean") {
        return point.numberValue != 0.0 ? QStringLiteral("ON") : QStringLiteral("OFF");
    }

    QString text = QString::number(point.numberValue, 'f', 2);
    if (!point.unit.isEmpty()) {
        text += QStringLiteral(" ") + point.unit;
    }
    return text;
}

QString displayPointName(const TelemetryPointData &point)
{
    if (!point.pointName.isEmpty()) {
        return point.pointName;
    }

    return point.pointKey;
}

} // namespace

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
    const QList<DeviceNode> devices = m_data->deviceTreeSnapshot();
    m_tree->setDevices(devices);
    if (devices.isEmpty()) {
        setDetailText(QStringLiteral("未收到 Pc_data 数据\n\n请确认 Pc_data 已启动并保持 IPC 连接。"));
    }
    //m_tree->expandAll();
}

void MonitorPage::onDeviceSelected(const QString &deviceKey)
{
    m_currentKey = deviceKey;
    refreshDetail();
}

void MonitorPage::refreshDetail()
{
    if (m_currentKey.isEmpty()) {
        if (m_data->deviceTreeSnapshot().isEmpty()) {
            setDetailText(QStringLiteral("未收到 Pc_data 数据\n\n请确认 Pc_data 已启动并保持 IPC 连接。"));
        }
        return;
    }

    const auto d = m_data->deviceData(m_currentKey);
    const auto &n = d.node;
    if (n.factoryId.isEmpty()) {
        setDetailText(QStringLiteral("当前设备暂无实时数据"));
        return;
    }

    QString text;
    text += QStringLiteral("工厂: %1\n厂房: %2\n网关: %3\n主站: RS485-%4 %5\n从站地址: %6\n设备名称: %7\n设备类型: %8\n在线状态: %9\n更新时间: %10\n\n")
        .arg(n.factoryId, n.areaName, n.gatewayId)
        .arg(n.masterSlot + 1).arg(n.masterName)
        .arg(n.slaveAddr).arg(n.deviceName, n.deviceType)
        .arg(n.online ? QStringLiteral("在线") : QStringLiteral("离线"))
        .arg(d.timestamp);

    text += QStringLiteral("解析状态: %1\n").arg(d.statusText.isEmpty() ? QStringLiteral("未知") : d.statusText);
    text += QStringLiteral("状态等级: %1\n").arg(d.statusLevel.isEmpty() ? QStringLiteral("unknown") : d.statusLevel);
    text += QStringLiteral("数据有效: %1\n").arg(d.valid ? QStringLiteral("是") : QStringLiteral("否"));
    if (!d.errorMessage.isEmpty()) {
        text += QStringLiteral("异常原因: %1\n").arg(d.errorMessage);
    }

    if (d.points.isEmpty()) {
        text += QStringLiteral("\n测点: 暂无\n");
        setDetailText(text);
        return;
    }

    QList<TelemetryPointData> points = d.points;
    std::sort(points.begin(), points.end(), [](const TelemetryPointData &left, const TelemetryPointData &right) {
        return left.pointKey < right.pointKey;
    });

    text += QStringLiteral("\n测点列表:\n");
    for (const TelemetryPointData &point : points) {
        text += QStringLiteral("- %1 [%2]: %3")
            .arg(displayPointName(point), point.pointKey, displayPointValue(point));
        if (!point.valid && !point.errorMessage.isEmpty()) {
            text += QStringLiteral("  原因: %1").arg(point.errorMessage);
        }
        text += QStringLiteral("\n");
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
