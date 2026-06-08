#include "SideBar.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QButtonGroup>

SideBar::SideBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("SideBar");
    setFixedWidth(180);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 18, 10, 18);
    layout->setSpacing(8);

    QStringList names = {
        QStringLiteral("首页总览"),
        QStringLiteral("实时监控"),
        QStringLiteral("趋势分析"),
        QStringLiteral("设备配置"),
        QStringLiteral("报警日志"),
        QStringLiteral("系统设置")
    };

    for (int i = 0; i < names.size(); ++i) {
        auto *btn = createButton(names[i], i);
        layout->addWidget(btn);
    }
    layout->addStretch();
    if (!m_buttons.isEmpty()) {
        m_buttons.first()->setChecked(true);
    }
}

QPushButton *SideBar::createButton(const QString &text, int index)
{
    auto *btn = new QPushButton(text, this);
    btn->setCheckable(true);
    btn->setMinimumHeight(42);
    btn->setObjectName("SideButton");
    m_buttons.append(btn);

    connect(btn, &QPushButton::clicked, this, [this, index]() {
        for (auto *b : m_buttons) b->setChecked(false);
        m_buttons[index]->setChecked(true);
        emit pageChanged(index);
    });
    return btn;
}
