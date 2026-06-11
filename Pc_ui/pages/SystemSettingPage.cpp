#include "SystemSettingPage.h"

#include <QComboBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QString statusDisplayText(const QString &status)
{
    if (status == QStringLiteral("connected")) {
        return QStringLiteral("连接正常");
    }
    if (status == QStringLiteral("connecting")) {
        return QStringLiteral("连接中");
    }
    if (status == QStringLiteral("failed")) {
        return QStringLiteral("连接失败");
    }
    if (status == QStringLiteral("disconnected")) {
        return QStringLiteral("未连接");
    }
    return status.isEmpty() ? QStringLiteral("未知") : status;
}

bool statusIsOk(const QString &status)
{
    return status == QStringLiteral("connected") || status == QStringLiteral("connecting");
}

} // namespace

SystemSettingPage::SystemSettingPage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 14, 18, 18);
    layout->setSpacing(14);

    auto *title = new QLabel(QStringLiteral("6. 系统设置界面"), this);
    title->setObjectName("PageTitle");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto *cards = new QGridLayout;
    cards->setContentsMargins(0, 0, 0, 0);
    cards->setHorizontalSpacing(8);
    cards->setVerticalSpacing(10);
    cards->addWidget(createMqttCard(), 0, 0);
    cards->addWidget(createDatabaseCard(), 0, 1);
    cards->addWidget(createCollectCard(), 0, 2);
    cards->addWidget(createAboutCard(), 0, 3);

    for (int i = 0; i < 4; ++i) {
        cards->setColumnStretch(i, 1);
    }

    layout->addLayout(cards, 1);

    setIpcConnected(false);
    setStatusText(QStringLiteral("等待后端"), false);
}

void SystemSettingPage::setIpcConnected(bool connected)
{
    m_ipcConnected = connected;
    if (m_testButton) {
        m_testButton->setEnabled(connected);
    }
    if (m_saveButton) {
        m_saveButton->setEnabled(connected);
    }

    if (connected) {
        emit mqttConfigRequested();
    } else {
        setStatusText(QStringLiteral("后端未连接"), false);
    }
}

void SystemSettingPage::onMqttConfigMessage(const QJsonObject &obj)
{
    const QString host = obj.value(QStringLiteral("host")).toString();
    const int port = obj.value(QStringLiteral("port")).toInt(1883);
    const QString status = obj.value(QStringLiteral("status")).toString();

    if (!host.isEmpty() && m_hostEdit) {
        m_hostEdit->setText(host);
    }
    if (m_portEdit) {
        m_portEdit->setText(QString::number(port));
    }

    setStatusText(statusDisplayText(status), statusIsOk(status));
}

void SystemSettingPage::onMqttConfigAck(const QJsonObject &obj)
{
    const bool ok = obj.value(QStringLiteral("ok")).toBool();
    const QString reason = obj.value(QStringLiteral("reason")).toString();

    onMqttConfigMessage(obj);

    if (!ok && !reason.isEmpty()) {
        setStatusText(reason, false);
    }
}

QWidget *SystemSettingPage::createCard(const QString &title, QWidget *content)
{
    auto *card = new QWidget(this);
    card->setObjectName(QStringLiteral("SystemSettingCard"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 14);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("SystemSettingCardTitle"));
    layout->addWidget(titleLabel);
    layout->addWidget(content, 1);

    return card;
}

QWidget *SystemSettingPage::createMqttCard()
{
    auto *content = new QWidget(this);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto *form = new QGridLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(10);
    form->setColumnStretch(0, 0);
    form->setColumnStretch(1, 1);

    m_hostEdit = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    m_hostEdit->setPlaceholderText(QStringLiteral("127.0.0.1"));
    m_hostEdit->setMinimumWidth(96);

    m_portEdit = new QLineEdit(QStringLiteral("1883"), this);
    m_portEdit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{1,5}")), m_portEdit));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("SystemSettingStatus"));

    form->addWidget(createFieldLabel(QStringLiteral("Broker 地址:")), 0, 0);
    form->addWidget(m_hostEdit, 0, 1);
    form->addWidget(createFieldLabel(QStringLiteral("端口:")), 1, 0);
    form->addWidget(m_portEdit, 1, 1);
    form->addWidget(createFieldLabel(QStringLiteral("状态:")), 2, 0);
    form->addWidget(m_statusLabel, 2, 1);
    layout->addLayout(form);

    auto *actions = new QHBoxLayout;
    actions->setContentsMargins(0, 14, 0, 0);
    actions->setSpacing(8);

    m_testButton = new QPushButton(QStringLiteral("连接测试"), this);
    m_testButton->setObjectName(QStringLiteral("SystemSettingPrimaryButton"));
    m_saveButton = new QPushButton(QStringLiteral("保存配置"), this);
    m_saveButton->setObjectName(QStringLiteral("SystemSettingSecondaryButton"));

    actions->addWidget(m_testButton);
    actions->addWidget(m_saveButton);
    actions->addStretch();
    layout->addLayout(actions);
    layout->addStretch();

    connect(m_testButton, &QPushButton::clicked, this, [this]() {
        if (!m_ipcConnected) {
            setStatusText(QStringLiteral("后端未连接"), false);
            return;
        }
        setStatusText(QStringLiteral("正在查询"), true);
        emit mqttConfigRequested();
    });

    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        QString host;
        int port = 0;
        if (!validateInput(host, port)) {
            return;
        }
        setStatusText(QStringLiteral("保存中"), true);
        emit mqttConfigSaveRequested(host, port);
    });

    return createCard(QStringLiteral("MQTT 设置"), content);
}

