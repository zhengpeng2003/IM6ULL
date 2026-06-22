#include "DeviceDetailCardBaseUi.h"

#include <QDateTime>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>
#include <utility>

namespace {

QString displayTime(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return QStringLiteral("-");
    }

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString fallbackText(const QString &text, const QString &fallback = QStringLiteral("-"))
{
    return text.isEmpty() ? fallback : text;
}

QString onlineText(const RealtimeDeviceData &data)
{
    if (data.serviceOffline) {
        return QStringLiteral("服务离线");
    }

    return data.node.online ? QStringLiteral("在线") : QStringLiteral("离线");
}

} // namespace

DeviceDetailCardBaseUi::DeviceDetailCardBaseUi(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DeviceDetailCardBaseUi");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(14);

    m_messageLabel = new QLabel(this);
    m_messageLabel->setObjectName("DeviceMessageLabel");
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    root->addWidget(m_messageLabel);

    m_infoCard = new QWidget(this);
    m_infoCard->setObjectName("DeviceInfoCard");
    auto *infoRoot = new QVBoxLayout(m_infoCard);
    infoRoot->setContentsMargins(14, 12, 14, 14);
    infoRoot->setSpacing(10);

    auto *title = new QLabel(QStringLiteral("设备信息"), m_infoCard);
    title->setObjectName("DeviceInfoTitle");
    infoRoot->addWidget(title);

    m_infoLayout = new QGridLayout;
    m_infoLayout->setHorizontalSpacing(16);
    m_infoLayout->setVerticalSpacing(8);
    infoRoot->addLayout(m_infoLayout);

    const QStringList keys = {
        QStringLiteral("factory"),
        QStringLiteral("area"),
        QStringLiteral("gateway"),
        QStringLiteral("master"),
        QStringLiteral("slave"),
        QStringLiteral("name"),
        QStringLiteral("type"),
        QStringLiteral("online"),
        QStringLiteral("time"),
        QStringLiteral("dataState"),
        QStringLiteral("parseState"),
        QStringLiteral("level"),
        QStringLiteral("valid"),
        QStringLiteral("reason")
    };
    const QStringList titles = {
        QStringLiteral("工厂"),
        QStringLiteral("厂房"),
        QStringLiteral("网关"),
        QStringLiteral("主站"),
        QStringLiteral("从站地址"),
        QStringLiteral("设备名称"),
        QStringLiteral("设备类型"),
        QStringLiteral("在线状态"),
        QStringLiteral("更新时间"),
        QStringLiteral("数据状态"),
        QStringLiteral("解析状态"),
        QStringLiteral("状态等级"),
        QStringLiteral("数据有效"),
        QStringLiteral("异常原因")
    };

    for (int i = 0; i < keys.size(); ++i) {
        addInfoRow(i, keys.at(i), titles.at(i));
    }

    root->addWidget(m_infoCard);

    m_contentLayout = new QVBoxLayout;
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(12);
    root->addLayout(m_contentLayout);
    root->addStretch();

    setMessage(QStringLiteral("请选择左侧设备"));
}

void DeviceDetailCardBaseUi::setDeviceData(const RealtimeDeviceData &data)
{
    m_messageLabel->hide();
    m_infoCard->show();

    const DeviceNode &node = data.node;
    setInfoValue(QStringLiteral("factory"), fallbackText(node.factoryName.isEmpty() ? node.factoryId : node.factoryName));
    setInfoValue(QStringLiteral("area"), fallbackText(node.areaName.isEmpty() ? node.areaId : node.areaName));
    setInfoValue(QStringLiteral("gateway"), fallbackText(node.gatewayName.isEmpty() ? node.gatewayId : node.gatewayName));
    setInfoValue(QStringLiteral("master"), QStringLiteral("RS485-%1 %2").arg(node.masterSlot + 1).arg(fallbackText(node.masterName)));
    setInfoValue(QStringLiteral("slave"), node.slaveAddr > 0 ? QString::number(node.slaveAddr) : QStringLiteral("-"));
    setInfoValue(QStringLiteral("name"), fallbackText(node.deviceName));
    setInfoValue(QStringLiteral("type"), fallbackText(node.deviceType, QStringLiteral("unknown")));
    setInfoValue(QStringLiteral("online"), onlineText(data));
    setInfoValue(QStringLiteral("time"), displayTime(data.timestamp > 0 ? data.timestamp : node.lastUpdateTime));
    setInfoValue(QStringLiteral("dataState"), fallbackText(data.dataState, QStringLiteral("未知")));
    setInfoValue(QStringLiteral("parseState"), fallbackText(data.statusText, QStringLiteral("未知")));
    setInfoValue(QStringLiteral("level"), fallbackText(data.statusLevel, QStringLiteral("unknown")));
    setInfoValue(QStringLiteral("valid"), data.valid ? QStringLiteral("是") : QStringLiteral("否"));
    setInfoValue(QStringLiteral("reason"), fallbackText(data.errorMessage));
}

void DeviceDetailCardBaseUi::setMessage(const QString &message)
{
    clearInfoValues();
    m_infoCard->hide();
    m_messageLabel->setText(message);
    m_messageLabel->show();
}

QVBoxLayout *DeviceDetailCardBaseUi::contentLayout() const
{
    return m_contentLayout;
}

void DeviceDetailCardBaseUi::addInfoRow(int row, const QString &key, const QString &title)
{
    auto *titleLabel = new QLabel(title, m_infoCard);
    titleLabel->setObjectName("DeviceInfoTitle");

    auto *valueLabel = new QLabel(QStringLiteral("-"), m_infoCard);
    valueLabel->setObjectName("DeviceInfoValue");
    valueLabel->setWordWrap(true);

    m_infoLayout->addWidget(titleLabel, row, 0, Qt::AlignTop | Qt::AlignLeft);
    m_infoLayout->addWidget(valueLabel, row, 1);
    m_valueLabels.insert(key, valueLabel);
}

void DeviceDetailCardBaseUi::setInfoValue(const QString &key, const QString &value)
{
    if (QLabel *label = m_valueLabels.value(key, nullptr)) {
        label->setText(value);
    }
}

void DeviceDetailCardBaseUi::clearInfoValues()
{
    for (QLabel *label : std::as_const(m_valueLabels)) {
        label->setText(QStringLiteral("-"));
    }
}
