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
                                            };

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 3, 5, 3);
    mainLayout->setSpacing(5);

    QWidget *cardsWidget = new QWidget(this);
    cardsWidget->setObjectName("InfoCardsArea");

    QGridLayout *cardsLayout = new QGridLayout(cardsWidget);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setHorizontalSpacing(4);
    cardsLayout->setVerticalSpacing(4);

    for (int i = 0; i < cardSpecs.size(); ++i) {
        cardsLayout->addWidget(createInfoCard(cardSpecs.at(i)), i / 2, i % 2);
    }

    cardsLayout->setColumnStretch(0, 1);
    cardsLayout->setColumnStretch(1, 1);

    QFrame *runtimePanel = new QFrame(this);
    runtimePanel->setObjectName("RuntimePanel");
    runtimePanel->setMinimumWidth(132);
    runtimePanel->setMaximumWidth(145);

    QVBoxLayout *runtimeLayout = new QVBoxLayout(runtimePanel);
    runtimeLayout->setContentsMargins(5, 4, 5, 4);
    runtimeLayout->setSpacing(2);

    hintLabel = new QLabel("运行信息", runtimePanel);
    hintLabel->setObjectName("PanelTitle");
    hintLabel->setFixedHeight(16);
    runtimeLayout->addWidget(hintLabel);

    for (const RuntimeRowSpec &spec : rowSpecs) {
        QWidget *row = createRuntimeRow(spec);
        row->setFixedHeight(17);
        runtimeLayout->addWidget(row);
    }

    runtimeLayout->addSpacing(2);

    reconnectButton = new QPushButton("重新连接", runtimePanel);
    reconnectButton->setObjectName("RuntimeActionButton");
    reconnectButton->setFixedHeight(21);
    runtimeLayout->addWidget(reconnectButton);

    runtimeLayout->addSpacing(3);

    cacheStateLabel = new QLabel("缓存：关闭", runtimePanel);
    cacheStateLabel->setObjectName("RuntimeValue");
    cacheStateLabel->setProperty("state", QVariant("offline"));
    cacheStateLabel->setFixedHeight(16);

    pendingCountLabel = new QLabel("待发送：0", runtimePanel);
    pendingCountLabel->setObjectName("RuntimeValue");
    pendingCountLabel->setProperty("state", QVariant("online"));
    pendingCountLabel->setFixedHeight(16);

    runtimeLayout->addWidget(cacheStateLabel);
    runtimeLayout->addWidget(pendingCountLabel);

    QHBoxLayout *cacheActions = new QHBoxLayout;
    cacheActions->setContentsMargins(0, 0, 0, 0);
    cacheActions->setSpacing(3);

    refreshCacheButton = new QPushButton("刷新", runtimePanel);
    refreshCacheButton->setObjectName("SmallActionButton");
    refreshCacheButton->setFixedHeight(21);

    flushCacheButton = new QPushButton("补发", runtimePanel);
    flushCacheButton->setObjectName("SmallActionButton");
    flushCacheButton->setFixedHeight(21);

    moreCacheButton = new QPushButton("更多", runtimePanel);
    moreCacheButton->setObjectName("SmallActionButton");
    moreCacheButton->setFixedHeight(21);

    cacheActions->addWidget(refreshCacheButton);
    cacheActions->addWidget(flushCacheButton);
    cacheActions->addWidget(moreCacheButton);

    runtimeLayout->addLayout(cacheActions);
    runtimeLayout->addStretch();

    mainLayout->addWidget(cardsWidget, 2);
    mainLayout->addWidget(runtimePanel, 1);

    updateIpcStatusLabel();

    connect(reconnectButton, &QPushButton::clicked,
            this, &Pageinfo::reconnectIpcRequested);

    connect(refreshCacheButton, &QPushButton::clicked,
            this, &Pageinfo::offlineCacheRefreshRequested);

    connect(flushCacheButton, &QPushButton::clicked,
            this, &Pageinfo::flushOfflineCacheRequested);

    connect(moreCacheButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this,
                                 "缓存设置",
                                 "缓存开关、自动补发、清空缓存等低频操作建议放到设置弹窗或单独设置页中。\n\n"
                                 "当前信息页只保留状态显示、刷新和补发，避免右侧面板过于拥挤。");
    });

    updateOfflineCacheStatus(m_cacheEnabled, m_flushEnabled, m_pendingCount);
}

