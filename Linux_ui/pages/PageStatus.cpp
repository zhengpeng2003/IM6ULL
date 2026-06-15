// ============================
// pages/PageStatus.cpp
// ============================

#include "PageStatus.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

#include "../pageui/slavedetaildialog.h"
#include "../pageui/slavelistdialog.h"

#include "../sensorui/sensorthdetailcardui.h"
#include "../sensorui/relaydetailcardui.h"

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
    Q_UNUSED(onlineSlaveCount);
    Q_UNUSED(alarmCount);
    Q_UNUSED(mqttState);
}

void PageStatus::setMasterList(const QList<MasterStatusInfo> &masters)
{
    if (!masterCombo)
        return;

    const int oldSlot = currentMasterSlot;

    masterCombo->blockSignals(true);
    masterCombo->clear();

    for (const MasterStatusInfo &master : masters)
        masterCombo->addItem(master.masterName, master.masterSlot);

    int targetIndex = masterCombo->findData(oldSlot);

    if (targetIndex < 0 && masterCombo->count() > 0)
        targetIndex = 0;

    if (targetIndex >= 0)
        masterCombo->setCurrentIndex(targetIndex);

    masterCombo->blockSignals(false);
    masterCombo->setEnabled(masterCombo->count() > 0);
}

void PageStatus::setCurrentMaster(int masterSlot,
                                  const QString &masterName,
                                  int slaveCount)
{
    currentMasterSlot = masterSlot;
    currentMasterName = masterName;

    if (masterCombo) {
        const int targetIndex = masterCombo->findData(masterSlot);

        masterCombo->blockSignals(true);

        if (targetIndex >= 0)
            masterCombo->setCurrentIndex(targetIndex);
        else if (masterCombo->count() > 0)
            masterCombo->setCurrentIndex(0);

        masterCombo->blockSignals(false);
        masterCombo->setEnabled(masterCombo->count() > 0);
    }

    refreshMasterLabels();

    if (listTitleLabel)
        listTitleLabel->setText(QString("当前从站设备：%1").arg(slaveCount));

    if (slaveListDialog)
        slaveListDialog->setSlaveList(slaves, slaveRuntime, currentMasterName);
}

void PageStatus::setSlaveList(const QList<SlaveDeviceInfo> &slaveList)
{
    slaves = slaveList;

    QMap<QString, bool> visibleKeys;

    for (const SlaveDeviceInfo &slave : slaves) {
        visibleKeys.insert(runtimeKey(slave), true);

        SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(slave));
        runtime.online = slave.online;

        slaveRuntime.insert(runtimeKey(slave), runtime);
    }

    for (auto it = slaveRuntime.begin(); it != slaveRuntime.end(); ) {
        if (!visibleKeys.contains(it.key()))
            it = slaveRuntime.erase(it);
        else
            ++it;
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

void PageStatus::removeSlave(int masterSlot,
                             int slaveAddr,
                             const QString &deviceType)
{
    for (int i = 0; i < slaves.size(); ++i) {
        const SlaveDeviceInfo slave = slaves.at(i);

        if (slave.masterSlot != masterSlot ||
            slave.slaveAddr != slaveAddr ||
            slave.deviceType != deviceType) {
            continue;
        }

        slaveRuntime.remove(runtimeKey(slave));
        slaves.removeAt(i);

        if (currentSlaveIndex >= slaves.size())
            currentSlaveIndex = slaves.size() - 1;

        if (currentSlaveIndex < 0 && !slaves.isEmpty())
            currentSlaveIndex = 0;

        rebuildSlaveCards();
        refreshMasterLabels();

        if (currentSlaveIndex >= 0)
            selectSlave(currentSlaveIndex);
        else
            clearCurrentDetail();

        updateOpenDialogs();
        return;
    }
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

    const QString portName = displayMasterName();
    const QString typeName = displayTypeName(slave.deviceType);

    if (slave.deviceType == "sensor_th") {
        detailStack->setCurrentWidget(sensorThDetailUi);

        sensorThDetailUi->setBaseInfo(portName,
                                      slave.masterSlot,
                                      slave.slaveAddr,
                                      typeName,
                                      slave.deviceType,
                                      slave.online);

        sensorThDetailUi->setPollInterval(slave.pollIntervalMs);

        sensorThDetailUi->setTemperatureHumidity(runtime.hasSensorTh,
                                                 runtime.temperature,
                                                 runtime.humidity,
                                                 runtime.updateTime);
    } else if (slave.deviceType == "relay") {
        detailStack->setCurrentWidget(relayDetailUi);

        relayDetailUi->setBaseInfo(portName,
                                   slave.masterSlot,
                                   slave.slaveAddr,
                                   typeName,
                                   slave.deviceType,
                                   slave.online);

        relayDetailUi->setPollInterval(slave.pollIntervalMs);

        relayDetailUi->setRelayChannels(runtime.hasRelay,
                                        runtime.relayChannels,
                                        runtime.updateTime);
    } else {
        detailStack->setCurrentWidget(sensorThDetailUi);

        sensorThDetailUi->setBaseInfo(portName,
                                      slave.masterSlot,
                                      slave.slaveAddr,
                                      slave.deviceType,
                                      slave.deviceType,
                                      slave.online);

        sensorThDetailUi->setPollInterval(slave.pollIntervalMs);
        sensorThDetailUi->clearData();
    }

    emit slaveSelected(slave.masterSlot,
                       slave.slaveAddr,
                       slave.deviceType);
}

void PageStatus::setAlarmText(const QString &text)
{
    if (alarmLabel)
        alarmLabel->setText(text);
}

void PageStatus::setSensorThData(int masterSlot,
                                 int slaveAddr,
                                 double temperature,
                                 double humidity,
                                 const QString &updateTime)
{
    SlaveRuntimeInfo runtime =
        slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "sensor_th"));

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

