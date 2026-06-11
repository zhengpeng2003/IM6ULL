#include "pageinfo.h"

#include <QGridLayout>
#include <QHBoxLayout>
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
    runtimePanel->setMinimumWidth(138);

    hintLabel = new QLabel("运行信息", runtimePanel);
    hintLabel->setObjectName("PanelTitle");

    QVBoxLayout *runtimeLayout = new QVBoxLayout(runtimePanel);
    runtimeLayout->setContentsMargins(7, 6, 7, 6);
    runtimeLayout->setSpacing(5);
    runtimeLayout->addWidget(hintLabel);

    for (const RuntimeRowSpec &spec : rowSpecs)
        runtimeLayout->addWidget(createRuntimeRow(spec));

    runtimeLayout->addStretch();

    reconnectButton = new QPushButton("重新连接", runtimePanel);
    reconnectButton->setObjectName("RuntimeActionButton");
    reconnectButton->setFixedHeight(26);
    runtimeLayout->addWidget(reconnectButton);

    mainLayout->addWidget(cardsWidget, 2);
    mainLayout->addWidget(runtimePanel, 1);

    updateIpcStatusLabel();

    connect(reconnectButton, &QPushButton::clicked,
            this, &Pageinfo::reconnectIpcRequested);
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
}

void Pageinfo::updateLastSync(const QDateTime &time)
{
    lastSyncTime = time;
    if (!lastSyncTime.isValid())
        return;

    setRuntimeRowValue("last_sync", lastSyncTime.toString("HH:mm:ss"), "online");
}
