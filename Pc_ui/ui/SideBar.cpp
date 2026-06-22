#include "SideBar.h"
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QStringList>
#include <QVBoxLayout>

SideBar::SideBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("SideBar");

    // 关键：让自定义 QWidget 支持 QSS 背景绘制
    setAttribute(Qt::WA_StyledBackground, true);

    setFixedWidth(190);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 16, 10, 18);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("工业物联网\n监控平台"), this);
    title->setObjectName("SideTitle");
    title->setWordWrap(true);
    layout->addWidget(title);
    layout->addSpacing(10);

    QStringList names = {
        QStringLiteral("首页总览"),
        QStringLiteral("实时监控"),
        QStringLiteral("趋势分析"),
        QStringLiteral("设备配置"),
        QStringLiteral("报警日志"),
        QStringLiteral("系统设置")
    };

    QStringList icons = {
        QStringLiteral(":/images/home.png"),
        QStringLiteral(":/images/monitor.png"),
        QStringLiteral(":/images/trend.png"),
        QStringLiteral(":/images/device.png"),
        QStringLiteral(":/images/alarm.png"),
        QStringLiteral(":/images/setting.png")
    };

    for (int i = 0; i < names.size(); ++i) {
        auto *btn = createButton(names[i], icons[i], i);
        layout->addWidget(btn);
    }

    layout->addStretch();

    if (!m_buttons.isEmpty()) {
        m_buttons.first()->setChecked(true);
    }
}

QPushButton *SideBar::createButton(const QString &text, const QString &iconPath, int index)
{
    auto *btn = new QPushButton(QIcon(iconPath), text, this);
    btn->setCheckable(true);
    btn->setFixedHeight(40);
    btn->setIconSize(QSize(20, 20));
    btn->setObjectName("SideButton");
    m_buttons.append(btn);

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        for (auto *b : m_buttons) b->setChecked(false);
        m_buttons[index]->setChecked(true);
        emit pageChanged(index);
    });
    return btn;
}
