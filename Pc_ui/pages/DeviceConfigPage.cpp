#include "DeviceConfigPage.h"
#include "ui/DeviceTreeWidget.h"
#include "core/DeviceManager.h"
#include "core/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

DeviceConfigPage::DeviceConfigPage(DeviceManager *device, ConfigManager *config, QWidget *parent)
    : QWidget(parent), m_device(device), m_config(config)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("设备配置"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *body = new QHBoxLayout;
    m_tree = new DeviceTreeWidget(this);
    m_tree->setDevices(m_device->allDevices());
    m_detail = new QLabel(QStringLiteral("选择左侧节点后显示配置详情"), this);
    m_detail->setObjectName("DetailPanel");
    m_detail->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    body->addWidget(m_tree, 1);
    body->addWidget(m_detail, 2);
    layout->addLayout(body, 1);

    auto *buttons = new QHBoxLayout;
    for (const auto &name : {QStringLiteral("添加厂房"), QStringLiteral("添加网关"), QStringLiteral("扫描端口"), QStringLiteral("添加主站"), QStringLiteral("添加从站"), QStringLiteral("保存配置")}) {
        buttons->addWidget(new QPushButton(name, this));
    }
    buttons->addStretch();
    layout->addLayout(buttons);
}
