#include "PageStatus.h"

#include <QComboBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

#include "pageui/slavedetaildialog.h"
#include "pageui/slavelistdialog.h"
PageStatus::PageStatus(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();
    setMasterSummary(0, 0, 0, "--");
    setAlarmText("告警：--");
}

void PageStatus::setMasterSummary(int masterCount,
                                  int onlineSlaveCount,
                                  int alarmCount,
                                  const QString &mqttState)
{
    Q_UNUSED(masterCount);
    summaryLabel->setText(QString("MQTT：%1    在线从站：%2    告警：%3")
                              .arg(mqttState)
                              .arg(onlineSlaveCount)
                              .arg(alarmCount));
}

void PageStatus::setMasterList(const QList<MasterStatusInfo> &masters,
                               int preferredMasterSlot)
{
    const int targetMasterSlot = preferredMasterSlot >= 0
        ? preferredMasterSlot
        : currentMasterSlot;

    masterCombo->blockSignals(true);
    masterCombo->clear();

    for (const MasterStatusInfo &master : masters)
        masterCombo->addItem(master.masterName, master.masterSlot);

    int index = -1;
    for (int i = 0; i < masterCombo->count(); ++i) {
        if (masterCombo->itemData(i).toInt() == targetMasterSlot) {
            index = i;
            break;
        }
    }
    if (index < 0 && masterCombo->count() > 0)
        index = 0;
    if (index >= 0)
        masterCombo->setCurrentIndex(index);

    masterCombo->blockSignals(false);

    if (masterCombo->count() > 0) {
        currentMasterSlot = masterCombo->currentData().toInt();
        currentMasterName = masterCombo->currentText();
    } else {
        currentMasterSlot = -1;
        currentMasterName.clear();
    }

    refreshMasterLabels();
}

void PageStatus::setCurrentMaster(const QString &masterName, int slaveCount)
{
    currentMasterName = masterName;
    const int comboIndex = masterCombo->findText(masterName);
    if (comboIndex >= 0)
        masterCombo->setCurrentIndex(comboIndex);

    slaveCountLabel->setText(QString("从站：%1").arg(slaveCount));
}

void PageStatus::setSlaveList(const QList<SlaveDeviceInfo> &slaveList)
{
    slaves = slaveList;
    for (const SlaveDeviceInfo &slave : slaves) {
        SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(slave));
        runtime.online = slave.online;
        slaveRuntime.insert(runtimeKey(slave), runtime);
    }
    currentSlaveIndex = slaves.isEmpty() ? -1 : 0;
    rebuildSlaveCards();
    refreshMasterLabels();
    if (currentSlaveIndex >= 0)
        selectSlave(currentSlaveIndex);
    else
        clearCurrentDetail();
    updateOpenDialogs();
}

int PageStatus::currentMasterSlotValue() const
{
    return currentMasterSlot;
}

void PageStatus::updateSlaveOnline(int masterSlot,
                                   int slaveAddr,
                                   const QString &deviceType,
                                   bool online)
{
    for (int i = 0; i < slaves.size(); ++i) {
        SlaveDeviceInfo &slave = slaves[i];
        if (slave.masterSlot != masterSlot ||
            slave.slaveAddr != slaveAddr ||
            slave.deviceType != deviceType) {
            continue;
        }

        slave.online = online;
        SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(slave));
        runtime.online = online;
        slaveRuntime.insert(runtimeKey(slave), runtime);
        updateSlaveCardStyle(i, i == currentSlaveIndex);

        if (i == currentSlaveIndex)
            selectSlave(i);
        updateOpenDialogs();
        return;
    }
}

void PageStatus::selectSlave(int index)
{
    if (index < 0 || index >= slaves.size())
        return;

    currentSlaveIndex = index;
    refreshSlaveCards();

    const SlaveDeviceInfo &slave = slaves.at(index);
    const SlaveRuntimeInfo runtime = runtimeForSlave(slave);
    if (slave.deviceType == "sensor_th")
        showSensorDetail(slave, runtime);
    else if (slave.deviceType == "relay")
        showRelayDetail(slave, runtime);
    else
        showMeterDetail(slave, runtime);

    emit slaveSelected(slave.masterSlot, slave.slaveAddr, slave.deviceType);
}

