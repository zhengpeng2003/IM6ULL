#include "MonitorPage.h"
#include "ui/DeviceTreeWidget.h"
#include "core/DataManager.h"
#include "core/CommandManager.h"
#include "core/UiStateStore.h"
#include "sensorui/DeviceDetailCardBaseUi.h"
#include "sensorui/SensorThDetailCardUi.h"
#include "sensorui/RelayDetailCardUi.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QFrame>
#include <QMap>
#include <QScrollArea>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTimer>
#include <QVariant>

namespace {

constexpr int kRefreshDelayMs = 250;

bool containsDeviceKey(const QList<DeviceNode> &devices, const QString &deviceKey)
{
    for (const DeviceNode &device : devices) {
        if (device.key() == deviceKey) {
            return true;
        }
    }
    return false;
}

QMap<QString, bool> variantMapToRelayStates(const QVariantMap &channels)
{
    QMap<QString, bool> result;
    for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
        result.insert(it.key(), it.value().toBool());
    }
    return result;
}

} // namespace

MonitorPage::MonitorPage(DataManager *data, CommandManager *command,
                         UiStateStore *stateStore, QWidget *parent)
    : QWidget(parent), m_data(data), m_command(command), m_stateStore(stateStore)
{
    setObjectName("MonitorPage");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("实时监控"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *body = new QHBoxLayout;
    m_tree = new DeviceTreeWidget(this);
    m_detailScrollArea = new QScrollArea(this);
    m_detailScrollArea->setObjectName("DeviceDetailScrollArea");
    m_detailScrollArea->setWidgetResizable(true);
    m_detailScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_detailScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_detailScrollArea->setFrameShape(QFrame::NoFrame);

    m_detailStack = new QStackedWidget(m_detailScrollArea);
    m_detailStack->setObjectName("DetailPanel");
    m_detailStack->setMinimumWidth(500);
    m_detailStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

    m_emptyCard = new DeviceDetailCardBaseUi(m_detailStack);
    m_baseCard = new DeviceDetailCardBaseUi(m_detailStack);
    m_sensorThCard = new SensorThDetailCardUi(m_detailStack);
    m_relayCard = new RelayDetailCardUi(m_detailStack);

    m_detailStack->addWidget(m_emptyCard);
    m_detailStack->addWidget(m_baseCard);
    m_detailStack->addWidget(m_sensorThCard);
    m_detailStack->addWidget(m_relayCard);
    m_emptyCard->setMessage(QStringLiteral("请选择左侧设备"));

    m_detailScrollArea->setWidget(m_detailStack);

    body->addWidget(m_tree, 1);
    body->addWidget(m_detailScrollArea, 2);
    layout->addLayout(body, 1);

    connect(m_tree, &DeviceTreeWidget::deviceSelected, this, &MonitorPage::onDeviceSelected);
    connect(m_tree, &DeviceTreeWidget::selectionLost, this, [this]() {
        m_currentKey.clear();
        m_emptyCard->setMessage(QStringLiteral("请选择左侧设备"));
        m_detailStack->setCurrentWidget(m_emptyCard);
    });
    connect(m_relayCard, &RelayDetailCardUi::relayCommandRequested,
            this, &MonitorPage::onRelayCommandRequested);
    if (m_command) {
        connect(m_command, &CommandManager::relayPendingChanged,
                this, &MonitorPage::refreshRelayPendingState);
    }

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
        m_emptyCard->setMessage(QStringLiteral("未收到 Pc_data 数据"));
        m_detailStack->setCurrentWidget(m_emptyCard);
        return;
    }

    if (!m_currentKey.isEmpty() && !containsDeviceKey(devices, m_currentKey)) {
        m_currentKey.clear();
        m_emptyCard->setMessage(QStringLiteral("请选择左侧设备"));
        m_detailStack->setCurrentWidget(m_emptyCard);
        return;
    }

    if (!m_currentKey.isEmpty()) {
        refreshDetail();
    } else {
        m_emptyCard->setMessage(QStringLiteral("请选择左侧设备"));
        m_detailStack->setCurrentWidget(m_emptyCard);
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
            m_emptyCard->setMessage(QStringLiteral("未收到 Pc_data 数据"));
        } else {
            m_emptyCard->setMessage(QStringLiteral("请选择左侧设备"));
        }
        m_detailStack->setCurrentWidget(m_emptyCard);
        return;
    }

    const auto d = m_data->deviceData(m_currentKey);
    const auto &n = d.node;
    if (n.factoryId.isEmpty()) {
        m_emptyCard->setMessage(QStringLiteral("当前设备暂无实时数据"));
        m_detailStack->setCurrentWidget(m_emptyCard);
        return;
    }

    if (n.deviceType == QStringLiteral("sensor_th")) {
        m_sensorThCard->setDeviceData(d);
        m_detailStack->setCurrentWidget(m_sensorThCard);
    } else if (n.deviceType == QStringLiteral("relay")) {
        m_relayCard->setDeviceData(d);
        if (m_command) {
            m_relayCard->setPendingRelayChannels(m_command->pendingRelayChannels(n));
        }
        m_detailStack->setCurrentWidget(m_relayCard);
    } else {
        m_baseCard->setDeviceData(d);
        m_detailStack->setCurrentWidget(m_baseCard);
    }
}

void MonitorPage::refreshRelayPendingState()
{
    if (!m_command || !m_relayCard || m_currentKey.isEmpty()) {
        return;
    }

    const RealtimeDeviceData data = m_data->deviceData(m_currentKey);
    if (data.node.deviceType != QStringLiteral("relay")) {
        return;
    }

    m_relayCard->setPendingRelayChannels(m_command->pendingRelayChannels(data.node));
}

void MonitorPage::onRelayCommandRequested(const DeviceNode &node, const QString &channel,
                                          bool on, const QVariantMap &channels)
{
    if (!m_command || node.deviceType != QStringLiteral("relay")) {
        return;
    }
    m_command->sendRelayCommand(node, channel, on, variantMapToRelayStates(channels));
}
