#include "TopBar.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>
#include <QDateTime>
#include <QStyle>

TopBar::TopBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("TopBar");
    setFixedHeight(52);
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(20, 0, 20, 0);
    layout->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("运行监控中心"), this);
    title->setObjectName("TopTitle");

    m_gatewayLabel = new QLabel(QStringLiteral("在线网关: 0"), this);
    m_deviceLabel = new QLabel(QStringLiteral("在线设备: 0"), this);
    m_alarmLabel = new QLabel(QStringLiteral("报警: 0"), this);

    m_timeLabel = new QLabel(this);
    m_userLabel = new QLabel(QStringLiteral("管理员"), this);

    m_gatewayLabel->setObjectName("TopMetric");
    m_deviceLabel->setObjectName("TopMetric");
    m_alarmLabel->setObjectName("TopAlarmMetric");

    m_timeLabel->setObjectName("TopMetric");
    m_userLabel->setObjectName("TopUser");


    layout->addWidget(title);
    layout->addStretch();

    layout->addWidget(m_gatewayLabel);
    layout->addWidget(m_deviceLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_timeLabel);
    layout->addWidget(m_userLabel);

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

}

void TopBar::updateCurrentTime()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}