void PageStatus::setAlarmText(const QString &text)
{
    alarmLabel->setText(text);
}

void PageStatus::setSensorThData(int masterSlot,
                                 int slaveAddr,
                                 double temperature,
                                 double humidity,
                                 const QString &updateTime)
{
    SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "sensor_th"));
    runtime.online = true;
    runtime.hasSensorTh = true;
    runtime.temperature = temperature;
    runtime.humidity = humidity;
    runtime.updateTime = updateTime;
    slaveRuntime.insert(runtimeKey(masterSlot, slaveAddr, "sensor_th"), runtime);

    if (isCurrentSlave(masterSlot, slaveAddr, "sensor_th"))
        selectSlave(currentSlaveIndex);
    updateOpenDialogs();
}

void PageStatus::setRelayStates(int masterSlot,
                                int slaveAddr,
                                bool ledOn,
                                bool fanOn,
                                bool buzzerOn,
                                const QString &updateTime)
{
    SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "relay"));
    runtime.online = true;
    runtime.hasRelay = true;
    runtime.ledOn = ledOn;
    runtime.fanOn = fanOn;
    runtime.buzzerOn = buzzerOn;
    runtime.updateTime = updateTime;
    slaveRuntime.insert(runtimeKey(masterSlot, slaveAddr, "relay"), runtime);

    if (isCurrentSlave(masterSlot, slaveAddr, "relay"))
        selectSlave(currentSlaveIndex);
    updateOpenDialogs();
}

void PageStatus::setMeterValues(int masterSlot,
                                int slaveAddr,
                                const QString &voltage,
                                const QString &current,
                                const QString &power,
                                const QString &energy,
                                const QString &updateTime)
{
    SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "meter"));
    runtime.online = true;
    runtime.hasMeter = true;
    runtime.voltage = voltage;
    runtime.current = current;
    runtime.power = power;
    runtime.energy = energy;
    runtime.updateTime = updateTime;
    slaveRuntime.insert(runtimeKey(masterSlot, slaveAddr, "meter"), runtime);

    if (isCurrentSlave(masterSlot, slaveAddr, "meter"))
        selectSlave(currentSlaveIndex);
    updateOpenDialogs();
}