void PageStatus::setRelayChannels(int masterSlot,
                                  int slaveAddr,
                                  const QVector<RelayChannelInfo> &channels,
                                  const QString &updateTime)
{
    SlaveRuntimeInfo runtime =
        slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "relay"));

    runtime.online = true;
    runtime.hasRelay = true;
    runtime.relayChannels = channels;
    runtime.updateTime = updateTime;

    if (channels.size() > 0)
        runtime.ledOn = channels.at(0).on;

    if (channels.size() > 1)
        runtime.fanOn = channels.at(1).on;

    if (channels.size() > 2)
        runtime.buzzerOn = channels.at(2).on;

    slaveRuntime.insert(runtimeKey(masterSlot, slaveAddr, "relay"), runtime);

    if (isCurrentSlave(masterSlot, slaveAddr, "relay"))
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
    QVector<RelayChannelInfo> channels =
        defaultRelayChannelsFromOldState(ledOn, fanOn, buzzerOn);

    SlaveRuntimeInfo runtime =
        slaveRuntime.value(runtimeKey(masterSlot, slaveAddr, "relay"));

    runtime.online = true;
    runtime.hasRelay = true;
    runtime.ledOn = ledOn;
    runtime.fanOn = fanOn;
    runtime.buzzerOn = buzzerOn;
    runtime.relayChannels = channels;
    runtime.updateTime = updateTime;

    slaveRuntime.insert(runtimeKey(masterSlot, slaveAddr, "relay"), runtime);

    if (isCurrentSlave(masterSlot, slaveAddr, "relay"))
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

    addSlaveButton = new QPushButton("+ 从站", this);
    addSlaveButton->setObjectName("ActionButton");
    addSlaveButton->setFixedWidth(58);
    addSlaveButton->setEnabled(false);

    masterCombo = new QComboBox(this);
    masterCombo->setObjectName("CompactCombo");
    masterCombo->setFixedWidth(86);
    masterCombo->setEnabled(false);

    QHBoxLayout *masterRow = new QHBoxLayout;
    masterRow->setContentsMargins(0, 0, 0, 0);
    masterRow->setSpacing(4);
    masterRow->addWidget(masterCombo);
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

    detailStack = new QStackedWidget(this);
    detailStack->setObjectName("DetailStack");

    sensorThDetailUi = new SensorThDetailCardUi(detailStack);
    relayDetailUi = new RelayDetailCardUi(detailStack);

    detailStack->addWidget(sensorThDetailUi);
    detailStack->addWidget(relayDetailUi);
    detailStack->setCurrentWidget(sensorThDetailUi);

    connect(sensorThDetailUi,
            &DeviceDetailCardBaseUi::removeSlaveRequested,
            this,
            &PageStatus::removeSlaveRequested);

    connect(relayDetailUi,
            &DeviceDetailCardBaseUi::removeSlaveRequested,
            this,
            &PageStatus::removeSlaveRequested);

    connect(relayDetailUi,
            &RelayDetailCardUi::relayCommandRequested,
            this,
            [this](int masterSlot, int slaveAddr, int channel, bool on) {
                emit relayChannelCommandRequested(masterSlot,
                                                  slaveAddr,
                                                  channel,
                                                  on);

                emit relayCommandRequested(masterSlot,
                                           slaveAddr,
                                           QString("do%1").arg(channel),
                                           on);
            });

    QHBoxLayout *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(6);
    bodyLayout->addWidget(slaveListPanel);
    bodyLayout->addWidget(detailStack, 1);

    alarmLabel = new QLabel(this);
    alarmLabel->setObjectName("AlarmBar");
    alarmLabel->setFixedHeight(20);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(bodyLayout, 1);
    mainLayout->addWidget(alarmLabel);

    connect(addSlaveButton, &QPushButton::clicked, this, [this]() {
        emit addSlaveRequested(currentMasterSlot);
    });

    connect(masterCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            [this]() {
                if (!masterCombo || masterCombo->currentIndex() < 0)
                    return;

                const int masterSlot = masterCombo->currentData().toInt();

                if (masterSlot == currentMasterSlot)
                    return;

                emit masterChanged(masterSlot);
            });

    refreshMasterLabels();
    rebuildSlaveCards();
    clearCurrentDetail();
}

