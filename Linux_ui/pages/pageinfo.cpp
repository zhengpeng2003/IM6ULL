#include "pageinfo.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

Pageinfo::Pageinfo(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void Pageinfo::initUI()
{
    setObjectName("PageArea");

    const QList<InfoCardSpec> cardSpecs = {
        {"os", "系统版本", "--", "normal"},
        {"kernel", "内核", "--", "normal"},
        {"arch", "架构", "--", "normal"},
        {"screen", "屏幕", "--", "normal"},
        {"ipc", "IPC状态", "--", "offline"},
        {"alarm", "最近告警", "--", "normal"},
    };

    const QList<RuntimeRowSpec> rowSpecs = {
        {"linux_data", "Linux_data", "待同步", "offline"},
        {"ipc", "IPC", "未连接", "offline"},
        {"last_sync", "最近同步", "--", "offline"},
        {"mqtt", "MQTT", "待接入", "normal"},
        {"log", "日志服务", "待接入", "normal"},
    };

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(6);

    QWidget *cardsWidget = new QWidget(this);
    cardsWidget->setObjectName("InfoCardsArea");

    QGridLayout *cardsLayout = new QGridLayout(cardsWidget);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setHorizontalSpacing(5);
    cardsLayout->setVerticalSpacing(5);

    for (int i = 0; i < cardSpecs.size(); ++i)
        cardsLayout->addWidget(createInfoCard(cardSpecs.at(i)), i / 2, i % 2);

    cardsLayout->setColumnStretch(0, 1);
    cardsLayout->setColumnStretch(1, 1);

    QFrame *runtimePanel = new QFrame(this);
    runtimePanel->setObjectName("RuntimePanel");
    runtimePanel->setMinimumWidth(150);

    hintLabel = new QLabel("运行信息", runtimePanel);
    hintLabel->setObjectName("PanelTitle");

    QVBoxLayout *runtimeLayout = new QVBoxLayout(runtimePanel);
    runtimeLayout->setContentsMargins(6, 5, 6, 5);
    runtimeLayout->setSpacing(3);
    runtimeLayout->addWidget(hintLabel);

    for (const RuntimeRowSpec &spec : rowSpecs)
        runtimeLayout->addWidget(createRuntimeRow(spec));

    runtimeLayout->addStretch();

    reconnectButton = new QPushButton("重新连接", runtimePanel);
    reconnectButton->setObjectName("RuntimeActionButton");
    reconnectButton->setFixedHeight(26);
    runtimeLayout->addWidget(reconnectButton);

    QLabel *cacheTitle = new QLabel("缓存数据库", runtimePanel);
    cacheTitle->setObjectName("PanelTitle");
    runtimeLayout->addWidget(cacheTitle);

    cacheStateLabel = new QLabel("缓存：关闭", runtimePanel);
    cacheStateLabel->setObjectName("RuntimeValue");
    flushStateLabel = new QLabel("发送：关闭", runtimePanel);
    flushStateLabel->setObjectName("RuntimeValue");
    pendingCountLabel = new QLabel("待发送：0", runtimePanel);
    pendingCountLabel->setObjectName("RuntimeValue");
    runtimeLayout->addWidget(cacheStateLabel);
    runtimeLayout->addWidget(flushStateLabel);
    runtimeLayout->addWidget(pendingCountLabel);

    QHBoxLayout *configRow = new QHBoxLayout;
    configRow->setContentsMargins(0, 0, 0, 0);
    configRow->setSpacing(4);
    cacheEnableButton = new QPushButton("缓存关闭", runtimePanel);
    cacheEnableButton->setObjectName("ToggleButton");
    cacheEnableButton->setCheckable(true);
    cacheEnableButton->setFixedHeight(23);
    saveCacheButton = new QPushButton("保存", runtimePanel);
    saveCacheButton->setObjectName("SmallActionButton");
    saveCacheButton->setFixedHeight(23);
    configRow->addWidget(cacheEnableButton, 1);
    configRow->addWidget(saveCacheButton);
    runtimeLayout->addLayout(configRow);

    QHBoxLayout *cacheActions = new QHBoxLayout;
    cacheActions->setContentsMargins(0, 0, 0, 0);
    cacheActions->setSpacing(4);
    refreshCacheButton = new QPushButton("刷新", runtimePanel);
    refreshCacheButton->setObjectName("SmallActionButton");
    refreshCacheButton->setFixedHeight(23);
    clearCacheButton = new QPushButton("清空", runtimePanel);
    clearCacheButton->setObjectName("DangerButton");
    clearCacheButton->setFixedHeight(23);
    flushCacheButton = new QPushButton("补发", runtimePanel);
    flushCacheButton->setObjectName("SmallActionButton");
    flushCacheButton->setFixedHeight(23);
    cacheActions->addWidget(refreshCacheButton);
    cacheActions->addWidget(clearCacheButton);
    cacheActions->addWidget(flushCacheButton);
    runtimeLayout->addLayout(cacheActions);

    mainLayout->addWidget(cardsWidget, 2);
    mainLayout->addWidget(runtimePanel, 1);

    updateIpcStatusLabel();

    connect(reconnectButton, &QPushButton::clicked,
            this, &Pageinfo::reconnectIpcRequested);
    connect(cacheEnableButton, &QPushButton::clicked, this, [this]() {
        m_cacheEnabled = cacheEnableButton->isChecked();
        if (!m_cacheEnabled)
            m_flushEnabled = false;
        updateOfflineCacheStatus(m_cacheEnabled, m_flushEnabled, m_pendingCount);
    });
    connect(saveCacheButton, &QPushButton::clicked, this, [this]() {
        emit offlineCacheConfigChanged(m_cacheEnabled, m_cacheEnabled ? m_flushEnabled : false);
    });
    connect(refreshCacheButton, &QPushButton::clicked,
            this, &Pageinfo::offlineCacheRefreshRequested);
    connect(clearCacheButton, &QPushButton::clicked, this, [this]() {
        if (QMessageBox::question(this,
                                  "清空缓存",
                                  "确认清空 MQTT 离线缓存？不会删除端口、从站和阈值配置。",
                                  QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) == QMessageBox::Yes) {
            emit clearOfflineCacheRequested();
        }
    });
    connect(flushCacheButton, &QPushButton::clicked,
            this, &Pageinfo::flushOfflineCacheRequested);

    updateOfflineCacheStatus(m_cacheEnabled, m_flushEnabled, m_pendingCount);
}

QFrame *Pageinfo::createInfoCard(const InfoCardSpec &spec)
{
    QFrame *cardFrame = new QFrame(this);
    cardFrame->setObjectName("InfoCard");
    cardFrame->setMinimumHeight(50);

    QLabel *iconLabel = new QLabel(cardFrame);
    iconLabel->setObjectName("InfoIconSlot");
    iconLabel->setFixedSize(24, 24);
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(spec.title, cardFrame);
    titleLabel->setObjectName("InfoCardTitle");

    QLabel *valueLabel = new QLabel(spec.defaultValue, cardFrame);
    valueLabel->setObjectName("InfoCardValue");
    valueLabel->setProperty("state", QVariant(spec.defaultState));

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(valueLabel);

    QHBoxLayout *cardLayout = new QHBoxLayout(cardFrame);
    cardLayout->setContentsMargins(6, 5, 6, 5);
    cardLayout->setSpacing(6);
    cardLayout->addWidget(iconLabel);
    cardLayout->addLayout(textLayout, 1);

    InfoCardWidget card;
    card.key = spec.key;
    card.frame = cardFrame;
    card.iconLabel = iconLabel;
    card.titleLabel = titleLabel;
    card.valueLabel = valueLabel;
    infoCards.append(card);

    return cardFrame;
}

QWidget *Pageinfo::createRuntimeRow(const RuntimeRowSpec &spec)
{
    QWidget *rowWidget = new QWidget(this);
    rowWidget->setObjectName("RuntimeRow");

    QLabel *dotLabel = new QLabel(rowWidget);
    dotLabel->setObjectName("RuntimeDot");
    dotLabel->setFixedSize(7, 7);
    dotLabel->setProperty("state", QVariant(spec.defaultState));

    QLabel *titleLabel = new QLabel(spec.title, rowWidget);
    titleLabel->setObjectName("RuntimeTitle");

    QLabel *valueLabel = new QLabel(spec.defaultValue, rowWidget);
    valueLabel->setObjectName("RuntimeValue");
    valueLabel->setProperty("state", QVariant(spec.defaultState));

    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(5);
    rowLayout->addWidget(dotLabel);
    rowLayout->addWidget(titleLabel);
    rowLayout->addStretch();
    rowLayout->addWidget(valueLabel);

    RuntimeRowWidget row;
    row.key = spec.key;
    row.dotLabel = dotLabel;
    row.titleLabel = titleLabel;
    row.valueLabel = valueLabel;
    runtimeRows.append(row);

    return rowWidget;
}

void Pageinfo::addInfo(const DataPack &pack)
{
    updateLastSync(pack.time.isValid() ? pack.time : QDateTime::currentDateTime());
    setRuntimeRowValue("linux_data", "运行中", "online");

    for (const auto &dev : pack.devices) {
        if (dev.type != DEV_SYSINFO || !dev.valid)
            continue;

        const auto &sys = dev.sys;
        setInfoCardValue("kernel", sys.kernel.isEmpty() ? "--" : sys.kernel);
        setInfoCardValue("arch", sys.arch.isEmpty() ? "--" : sys.arch);
        setInfoCardValue("os", sys.os.isEmpty() ? "--" : sys.os);
        setInfoCardValue("screen", QString("%1 x %2").arg(sys.screenW).arg(sys.screenH));
    }
}

void Pageinfo::setIpcConnected(bool connected)
{
    if (m_ipcConnected == connected)
        return;

    m_ipcConnected = connected;
    updateIpcStatusLabel();
    updateOfflineCacheButtons();
}

void Pageinfo::updateOfflineCacheStatus(bool cacheEnabled, bool flushEnabled, int pendingCount)
{
    m_cacheSupported = true;
    m_cacheEnabled = cacheEnabled;
    m_flushEnabled = cacheEnabled ? flushEnabled : false;
    m_pendingCount = pendingCount < 0 ? 0 : pendingCount;

    if (cacheStateLabel) {
        cacheStateLabel->setText(m_cacheEnabled ? "缓存：开启" : "缓存：关闭");
        cacheStateLabel->setProperty("state", QVariant(m_cacheEnabled ? "online" : "offline"));
        polishState(cacheStateLabel);
    }
    if (flushStateLabel) {
        flushStateLabel->setText(m_flushEnabled ? "发送：开启" : "发送：关闭");
        flushStateLabel->setProperty("state", QVariant(m_flushEnabled ? "online" : "offline"));
        polishState(flushStateLabel);
    }
    if (pendingCountLabel) {
        pendingCountLabel->setText(QString("待发送：%1").arg(m_pendingCount));
        pendingCountLabel->setProperty("state", QVariant(m_pendingCount > 0 ? "warning" : "online"));
        polishState(pendingCountLabel);
    }

    setToggleButtonChecked(cacheEnableButton, m_cacheEnabled);
    setToggleButtonChecked(flushEnableButton, m_flushEnabled);
    updateOfflineCacheButtons();
}

void Pageinfo::setOfflineCacheUnsupported()
{
    m_cacheSupported = false;
    m_cacheEnabled = false;
    m_flushEnabled = false;
    m_pendingCount = 0;

    if (cacheStateLabel) {
        cacheStateLabel->setText("缓存：不支持");
        cacheStateLabel->setProperty("state", QVariant("offline"));
        polishState(cacheStateLabel);
    }
    if (flushStateLabel) {
        flushStateLabel->setText("发送：关闭");
        flushStateLabel->setProperty("state", QVariant("offline"));
        polishState(flushStateLabel);
    }
    if (pendingCountLabel) {
        pendingCountLabel->setText("待发送：0");
        pendingCountLabel->setProperty("state", QVariant("offline"));
        polishState(pendingCountLabel);
    }

    setToggleButtonChecked(cacheEnableButton, false);
    setToggleButtonChecked(flushEnableButton, false);
    updateOfflineCacheButtons();
}

void Pageinfo::setInfoCardValue(const QString &key, const QString &value, const QString &state)
{
    for (InfoCardWidget &card : infoCards) {
        if (card.key != key || !card.valueLabel)
            continue;

        card.valueLabel->setText(value);
        if (!state.isEmpty())
            card.valueLabel->setProperty("state", QVariant(state));
        polishState(card.valueLabel);
        return;
    }
}

void Pageinfo::setRuntimeRowValue(const QString &key, const QString &value, const QString &state)
{
    for (RuntimeRowWidget &row : runtimeRows) {
        if (row.key != key)
            continue;

        if (row.valueLabel) {
            row.valueLabel->setText(value);
            if (!state.isEmpty())
                row.valueLabel->setProperty("state", QVariant(state));
            polishState(row.valueLabel);
        }

        if (row.dotLabel && !state.isEmpty()) {
            row.dotLabel->setProperty("state", QVariant(state));
            polishState(row.dotLabel);
        }
        return;
    }
}

void Pageinfo::polishState(QWidget *widget)
{
    if (!widget)
        return;

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void Pageinfo::updateIpcStatusLabel()
{
    if (!reconnectButton)
        return;

    const QString state = m_ipcConnected ? "online" : "offline";
    setInfoCardValue("ipc", m_ipcConnected ? "已连接" : "未连接", state);
    setRuntimeRowValue("ipc", m_ipcConnected ? "已连接" : "未连接", state);

    reconnectButton->setEnabled(!m_ipcConnected);
    updateOfflineCacheButtons();
}

void Pageinfo::updateLastSync(const QDateTime &time)
{
    lastSyncTime = time;
    if (!lastSyncTime.isValid())
        return;

    setRuntimeRowValue("last_sync", lastSyncTime.toString("HH:mm:ss"), "online");
}

void Pageinfo::updateOfflineCacheButtons()
{
    const bool available = m_ipcConnected && m_cacheSupported;
    if (cacheEnableButton)
        cacheEnableButton->setEnabled(available);
    if (saveCacheButton)
        saveCacheButton->setEnabled(available);
    if (refreshCacheButton)
        refreshCacheButton->setEnabled(available);
    if (flushEnableButton)
        flushEnableButton->setEnabled(false);
    if (clearCacheButton)
        clearCacheButton->setEnabled(available && m_pendingCount > 0);
    if (flushCacheButton)
        flushCacheButton->setEnabled(available && m_pendingCount > 0);
}

void Pageinfo::setToggleButtonChecked(QPushButton *button, bool checked)
{
    if (!button)
        return;

    button->blockSignals(true);
    button->setChecked(checked);
    button->setText(checked ? "缓存开启" : "缓存关闭");
    button->setProperty("checked", QVariant(checked ? "true" : "false"));
    polishState(button);
    button->blockSignals(false);
}