bool PageStatus::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (int i = 0; i < slaveCards.size(); ++i) {
            if (watched == slaveCards.at(i).frame) {
                selectSlave(i);
                openSlaveListDialog();
                return true;
            }
        }

        if (watched == slaveListPanel) {
            openSlaveListDialog();
            return true;
        }

        if (watched == slaveScrollArea || watched == slaveScrollArea->viewport()) {
            openSlaveListDialog();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void PageStatus::initUI()
{
    summaryLabel = new QLabel(this);
    summaryLabel->setObjectName("SummaryBar");
    summaryLabel->setFixedHeight(24);

    QLabel *masterLabel = new QLabel("端口：", this);
    masterLabel->setObjectName("DetailKey");

    masterCombo = new QComboBox(this);
    masterCombo->setObjectName("CompactCombo");
    masterCombo->setFixedWidth(112);

    slaveCountLabel = new QLabel(this);
    slaveCountLabel->setObjectName("DetailValue");

    addSlaveButton = new QPushButton("+ 从站", this);
    addSlaveButton->setObjectName("ActionButton");
    addSlaveButton->setFixedWidth(58);
    addSlaveButton->setEnabled(false);

    QHBoxLayout *masterRow = new QHBoxLayout;
    masterRow->setContentsMargins(0, 0, 0, 0);
    masterRow->setSpacing(4);
    masterRow->addWidget(masterLabel);
    masterRow->addWidget(masterCombo);
    masterRow->addWidget(slaveCountLabel);
    masterRow->addStretch();
    masterRow->addWidget(addSlaveButton);

    slaveListPanel = new QFrame(this);
    slaveListPanel->setObjectName("Panel");
    slaveListPanel->setFixedWidth(172);
    slaveListPanel->setCursor(Qt::PointingHandCursor);
    slaveListPanel->installEventFilter(this);

    currentPortLabel = new QLabel("当前端口：--", slaveListPanel);
    currentPortLabel->setObjectName("PanelTitle");
    currentPortLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    listTitleLabel = new QLabel("当前从站设备：0", slaveListPanel);
    listTitleLabel->setObjectName("PanelTitle");
    listTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    emptyListLabel = new QLabel("暂无从站设备", slaveListPanel);
    emptyListLabel->setObjectName("HintText");
    emptyListLabel->setAlignment(Qt::AlignCenter);
    emptyListLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    QWidget *slaveListContent = new QWidget(slaveListPanel);
    slaveListContent->setObjectName("SlaveListContent");
    slaveListLayout = new QVBoxLayout(slaveListContent);
    slaveListLayout->setContentsMargins(0, 0, 0, 0);
    slaveListLayout->setSpacing(4);

    slaveScrollArea = new QScrollArea(slaveListPanel);
    slaveScrollArea->setObjectName("SlaveListScrollArea");
    slaveScrollArea->setWidgetResizable(true);
    slaveScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    slaveScrollArea->setWidget(slaveListContent);
    slaveScrollArea->installEventFilter(this);
    slaveScrollArea->viewport()->installEventFilter(this);

    QVBoxLayout *slavePanelLayout = new QVBoxLayout(slaveListPanel);
    slavePanelLayout->setContentsMargins(7, 6, 7, 7);
    slavePanelLayout->setSpacing(4);
    slavePanelLayout->addWidget(currentPortLabel);
    slavePanelLayout->addLayout(masterRow);
    slavePanelLayout->addWidget(listTitleLabel);
    slavePanelLayout->addWidget(slaveScrollArea, 1);

    detailPanel = new QFrame(this);
    detailPanel->setObjectName("DetailPanel");

    detailTitleLabel = new QLabel("从站详情（地址：--）", detailPanel);
    detailTitleLabel->setObjectName("DetailTitle");

    detailStateLabel = new QLabel("离线", detailPanel);
    detailStateLabel->setObjectName("DetailStateBadge");
    detailStateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout *detailTitleRow = new QHBoxLayout;
    detailTitleRow->setContentsMargins(0, 0, 0, 0);
    detailTitleRow->setSpacing(4);
    detailTitleRow->addWidget(detailTitleLabel, 1);
    detailTitleRow->addWidget(detailStateLabel);

    QLabel *portKey = new QLabel("端口：", detailPanel);
    QLabel *addrKey = new QLabel("地址：", detailPanel);
    QLabel *typeKey = new QLabel("类型：", detailPanel);
    portKey->setObjectName("DetailKey");
    addrKey->setObjectName("DetailKey");
    typeKey->setObjectName("DetailKey");

    detailPortLabel = new QLabel("--", detailPanel);
    detailAddrLabel = new QLabel("--", detailPanel);
    detailTypeLabel = new QLabel("--", detailPanel);
    detailPortLabel->setObjectName("DetailValue");
    detailAddrLabel->setObjectName("DetailValue");
    detailTypeLabel->setObjectName("DetailValue");

    QHBoxLayout *metaRow = new QHBoxLayout;
    metaRow->setContentsMargins(0, 0, 0, 0);
    metaRow->setSpacing(3);
    metaRow->addWidget(portKey);
    metaRow->addWidget(detailPortLabel);
    metaRow->addSpacing(5);
    metaRow->addWidget(addrKey);
    metaRow->addWidget(detailAddrLabel);
    metaRow->addSpacing(5);
    metaRow->addWidget(typeKey);
    metaRow->addWidget(detailTypeLabel, 1);

    metricPanel = new QFrame(detailPanel);
    metricPanel->setObjectName("MetricPanel");
    metricGrid = new QGridLayout(metricPanel);
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setHorizontalSpacing(6);
    metricGrid->setVerticalSpacing(6);
    metricA = createMetricCard("T", "温度");
    metricB = createMetricCard("H", "湿度");
    metricC = createMetricCard("R", "状态");
    metricD = createMetricCard("P", "状态");
    metricGrid->addWidget(metricA.frame, 0, 0);
    metricGrid->addWidget(metricB.frame, 0, 1);
    metricGrid->addWidget(metricC.frame, 1, 0);
    metricGrid->addWidget(metricD.frame, 1, 1);
    metricGrid->setColumnStretch(0, 1);
    metricGrid->setColumnStretch(1, 1);

    relayControlPanel = new QWidget(detailPanel);
    relayControlPanel->setObjectName("RelayControlPanel");
    QHBoxLayout *relayControlLayout = new QHBoxLayout(relayControlPanel);
    relayControlLayout->setContentsMargins(0, 0, 0, 0);
    relayControlLayout->setSpacing(4);

    auto createRelayButton = [this](const QString &text, const QString &channel, bool on) {
        QPushButton *button = new QPushButton(text, relayControlPanel);
        button->setObjectName(on ? "SmallActionButton" : "SmallGhostButton");
        button->setFixedWidth(34);
        connect(button, &QPushButton::clicked, this, [this, channel, on]() {
            if (currentSlaveIndex < 0 || currentSlaveIndex >= slaves.size())
                return;
            const SlaveDeviceInfo &slave = slaves.at(currentSlaveIndex);
            emit relayCommandRequested(slave.masterSlot, slave.slaveAddr, channel, on);
        });
        return button;
    };

    ledOnButton = createRelayButton("灯开", "led", true);
    ledOffButton = createRelayButton("灯关", "led", false);
    fanOnButton = createRelayButton("扇开", "fan", true);
    fanOffButton = createRelayButton("扇关", "fan", false);
    buzzerOnButton = createRelayButton("蜂开", "buzzer", true);
    buzzerOffButton = createRelayButton("蜂关", "buzzer", false);
    relayControlLayout->addWidget(ledOnButton);
    relayControlLayout->addWidget(ledOffButton);
    relayControlLayout->addWidget(fanOnButton);
    relayControlLayout->addWidget(fanOffButton);
    relayControlLayout->addWidget(buzzerOnButton);
    relayControlLayout->addWidget(buzzerOffButton);
    relayControlLayout->addStretch();

    pollIntervalLabel = new QLabel("轮询间隔：1000 ms", detailPanel);
    lastUpdateLabel = new QLabel("最后更新：--", detailPanel);
    pollIntervalLabel->setObjectName("DetailValue");
    lastUpdateLabel->setObjectName("DetailValue");
    lastUpdateLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    QHBoxLayout *footerRow = new QHBoxLayout;
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->addWidget(pollIntervalLabel);
    footerRow->addStretch();
    footerRow->addWidget(lastUpdateLabel);

    QVBoxLayout *detailLayout = new QVBoxLayout(detailPanel);
    detailLayout->setContentsMargins(10, 8, 10, 8);
    detailLayout->setSpacing(7);
    detailLayout->addLayout(detailTitleRow);
    detailLayout->addLayout(metaRow);
    detailLayout->addWidget(metricPanel, 1);
    detailLayout->addWidget(relayControlPanel);
    detailLayout->addLayout(footerRow);

    QHBoxLayout *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(6);
    bodyLayout->addWidget(slaveListPanel);
    bodyLayout->addWidget(detailPanel, 1);

    alarmLabel = new QLabel(this);
    alarmLabel->setObjectName("AlarmBar");
    alarmLabel->setFixedHeight(20);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);
    mainLayout->addWidget(summaryLabel);
    mainLayout->addLayout(bodyLayout, 1);
    mainLayout->addWidget(alarmLabel);

    connect(masterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0) {
            currentMasterSlot = masterCombo->itemData(index).toInt();
            currentMasterName = masterCombo->itemText(index);
        } else {
            currentMasterSlot = -1;
            currentMasterName.clear();
        }

        refreshMasterLabels();
        emit masterChanged(currentMasterSlot);
    });

    connect(addSlaveButton, &QPushButton::clicked, this, [this]() {
        emit addSlaveRequested(currentMasterSlot);
    });

    refreshMasterLabels();
    rebuildSlaveCards();
    clearCurrentDetail();
}

