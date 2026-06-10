#include "SystemSettingPage.h"
#include <QVBoxLayout>
#include <QLabel>

SystemSettingPage::SystemSettingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("系统设置"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *placeholder = new QLabel(QStringLiteral("Pc_ui 当前通过 IPC 接收 Pc_data 数据。"), this);
    layout->addWidget(placeholder);
    layout->addStretch();
}
