#include "TopBar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QStyle>

TopBar::TopBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("TopBar");
    setFixedHeight(56);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(18, 0, 18, 0);
    layout->setSpacing(18);

    auto *title = new QLabel(QStringLiteral("Pc_mqtt 工业物联网监控平台"), this);
    title->setObjectName("TopTitle");

    m_gatewayLabel = new QLabel(QStringLiteral("在线网关: 0"), this);
    m_deviceLabel = new QLabel(QStringLiteral("在线设备: 0"), this);
    m_alarmLabel = new QLabel(QStringLiteral("报警: 0"), this);
    m_serviceLabel = new QLabel(QStringLiteral("Pc_data 服务离线"), this);
    m_timeLabel = new QLabel(this);

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(m_gatewayLabel);
    layout->addWidget(m_deviceLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_serviceLabel);
    layout->addWidget(m_timeLabel);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TopBar::updateCurrentTime);
    m_timer->start(1000);
    updateCurrentTime();
}

void TopBar::setOnlineGatewayCount(int count)
{
    if (m_onlineGatewayCount == count) {
        return;
    }
    m_onlineGatewayCount = count;
    m_gatewayLabel->setText(QStringLiteral("在线网关: %1").arg(count));
}

void TopBar::setOnlineDeviceCount(int count)
{
    if (m_onlineDeviceCount == count) {
        return;
    }
    m_onlineDeviceCount = count;
    m_deviceLabel->setText(QStringLiteral("在线设备: %1").arg(count));
}

void TopBar::setAlarmCount(int count)
{
    if (m_alarmCount == count) {
        return;
    }
    m_alarmCount = count;
    m_alarmLabel->setText(QStringLiteral("报警: %1").arg(count));
}

void TopBar::setServiceOnline(bool online)
{
    if (m_serviceOnline == online) {
        return;
    }
    m_serviceOnline = online;
    m_serviceLabel->setText(online ? QStringLiteral("Pc_data 服务在线")
                                   : QStringLiteral("Pc_data 服务离线"));
}

void TopBar::updateCurrentTime()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}