void PageStatus::refreshMasterLabels()
{
    const QString portName = displayMasterName();
    slaveCountLabel->setText(QString("从站：%1").arg(slaves.size()));
    addSlaveButton->setEnabled(currentMasterSlot >= 0);
    if (currentPortLabel)
        currentPortLabel->setText(QString("当前端口：%1").arg(portName));
    if (listTitleLabel)
        listTitleLabel->setText(QString("当前从站设备：%1").arg(slaves.size()));
}

void PageStatus::refreshSlaveCards()
{
    for (int i = 0; i < slaveCards.size(); ++i)
        updateSlaveCardStyle(i, i == currentSlaveIndex);
}

void PageStatus::rebuildSlaveCards()
{
    while (slaveListLayout->count() > 0) {
        QLayoutItem *item = slaveListLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            if (widget != emptyListLabel)
                widget->deleteLater();
        }
        delete item;
    }
    slaveCards.clear();

    emptyListLabel->setVisible(slaves.isEmpty());
    if (slaves.isEmpty()) {
        slaveListLayout->addWidget(emptyListLabel, 1);
    } else {
        for (int i = 0; i < slaves.size(); ++i)
            slaveListLayout->addWidget(createSlaveCard(i));
        slaveListLayout->addStretch();
    }
}

void PageStatus::clearCurrentDetail()
{
    detailTitleLabel->setText("从站详情（地址：--）");
    detailStateLabel->setText("离线");
    detailStateLabel->setProperty("state", "offline");
    detailPortLabel->setText(displayMasterName());
    detailAddrLabel->setText("--");
    detailTypeLabel->setText("--");
    setMetricCard(metricA, "T", "温度", "--", "");
    setMetricCard(metricB, "H", "湿度", "--", "");
    setMetricCard(metricC, "R", "状态", "--", "");
    setMetricCard(metricD, "P", "状态", "--", "");
    metricA.frame->setVisible(true);
    metricB.frame->setVisible(true);
    metricC.frame->setVisible(false);
    metricD.frame->setVisible(false);
    relayControlPanel->setVisible(false);
    pollIntervalLabel->setText("轮询间隔：1000 ms");
    lastUpdateLabel->setText("最后更新：--");
    detailStateLabel->style()->unpolish(detailStateLabel);
    detailStateLabel->style()->polish(detailStateLabel);
}

