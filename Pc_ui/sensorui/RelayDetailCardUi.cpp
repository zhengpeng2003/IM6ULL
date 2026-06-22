#include "RelayDetailCardUi.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

namespace {

bool isRelayPoint(const TelemetryPointData &point)
{
    return point.pointKey.startsWith(QStringLiteral("relay_")) ||
           point.pointKey.startsWith(QStringLiteral("relay.ch"));
}

QString commandChannelKey(const QString &channel)
{
    static const QRegularExpression relayDotChannel(QStringLiteral("^relay\\.ch(\\d+)$"));
    const QRegularExpressionMatch match = relayDotChannel.match(channel);
    if (match.hasMatch()) {
        return QStringLiteral("relay_%1").arg(match.captured(1));
    }

    return channel;
}

QString displayChannelName(const QString &channel, const QString &pointName)
{
    if (!pointName.isEmpty()) {
        return pointName;
    }

    static const QRegularExpression relayNumber(QStringLiteral("(\\d+)$"));
    const QRegularExpressionMatch match = relayNumber.match(channel);
    if (match.hasMatch()) {
        return QStringLiteral("通道 %1").arg(match.captured(1));
    }

    return channel;
}

} // namespace

RelayDetailCardUi::RelayDetailCardUi(QWidget *parent)
    : DeviceDetailCardBaseUi(parent)
{
    setObjectName("RelayDetailCardUi");

    m_channelCard = new QWidget(this);
    m_channelCard->setObjectName("MetricCard");
    auto *root = new QVBoxLayout(m_channelCard);
    root->setContentsMargins(14, 12, 14, 14);
    root->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("继电器通道"), m_channelCard);
    title->setObjectName("MetricTitle");
    root->addWidget(title);

    m_channelLayout = new QVBoxLayout;
    m_channelLayout->setContentsMargins(0, 0, 0, 0);
    m_channelLayout->setSpacing(8);
    root->addLayout(m_channelLayout);

    contentLayout()->addWidget(m_channelCard);
}

void RelayDetailCardUi::setDeviceData(const RealtimeDeviceData &data)
{
    DeviceDetailCardBaseUi::setDeviceData(data);
    m_data = data;
    clearRows();

    QMap<QString, bool> channels = data.relay.channels;
    if (!channels.isEmpty()) {
        QMap<QString, bool> normalized;
        for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
            normalized.insert(commandChannelKey(it.key()), it.value());
        }
        channels = normalized;
    }
    if (channels.isEmpty()) {
        for (const TelemetryPointData &point : data.points) {
            if (!isRelayPoint(point) || !point.valid) {
                continue;
            }
            channels.insert(commandChannelKey(point.pointKey), point.numberValue != 0.0);
        }
    }

    const QMap<QString, QString> channelNames = pointNamesByChannel(data);
    const QVariantMap channelVariants = channelsToVariantMap(channels);
    const bool controlsEnabled = data.node.deviceType == QStringLiteral("relay") &&
                                 !data.serviceOffline &&
                                 data.node.online &&
                                 data.dataState == QStringLiteral("normal");

    if (channels.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无继电器通道数据"), m_channelCard);
        empty->setObjectName("MetricValue");
        m_channelLayout->addWidget(empty);
        return;
    }

    for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
        const QString channel = it.key();
        const bool on = it.value();
        const bool pending = m_pendingChannels.contains(channel);

        auto *row = new QWidget(m_channelCard);
        row->setObjectName("RelayChannelRow");
        auto *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(10);

        auto *name = new QLabel(displayChannelName(channel, channelNames.value(channel)), row);
        name->setObjectName("RelayChannelName");
        name->setMinimumWidth(120);

        auto *state = new QLabel(on ? QStringLiteral("ON") : QStringLiteral("OFF"), row);
        state->setObjectName("RelayChannelState");
        state->setMinimumWidth(64);

        auto *openButton = new QPushButton(QStringLiteral("开启"), row);
        openButton->setObjectName("RelayControlButton");
        openButton->setEnabled(controlsEnabled && !on && !pending);

        auto *closeButton = new QPushButton(QStringLiteral("关闭"), row);
        closeButton->setObjectName("RelayControlButton");
        closeButton->setEnabled(controlsEnabled && on && !pending);

        if (pending) {
            const QString pendingText = QStringLiteral("该继电器通道命令正在执行，请等待回包");
            openButton->setToolTip(pendingText);
            closeButton->setToolTip(pendingText);
            state->setText(QStringLiteral("执行中"));
        }

        connect(openButton, &QPushButton::clicked, this, [this, channel, channelVariants]() {
            emit relayCommandRequested(m_data.node, channel, true, channelVariants);
        });
        connect(closeButton, &QPushButton::clicked, this, [this, channel, channelVariants]() {
            emit relayCommandRequested(m_data.node, channel, false, channelVariants);
        });

        rowLayout->addWidget(name, 1);
        rowLayout->addWidget(state);
        rowLayout->addWidget(openButton);
        rowLayout->addWidget(closeButton);
        m_channelLayout->addWidget(row);
    }
}

void RelayDetailCardUi::setPendingRelayChannels(const QSet<QString> &channels)
{
    if (m_pendingChannels == channels) {
        return;
    }
    m_pendingChannels = channels;
    if (!m_data.node.gatewayId.isEmpty()) {
        setDeviceData(m_data);
    }
}

void RelayDetailCardUi::clearRows()
{
    while (QLayoutItem *item = m_channelLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

QVariantMap RelayDetailCardUi::channelsToVariantMap(const QMap<QString, bool> &channels) const
{
    QVariantMap result;
    for (auto it = channels.cbegin(); it != channels.cend(); ++it) {
        result.insert(it.key(), it.value());
    }
    return result;
}

QMap<QString, QString> RelayDetailCardUi::pointNamesByChannel(const RealtimeDeviceData &data) const
{
    QMap<QString, QString> result;
    for (const TelemetryPointData &point : data.points) {
        if (isRelayPoint(point)) {
            result.insert(commandChannelKey(point.pointKey), point.pointName);
        }
    }
    return result;
}
