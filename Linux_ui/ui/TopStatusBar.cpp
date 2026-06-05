#include "TopStatusBar.h"
#include <QTime>

TopStatusBar::TopStatusBar(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    initSignal();
}

void TopStatusBar::initUI()
{
    setFixedHeight(40);
    setObjectName("TopStatusBar");

    titleLabel = new QLabel("系统 1.0");
    timeLabel  = new QLabel("00:00");
    statusDot  = new QLabel;

    statusDot->setFixedSize(10, 10);
    statusDot->setObjectName("StatusGreen");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 0, 10, 0);

    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(timeLabel);
    layout->addSpacing(10);
    layout->addWidget(statusDot);
}

void TopStatusBar::initSignal()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        timeLabel->setText(QTime::currentTime().toString("HH:mm"));
    });
    timer->start(1000);
}