void PageStatus::updateSlaveCardStyle(int index, bool selected)
{
    if (index < 0 || index >= slaveCards.size())
        return;

    const SlaveDeviceInfo &slave = slaves.at(index);
    SlaveCard &card = slaveCards[index];
    card.frame->setProperty("selected", selected);
    card.title->setText(QString("%1  %2")
                            .arg(slave.slaveAddr)
                            .arg(slave.displayName.isEmpty()
                                     ? displayTypeName(slave.deviceType)
                                     : slave.displayName));
    card.state->setText(slave.online ? "在线" : "离线");
    card.dot->setProperty("state", slave.online ? "online" : "offline");
    card.state->setProperty("state", slave.online ? "online" : "offline");

    card.frame->style()->unpolish(card.frame);
    card.frame->style()->polish(card.frame);
    card.title->style()->unpolish(card.title);
    card.title->style()->polish(card.title);
    card.dot->style()->unpolish(card.dot);
    card.dot->style()->polish(card.dot);
    card.state->style()->unpolish(card.state);
    card.state->style()->polish(card.state);
}

QFrame *PageStatus::createSlaveCard(int index)
{
    QFrame *frame = new QFrame(slaveScrollArea);
    frame->setObjectName("SlaveCard");
    frame->setFixedHeight(34);

    QLabel *title = new QLabel(frame);
    title->setObjectName("SlaveTitle");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    QLabel *dot = new QLabel(frame);
    dot->setObjectName("StateDot");
    dot->setFixedSize(7, 7);
    dot->setAttribute(Qt::WA_TransparentForMouseEvents);
    QLabel *state = new QLabel(frame);
    state->setObjectName("SlaveState");
    state->setAttribute(Qt::WA_TransparentForMouseEvents);
    frame->setCursor(Qt::PointingHandCursor);
    frame->installEventFilter(this);

    QHBoxLayout *rowLayout = new QHBoxLayout(frame);
    rowLayout->setContentsMargins(7, 3, 7, 3);
    rowLayout->setSpacing(5);
    rowLayout->addWidget(title, 1);
    rowLayout->addWidget(dot);
    rowLayout->addWidget(state);

    SlaveCard card;
    card.frame = frame;
    card.title = title;
    card.dot = dot;
    card.state = state;
    slaveCards.append(card);
    updateSlaveCardStyle(index, false);
    return frame;
}

