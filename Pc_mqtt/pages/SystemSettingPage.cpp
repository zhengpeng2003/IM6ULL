#include "SystemSettingPage.h"
#include "core/ConfigManager.h"
#include "core/MqttClientManager.h"
#include "model/ConfigModel.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>

SystemSettingPage::SystemSettingPage(ConfigManager *config, MqttClientManager *mqtt, QWidget *parent)
    : QWidget(parent), m_config(config), m_mqtt(mqtt)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("系统设置"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    const auto cfg = m_config->loadMqttConfig();
    auto *form = new QFormLayout;
    m_hostEdit = new QLineEdit(cfg.host, this);
    m_portEdit = new QLineEdit(QString::number(cfg.port), this);
    m_userEdit = new QLineEdit(cfg.username, this);
    m_passwordEdit = new QLineEdit(cfg.password, this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_clientIdEdit = new QLineEdit(cfg.clientId, this);
    m_autoConnect = new QCheckBox(QStringLiteral("启动时自动连接 MQTT"), this);
    m_autoReconnect = new QCheckBox(QStringLiteral("断线自动重连"), this);
    m_autoConnect->setChecked(cfg.autoConnect);
    m_autoReconnect->setChecked(cfg.autoReconnect);

    form->addRow(QStringLiteral("MQTT Broker:"), m_hostEdit);
    form->addRow(QStringLiteral("MQTT Port:"), m_portEdit);
    form->addRow(QStringLiteral("用户名:"), m_userEdit);
    form->addRow(QStringLiteral("密码:"), m_passwordEdit);
    form->addRow(QStringLiteral("Client ID:"), m_clientIdEdit);
    form->addRow(QString(), m_autoConnect);
    form->addRow(QString(), m_autoReconnect);
    layout->addLayout(form);

    auto *buttons = new QHBoxLayout;
    auto *save = new QPushButton(QStringLiteral("保存配置"), this);
    auto *test = new QPushButton(QStringLiteral("测试连接"), this);
    buttons->addWidget(save);
    buttons->addWidget(test);
    buttons->addStretch();
    layout->addLayout(buttons);
    layout->addStretch();

    connect(save, &QPushButton::clicked, this, &SystemSettingPage::saveConfig);
    connect(test, &QPushButton::clicked, this, &SystemSettingPage::testConnect);
}

void SystemSettingPage::saveConfig()
{
    MqttConfig cfg;
    cfg.host = m_hostEdit->text();
    cfg.port = m_portEdit->text().toInt();
    cfg.username = m_userEdit->text();
    cfg.password = m_passwordEdit->text();
    cfg.clientId = m_clientIdEdit->text();
    cfg.autoConnect = m_autoConnect->isChecked();
    cfg.autoReconnect = m_autoReconnect->isChecked();
    m_config->saveMqttConfig(cfg);
}

void SystemSettingPage::testConnect()
{
    saveConfig();
    m_mqtt->connectToBroker(m_config->loadMqttConfig());
}
