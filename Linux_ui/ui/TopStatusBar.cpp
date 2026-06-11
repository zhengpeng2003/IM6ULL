#include "TopStatusBar.h"

#include <QStyle>
#include <QTime>

TopStatusBar::TopStatusBar(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    initSignal();
}

void TopStatusBar::setBackendConnected(bool connected)
{
    titleLabel->setText(connected
        ? "工业物联网终端"
        : "工业物联网终端 - IPC未连接");
    statusDot->setObjectName(connected ? "StatusGreen" : "StatusGray");
    statusDot->style()->unpolish(statusDot);
    statusDot->style()->polish(statusDot);
}

void TopStatusBar::initUI()
{
    setFixedHeight(28);
    setObjectName("TopStatusBar");

    titleLabel = new QLabel("工业物联网终端");
    timeLabel = new QLabel("00:00");
    statusDot = new QLabel;

    statusDot->setFixedSize(10, 10);
    statusDot->setObjectName("StatusGreen");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(6);

    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(timeLabel);
    layout->addWidget(statusDot);

    setBackendConnected(true);
}

void TopStatusBar::initSignal()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        timeLabel->setText(QTime::currentTime().toString("HH:mm"));
    });
    timer->start(1000);
}