PageStatus::MetricCard PageStatus::createMetricCard(const QString &iconText, const QString &name)
{
    MetricCard card;
    card.frame = new QFrame(metricPanel);
    card.frame->setObjectName("MetricCard");
    card.frame->setMinimumHeight(48);

    card.icon = new QLabel(iconText, card.frame);
    card.icon->setObjectName("MetricIcon");
    card.icon->setAlignment(Qt::AlignCenter);
    card.icon->setFixedSize(24, 24);

    card.name = new QLabel(name, card.frame);
    card.name->setObjectName("DetailKey");
    card.value = new QLabel("--", card.frame);
    card.value->setObjectName("MetricValue");
    card.unit = new QLabel("", card.frame);
    card.unit->setObjectName("DetailKey");

    QHBoxLayout *valueLayout = new QHBoxLayout;
    valueLayout->setContentsMargins(0, 0, 0, 0);
    valueLayout->setSpacing(2);
    valueLayout->addWidget(card.value);
    valueLayout->addWidget(card.unit);
    valueLayout->addStretch();

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textLayout->addWidget(card.name);
    textLayout->addLayout(valueLayout);

    QHBoxLayout *layout = new QHBoxLayout(card.frame);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(7);
    layout->addWidget(card.icon);
    layout->addLayout(textLayout, 1);
    return card;
}

void PageStatus::setMetricCard(MetricCard &card,
                               const QString &iconText,
                               const QString &name,
                               const QString &value,
                               const QString &unit)
{
    card.icon->setText(iconText);
    card.name->setText(name);
    card.value->setText(value);
    card.unit->setText(unit);
}

void PageStatus::setDetailMeta(const SlaveDeviceInfo &slave)
{
    detailTitleLabel->setText(QString("从站详情（地址：%1）").arg(slave.slaveAddr));
    detailStateLabel->setText(slave.online ? "在线" : "离线");
    detailStateLabel->setProperty("state", slave.online ? "online" : "offline");
    detailPortLabel->setText(displayMasterName());
    detailAddrLabel->setText(QString::number(slave.slaveAddr));
    detailTypeLabel->setText(displayTypeName(slave.deviceType));
    pollIntervalLabel->setText("轮询间隔：1000 ms");
    detailStateLabel->style()->unpolish(detailStateLabel);
    detailStateLabel->style()->polish(detailStateLabel);
}

void PageStatus::showSensorDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime)
{
    setDetailMeta(slave);
    setMetricCard(metricA, "T", "温度",
                  runtime.hasSensorTh ? QString("%1").arg(runtime.temperature, 0, 'f', 1) : "--",
                  runtime.hasSensorTh ? "℃" : "");
    setMetricCard(metricB, "H", "湿度",
                  runtime.hasSensorTh ? QString("%1").arg(runtime.humidity, 0, 'f', 1) : "--",
                  runtime.hasSensorTh ? "%RH" : "");
    metricA.frame->setVisible(true);
    metricB.frame->setVisible(true);
    metricC.frame->setVisible(false);
    metricD.frame->setVisible(false);
    relayControlPanel->setVisible(false);
    lastUpdateLabel->setText(QString("最后更新：%1")
                                 .arg(runtime.updateTime.isEmpty() ? "--" : runtime.updateTime));
}

void PageStatus::showRelayDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime)
{
    setDetailMeta(slave);
    setMetricCard(metricA, "L", "LED", runtime.hasRelay ? (runtime.ledOn ? "开启" : "关闭") : "--", "");
    setMetricCard(metricB, "F", "FAN", runtime.hasRelay ? (runtime.fanOn ? "开启" : "关闭") : "--", "");
    setMetricCard(metricC, "B", "BUZZER", runtime.hasRelay ? (runtime.buzzerOn ? "开启" : "关闭") : "--", "");
    metricA.frame->setVisible(true);
    metricB.frame->setVisible(true);
    metricC.frame->setVisible(true);
    metricD.frame->setVisible(false);
    relayControlPanel->setVisible(true);
    const QList<QPushButton *> buttons = {
        ledOnButton, ledOffButton, fanOnButton, fanOffButton, buzzerOnButton, buzzerOffButton
    };
    for (QPushButton *button : buttons)
        button->setEnabled(slave.online);
    lastUpdateLabel->setText(QString("最后更新：%1")
                                 .arg(runtime.updateTime.isEmpty() ? "--" : runtime.updateTime));
}

