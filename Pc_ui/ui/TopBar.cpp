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
    m_timeLabel = new QLabel(this);

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(m_gatewayLabel);
    layout->addWidget(m_deviceLabel);
    layout->addWidget(m_alarmLabel);
    layout->addWidget(m_timeLabel);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &TopBar::updateCurrentTime);
    m_timer->start(1000);
    updateCurrentTime();
}

void TopBar::setOnlineGatewayCount(int count)
{
    m_gatewayLabel->setText(QStringLiteral("在线网关: %1").arg(count));
}

void TopBar::setOnlineDeviceCount(int count)
{
    m_deviceLabel->setText(QStringLiteral("在线设备: %1").arg(count));
}

void TopBar::setAlarmCount(int count)
{
    m_alarmLabel->setText(QStringLiteral("报警: %1").arg(count));
}

void TopBar::updateCurrentTime()
{
    m_timeLabel->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
}
