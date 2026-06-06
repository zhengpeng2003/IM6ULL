#include "BottomNavBar.h"

BottomNavBar::BottomNavBar(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    setFixedHeight(34);
}

void BottomNavBar::initUI()
{
    QPushButton *btnHome = new QPushButton("Home", this);
    QPushButton *btnTrend = new QPushButton("Trend", this);
    QPushButton *btnSet = new QPushButton("Setting", this);
    QPushButton *btnInfo = new QPushButton("Info", this);

    btnHome->setObjectName("NavButton");
    btnTrend->setObjectName("NavButton");
    btnSet->setObjectName("NavButton");
    btnInfo->setObjectName("NavButton");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(btnHome);
    layout->addWidget(btnTrend);
    layout->addWidget(btnSet);
    layout->addWidget(btnInfo);

    connect(btnHome, &QPushButton::clicked, this, [=]() {
        emit sigPageChanged(0);
    });
    connect(btnTrend, &QPushButton::clicked, this, [=]() {
        emit sigPageChanged(1);
    });
    connect(btnSet, &QPushButton::clicked, this, [=]() {
        emit sigPageChanged(2);
    });
    connect(btnInfo, &QPushButton::clicked, this, [=]() {
        emit sigPageChanged(3);
    });
}