void PageStatus::showMeterDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime)
{
    setDetailMeta(slave);
    setMetricCard(metricA, "V", "电压", runtime.hasMeter ? runtime.voltage : "--", "");
    setMetricCard(metricB, "I", "电流", runtime.hasMeter ? runtime.current : "--", "");
    setMetricCard(metricC, "P", "功率", runtime.hasMeter ? runtime.power : "--", "");
    setMetricCard(metricD, "E", "电能", runtime.hasMeter ? runtime.energy : "--", "");
    metricA.frame->setVisible(true);
    metricB.frame->setVisible(true);
    metricC.frame->setVisible(true);
    metricD.frame->setVisible(true);
    relayControlPanel->setVisible(false);
    lastUpdateLabel->setText(QString("最后更新：%1")
                                 .arg(runtime.updateTime.isEmpty() ? "--" : runtime.updateTime));
}

QString PageStatus::displayTypeName(const QString &deviceType) const
{
    if (deviceType == "sensor_th")
        return "温湿度传感器";
    if (deviceType == "relay")
        return "继电器";
    if (deviceType == "meter")
        return "电表";
    return deviceType;
}

QString PageStatus::displayMasterName() const
{
    if (!currentMasterName.isEmpty())
        return currentMasterName;
    return currentMasterSlot >= 0 ? QString("RS485-%1").arg(currentMasterSlot + 1) : "--";
}

void PageStatus::openSlaveListDialog()
{
    if (!slaveListDialog) {
        slaveListDialog = new SlaveListDialog(this);
        connect(slaveListDialog, &QObject::destroyed, this, [this]() {
            slaveListDialog = nullptr;
        });
        connect(slaveListDialog,
                &SlaveListDialog::slaveActivated,
                this,
                &PageStatus::openSlaveDetailDialog);
    }

    slaveListDialog->setSlaveList(slaves, slaveRuntime, currentMasterName);
    slaveListDialog->show();
    slaveListDialog->raise();
    slaveListDialog->activateWindow();
}

void PageStatus::openSlaveDetailDialog(int index)
{
    if (index < 0 || index >= slaves.size())
        return;

    if (!slaveDetailDialog) {
        slaveDetailDialog = new SlaveDetailDialog(this);
        connect(slaveDetailDialog, &QObject::destroyed, this, [this]() {
            slaveDetailDialog = nullptr;
        });
    }

    const SlaveDeviceInfo &slave = slaves.at(index);
    slaveDetailDialog->setSlave(slave, runtimeForSlave(slave), currentMasterName);
    slaveDetailDialog->show();
    slaveDetailDialog->raise();
    slaveDetailDialog->activateWindow();
}

void PageStatus::updateOpenDialogs()
{
    if (slaveListDialog)
        slaveListDialog->setSlaveList(slaves, slaveRuntime, currentMasterName);

    if (!slaveDetailDialog)
        return;

    for (const SlaveDeviceInfo &slave : slaves) {
        if (slaveDetailDialog->isShowingSlave(slave.masterSlot, slave.slaveAddr, slave.deviceType)) {
            slaveDetailDialog->setSlave(slave, runtimeForSlave(slave), currentMasterName);
            return;
        }
    }
}

QString PageStatus::runtimeKey(const SlaveDeviceInfo &slave) const
{
    return runtimeKey(slave.masterSlot, slave.slaveAddr, slave.deviceType);
}

QString PageStatus::runtimeKey(int masterSlot, int slaveAddr, const QString &deviceType) const
{
    return QString("%1:%2:%3").arg(masterSlot).arg(slaveAddr).arg(deviceType);
}

SlaveRuntimeInfo PageStatus::runtimeForSlave(const SlaveDeviceInfo &slave) const
{
    SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(slave));
    runtime.online = slave.online || runtime.online;
    return runtime;
}

bool PageStatus::isCurrentSlave(int masterSlot, int slaveAddr, const QString &deviceType) const
{
    if (currentSlaveIndex < 0 || currentSlaveIndex >= slaves.size())
        return false;

    const SlaveDeviceInfo &slave = slaves.at(currentSlaveIndex);
    return slave.masterSlot == masterSlot &&
           slave.slaveAddr == slaveAddr &&
           slave.deviceType == deviceType;
}
