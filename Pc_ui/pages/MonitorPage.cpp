#include "MonitorPage.h"
#include "ui/DeviceTreeWidget.h"
#include "core/DataManager.h"
#include "core/CommandManager.h"
#include "core/UiStateStore.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <QDebug>
#include <QShowEvent>
#include <QTimer>
#include <algorithm>

namespace {

constexpr int kRefreshDelayMs = 250;

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

MonitorPage::MonitorPage(DataManager *data, CommandManager *command,
                         UiStateStore *stateStore, QWidget *parent)
    : QWidget(parent), m_data(data), m_command(command), m_stateStore(stateStore)
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
    connect(m_tree, &DeviceTreeWidget::selectionLost, this, [this]() {
        m_currentKey.clear();
        setDetailText(QStringLiteral("请选择左侧设备"));
    });
    m_treeRefreshTimer = new QTimer(this);
    m_treeRefreshTimer->setSingleShot(true);
    connect(m_treeRefreshTimer, &QTimer::timeout, this, &MonitorPage::refreshDeviceTree);

    m_detailRefreshTimer = new QTimer(this);
    m_detailRefreshTimer->setSingleShot(true);
    connect(m_detailRefreshTimer, &QTimer::timeout, this, &MonitorPage::refreshDetail);

    if (m_stateStore) {
        connect(m_stateStore, &UiStateStore::stateChanged, this, &MonitorPage::scheduleRefreshDeviceTree);
        connect(m_stateStore, &UiStateStore::stateChanged, this, &MonitorPage::scheduleRefreshDetail);
    }
    connect(m_fanOn, &QPushButton::clicked, this, [this](){ sendFanCommand(true); });
    connect(m_fanOff, &QPushButton::clicked, this, [this](){ sendFanCommand(false); });

    refreshDeviceTree();
}

void MonitorPage::scheduleRefreshDeviceTree()
{
    m_treeRefreshDirty = true;
    const bool willStart = isVisible() && m_treeRefreshTimer && !m_treeRefreshTimer->isActive();
    qDebug() << "[DBG_PAGE] MonitorPage scheduleRefreshDeviceTree visible:" << isVisible()
             << "startDebounce250ms:" << willStart;
    if (!isVisible()) {
        return;
    }

    if (m_treeRefreshTimer && !m_treeRefreshTimer->isActive()) {
        m_treeRefreshTimer->start(kRefreshDelayMs);
    }
}

void MonitorPage::scheduleRefreshDetail()
{
    m_detailRefreshDirty = true;
    const bool willStart = isVisible() && m_detailRefreshTimer && !m_detailRefreshTimer->isActive();
    qDebug() << "[DBG_PAGE] MonitorPage scheduleRefreshDetail visible:" << isVisible()
             << "startDebounce250ms:" << willStart
             << "currentKey:" << m_currentKey;
    if (!isVisible()) {
        return;
    }

    if (m_detailRefreshTimer && !m_detailRefreshTimer->isActive()) {
        m_detailRefreshTimer->start(kRefreshDelayMs);
    }
}

void MonitorPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (m_treeRefreshDirty) {
        scheduleRefreshDeviceTree();
    }
    if (m_detailRefreshDirty) {
        scheduleRefreshDetail();
    }
}

void MonitorPage::refreshDeviceTree()
{
    m_treeRefreshDirty = false;
    const QList<DeviceNode> devices = m_data->deviceTreeSnapshot();
    qDebug() << "[DBG_PAGE] MonitorPage refreshDeviceTree executed deviceCount:"
             << devices.size()
             << "currentKey:" << m_currentKey;
    m_tree->setDevices(devices);
    if (devices.isEmpty()) {
        m_currentKey.clear();
        setDetailText(QStringLiteral("未收到 Pc_data 数据\n\n请确认 Pc_data 已启动并保持 IPC 连接。"));
        m_fanOn->setEnabled(false);
        m_fanOff->setEnabled(false);
        return;
    }

    if (!m_currentKey.isEmpty()) {
        refreshDetail();
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
    m_detailRefreshDirty = false;
    qDebug() << "[DBG_PAGE] MonitorPage refreshDetail executed currentKey:"
             << m_currentKey
             << "deviceCount:" << m_data->deviceTreeSnapshot().size();
    if (m_currentKey.isEmpty()) {
        if (m_data->deviceTreeSnapshot().isEmpty()) {
            setDetailText(QStringLiteral("未收到 Pc_data 数据\n\n请确认 Pc_data 已启动并保持 IPC 连接。"));
        }
        m_fanOn->setEnabled(false);
        m_fanOff->setEnabled(false);
        return;
    }

    const auto d = m_data->deviceData(m_currentKey);
    const auto &n = d.node;
    if (n.factoryId.isEmpty()) {
        setDetailText(QStringLiteral("当前设备暂无实时数据"));
        m_fanOn->setEnabled(false);
        m_fanOff->setEnabled(false);
        return;
    }

    QString text;
    text += QStringLiteral("工厂: %1\n厂房: %2\n网关: %3\n主站: RS485-%4 %5\n从站地址: %6\n设备名称: %7\n设备类型: %8\n在线状态: %9\n更新时间: %10\n\n")
        .arg(n.factoryId, n.areaName, n.gatewayId)
        .arg(n.masterSlot + 1).arg(n.masterName)
        .arg(n.slaveAddr).arg(n.deviceName, n.deviceType)
        .arg(d.serviceOffline ? QStringLiteral("服务离线") : (n.online ? QStringLiteral("在线") : QStringLiteral("离线")))
        .arg(QDateTime::fromMSecsSinceEpoch(d.timestamp).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));

    text += QStringLiteral("数据状态: %1\n").arg(d.dataState.isEmpty() ? QStringLiteral("未知") : d.dataState);
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
    const bool controlsEnabled = d.node.deviceType == QStringLiteral("relay") && !d.serviceOffline &&
        d.node.online && d.dataState == QStringLiteral("normal");
    m_fanOn->setEnabled(controlsEnabled);
    m_fanOff->setEnabled(controlsEnabled);
}

void MonitorPage::sendFanCommand(bool on)
{
    if (m_currentKey.isEmpty()) return;
    const auto d = m_data->deviceData(m_currentKey);
    if (d.node.deviceType != "relay") return;
    if (d.serviceOffline || !d.node.online || d.dataState != QStringLiteral("normal")) return;
    m_command->sendRelayCommand(d.node, "fan", on, d.relay.channels);
}

void MonitorPage::setDetailText(const QString &text)
{
    m_detail->setText(text);
}
