#include "BottomNavBar.h"

BottomNavBar::BottomNavBar(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    setFixedHeight(48);
}

void BottomNavBar::initUI()
{
    QPushButton *btnHome = new QPushButton("主页", this);
    QPushButton *btnTrend = new QPushButton("趋势", this);
    QPushButton *btnSet = new QPushButton("设置", this);
    QPushButton *btnInfo = new QPushButton("信息", this);

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