QWidget *SystemSettingPage::createDatabaseCard()
{
    auto *content = new QWidget(this);
    auto *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(10);

    auto *typeCombo = new QComboBox(this);
    typeCombo->addItem(QStringLiteral("SQLite"));
    typeCombo->setEnabled(false);

    auto *pathEdit = new QLineEdit(QStringLiteral("./db/pc_data.db"), this);
    pathEdit->setReadOnly(true);

    auto *openButton = new QPushButton(QStringLiteral("打开目录"), this);
    openButton->setObjectName(QStringLiteral("SystemSettingOutlineButton"));
    openButton->setEnabled(false);
    auto *backupButton = new QPushButton(QStringLiteral("备份历史数据"), this);
    backupButton->setObjectName(QStringLiteral("SystemSettingOutlineButton"));
    backupButton->setEnabled(false);

    layout->addWidget(createFieldLabel(QStringLiteral("数据库类型:")), 0, 0);
    layout->addWidget(typeCombo, 0, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("数据库文件:")), 1, 0);
    layout->addWidget(pathEdit, 1, 1);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(8);
    actions->addWidget(openButton);
    actions->addWidget(backupButton);
    actions->addStretch();
    layout->addLayout(actions, 2, 0, 1, 2);
    layout->setRowStretch(3, 1);

    return createCard(QStringLiteral("数据库设置"), content);
}

QWidget *SystemSettingPage::createCollectCard()
{
    auto *content = new QWidget(this);
    auto *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(10);

    auto *interval = new QLineEdit(QStringLiteral("1000"), this);
    interval->setReadOnly(true);

    auto *theme = new QComboBox(this);
    theme->addItem(QStringLiteral("默认"));
    theme->setEnabled(false);

    auto *logLevel = new QComboBox(this);
    logLevel->addItem(QStringLiteral("信息"));
    logLevel->setEnabled(false);

    auto *autoSave = new QCheckBox(this);
    autoSave->setChecked(true);
    autoSave->setEnabled(false);

    auto *save = new QPushButton(QStringLiteral("保存设置"), this);
    save->setObjectName(QStringLiteral("SystemSettingWideButton"));
    save->setEnabled(false);

    layout->addWidget(createFieldLabel(QStringLiteral("刷新间隔:")), 0, 0);
    auto *intervalBox = new QHBoxLayout;
    intervalBox->addWidget(interval);
    intervalBox->addWidget(createFieldLabel(QStringLiteral("ms")));
    layout->addLayout(intervalBox, 0, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("主题样式:")), 1, 0);
    layout->addWidget(theme, 1, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("日志等级:")), 2, 0);
    layout->addWidget(logLevel, 2, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("自动保存数据:")), 3, 0);
    layout->addWidget(autoSave, 3, 1);
    layout->addWidget(save, 4, 0, 1, 2);
    layout->setRowStretch(5, 1);

    return createCard(QStringLiteral("采集设置"), content);
}

QWidget *SystemSettingPage::createAboutCard()
{
    auto *content = new QWidget(this);
    auto *layout = new QGridLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setHorizontalSpacing(10);
    layout->setVerticalSpacing(12);

    layout->addWidget(createFieldLabel(QStringLiteral("版本:")), 0, 0);
    layout->addWidget(new QLabel(QStringLiteral("v1.0.0"), this), 0, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("开发者:")), 1, 0);
    layout->addWidget(new QLabel(QStringLiteral("IM6ULL Team"), this), 1, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("构建时间:")), 2, 0);
    layout->addWidget(new QLabel(QStringLiteral("2025-06-01"), this), 2, 1);
    layout->addWidget(createFieldLabel(QStringLiteral("运行环境:")), 3, 0);
    layout->addWidget(new QLabel(QStringLiteral("Windows"), this), 3, 1);
    layout->addWidget(new QLabel(QStringLiteral("Qt 5.15.2"), this), 4, 1);
    layout->setRowStretch(5, 1);

    return createCard(QStringLiteral("关于"), content);
}

QLabel *SystemSettingPage::createFieldLabel(const QString &text)
{
    auto *label = new QLabel(text, this);
    label->setObjectName(QStringLiteral("SystemSettingFieldLabel"));
    return label;
}

void SystemSettingPage::setStatusText(const QString &text, bool ok)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setText(text);
    m_statusLabel->setProperty("ok", ok);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}

bool SystemSettingPage::validateInput(QString &host, int &port) const
{
    host = m_hostEdit ? m_hostEdit->text().trimmed() : QString();
    bool ok = false;
    port = m_portEdit ? m_portEdit->text().toInt(&ok) : 0;

    if (host.isEmpty()) {
        const_cast<SystemSettingPage *>(this)->setStatusText(QStringLiteral("地址不能为空"), false);
        return false;
    }

    if (!ok || port < 1 || port > 65535) {
        const_cast<SystemSettingPage *>(this)->setStatusText(QStringLiteral("端口无效"), false);
        return false;
    }

    if (!m_ipcConnected) {
        const_cast<SystemSettingPage *>(this)->setStatusText(QStringLiteral("后端未连接"), false);
        return false;
    }

    return true;
}