void PageStatus::refreshMasterLabels()
{
    const QString portName = displayMasterName();

    if (addSlaveButton)
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
    if (!sensorThDetailUi || !detailStack)
        return;

    detailStack->setCurrentWidget(sensorThDetailUi);

    sensorThDetailUi->setBaseInfo(displayMasterName(),
                                  currentMasterSlot,
                                  -1,
                                  "--",
                                  "--",
                                  false);

    sensorThDetailUi->clearData();
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

    slaveDetailDialog->setSlave(slave,
                                runtimeForSlave(slave),
                                currentMasterName);

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
        if (slaveDetailDialog->isShowingSlave(slave.masterSlot,
                                              slave.slaveAddr,
                                              slave.deviceType)) {
            slaveDetailDialog->setSlave(slave,
                                        runtimeForSlave(slave),
                                        currentMasterName);
            return;
        }
    }

    slaveDetailDialog->close();
}

QString PageStatus::runtimeKey(const SlaveDeviceInfo &slave) const
{
    return runtimeKey(slave.masterSlot,
                      slave.slaveAddr,
                      slave.deviceType);
}

QString PageStatus::runtimeKey(int masterSlot,
                               int slaveAddr,
                               const QString &deviceType) const
{
    return QString("%1:%2:%3")
    .arg(masterSlot)
        .arg(slaveAddr)
        .arg(deviceType);
}

SlaveRuntimeInfo PageStatus::runtimeForSlave(const SlaveDeviceInfo &slave) const
{
    SlaveRuntimeInfo runtime = slaveRuntime.value(runtimeKey(slave));
    runtime.online = slave.online;

    if (slave.deviceType == "relay" && runtime.relayChannels.isEmpty())
        runtime.relayChannels = defaultRelayChannelsFromOldState(runtime.ledOn,
                                                                 runtime.fanOn,
                                                                 runtime.buzzerOn);

    return runtime;
}

bool PageStatus::isCurrentSlave(int masterSlot,
                                int slaveAddr,
                                const QString &deviceType) const
{
    if (currentSlaveIndex < 0 || currentSlaveIndex >= slaves.size())
        return false;

    const SlaveDeviceInfo &slave = slaves.at(currentSlaveIndex);

    return slave.masterSlot == masterSlot &&
           slave.slaveAddr == slaveAddr &&
           slave.deviceType == deviceType;
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

    return currentMasterSlot >= 0
               ? QString("RS485-%1").arg(currentMasterSlot + 1)
               : "--";
}

QVector<RelayChannelInfo>
PageStatus::defaultRelayChannelsFromOldState(bool ledOn,
                                             bool fanOn,
                                             bool buzzerOn) const
{
    QVector<RelayChannelInfo> channels;

    RelayChannelInfo do1;
    do1.channel = 1;
    do1.key = "do1";
    do1.name = "DO1";
    do1.enabled = true;
    do1.on = ledOn;
    channels.append(do1);

    RelayChannelInfo do2;
    do2.channel = 2;
    do2.key = "do2";
    do2.name = "DO2";
    do2.enabled = true;
    do2.on = fanOn;
    channels.append(do2);

    RelayChannelInfo do3;
    do3.channel = 3;
    do3.key = "do3";
    do3.name = "DO3";
    do3.enabled = true;
    do3.on = buzzerOn;
    channels.append(do3);

    return channels;
}