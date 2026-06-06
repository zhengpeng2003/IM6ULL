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
    setFixedHeight(28);
    setObjectName("TopStatusBar");

    titleLabel = new QLabel("Industrial HMI");
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
}

void TopStatusBar::initSignal()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        timeLabel->setText(QTime::currentTime().toString("HH:mm"));
    });
    timer->start(1000);
}
