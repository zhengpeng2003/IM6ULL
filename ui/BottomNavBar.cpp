#include "BottomNavBar.h"
BottomNavBar::BottomNavBar(QWidget *parent)
    : QWidget(parent)   // ✅ 正确：直接父类是 QWidget
{
    initUI();
    setFixedHeight(48);
}

void BottomNavBar::initUI()
{
    QPushButton *btnHome = new QPushButton("主页", this);
    QPushButton *btnStat = new QPushButton("温度湿度曲线", this);
    QPushButton *btnSet  = new QPushButton("设置", this);
    QPushButton *btnInfo = new QPushButton("关于", this);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(btnHome);
    layout->addWidget(btnStat);
    layout->addWidget(btnSet);
    layout->addWidget(btnInfo);

    connect(btnHome, &QPushButton::clicked, this, [=](){
        emit sigPageChanged(0);
    });
    connect(btnStat, &QPushButton::clicked, this, [=](){
        emit sigPageChanged(1);
    });
    connect(btnSet,  &QPushButton::clicked, this, [=](){
        emit sigPageChanged(2);
    });
    connect(btnInfo, &QPushButton::clicked, this, [=](){
        emit sigPageChanged(3);
    });
}