QFrame *Pageinfo::createInfoCard(const InfoCardSpec &spec)
{
    QFrame *cardFrame = new QFrame(this);
    cardFrame->setObjectName("InfoCard");
    cardFrame->setMinimumHeight(44);

    QLabel *iconLabel = new QLabel(cardFrame);
    iconLabel->setObjectName("InfoIconSlot");
    iconLabel->setFixedSize(20, 20);
    iconLabel->setAlignment(Qt::AlignCenter);

    QLabel *titleLabel = new QLabel(spec.title, cardFrame);
    titleLabel->setObjectName("InfoCardTitle");

    QLabel *valueLabel = new QLabel(spec.defaultValue, cardFrame);
    valueLabel->setObjectName("InfoCardValue");
    valueLabel->setProperty("state", QVariant(spec.defaultState));

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(0);
    textLayout->addWidget(titleLabel);
    textLayout->addWidget(valueLabel);

    QHBoxLayout *cardLayout = new QHBoxLayout(cardFrame);
    cardLayout->setContentsMargins(5, 3, 5, 3);
    cardLayout->setSpacing(5);
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
    rowWidget->setFixedHeight(17);

    QLabel *dotLabel = new QLabel(rowWidget);
    dotLabel->setObjectName("RuntimeDot");
    dotLabel->setFixedSize(6, 6);
    dotLabel->setProperty("state", QVariant(spec.defaultState));

    QLabel *titleLabel = new QLabel(spec.title, rowWidget);
    titleLabel->setObjectName("RuntimeTitle");

    QLabel *valueLabel = new QLabel(spec.defaultValue, rowWidget);
    valueLabel->setObjectName("RuntimeValue");
    valueLabel->setProperty("state", QVariant(spec.defaultState));

    QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(3);
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
        if (dev.type != DEV_SYSINFO || !dev.valid) {
            continue;
        }

        const auto &sys = dev.sys;

        setInfoCardValue("kernel", sys.kernel.isEmpty() ? "--" : sys.kernel);
        setInfoCardValue("arch", sys.arch.isEmpty() ? "--" : sys.arch);
        setInfoCardValue("os", sys.os.isEmpty() ? "--" : sys.os);
        setInfoCardValue("screen", QString("%1 x %2").arg(sys.screenW).arg(sys.screenH));
    }
}

void Pageinfo::setIpcConnected(bool connected)
{
    if (m_ipcConnected == connected) {
        return;
    }

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

    if (pendingCountLabel) {
        pendingCountLabel->setText(QString("待发送：%1").arg(m_pendingCount));

        if (!m_cacheEnabled) {
            pendingCountLabel->setProperty("state", QVariant("offline"));
        } else if (m_pendingCount > 0) {
            pendingCountLabel->setProperty("state", QVariant("warning"));
        } else {
            pendingCountLabel->setProperty("state", QVariant("online"));
        }

        polishState(pendingCountLabel);
    }

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

    if (pendingCountLabel) {
        pendingCountLabel->setText("待发送：0");
        pendingCountLabel->setProperty("state", QVariant("offline"));
        polishState(pendingCountLabel);
    }

    updateOfflineCacheButtons();
}

void Pageinfo::setInfoCardValue(const QString &key, const QString &value, const QString &state)
{
    for (InfoCardWidget &card : infoCards) {
        if (card.key != key || !card.valueLabel) {
            continue;
        }

        card.valueLabel->setText(value);

        if (!state.isEmpty()) {
            card.valueLabel->setProperty("state", QVariant(state));
        }

        polishState(card.valueLabel);
        return;
    }
}

void Pageinfo::setRuntimeRowValue(const QString &key, const QString &value, const QString &state)
{
    for (RuntimeRowWidget &row : runtimeRows) {
        if (row.key != key) {
            continue;
        }

        if (row.valueLabel) {
            row.valueLabel->setText(value);

            if (!state.isEmpty()) {
                row.valueLabel->setProperty("state", QVariant(state));
            }

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
    if (!widget) {
        return;
    }

    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

void Pageinfo::updateIpcStatusLabel()
{
    const QString state = m_ipcConnected ? "online" : "offline";

    setInfoCardValue("ipc", m_ipcConnected ? "已连接" : "未连接", state);
    setRuntimeRowValue("ipc", m_ipcConnected ? "已连接" : "未连接", state);

    if (reconnectButton) {
        reconnectButton->setEnabled(!m_ipcConnected);
    }

    updateOfflineCacheButtons();
}

void Pageinfo::updateLastSync(const QDateTime &time)
{
    lastSyncTime = time;

    if (!lastSyncTime.isValid()) {
        return;
    }

    setRuntimeRowValue("last_sync", lastSyncTime.toString("HH:mm:ss"), "online");
}

void Pageinfo::updateOfflineCacheButtons()
{
    const bool available = m_ipcConnected && m_cacheSupported;

    if (refreshCacheButton) {
        refreshCacheButton->setEnabled(available);
    }

    if (flushCacheButton) {
        flushCacheButton->setEnabled(available && m_cacheEnabled && m_pendingCount > 0);
    }

    if (moreCacheButton) {
        moreCacheButton->setEnabled(available);
    }
}
