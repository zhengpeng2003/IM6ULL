#include "PageStatus.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

#include "slavedetaildialog.h"
#include "slavelistdialog.h"
#include "sensorui/sensorui.h"

PageStatus::PageStatus(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();
    setMasterSummary(0, 0, 0, "--");
    setAlarmText("Alarm: --");
}

void PageStatus::setMasterSummary(int masterCount,
                                  int onlineSlaveCount,
                                  int alarmCount,
                                  const QString &mqttState)
{
    summaryLabel->setText(QString("Masters:%1   Online:%2   Alarms:%3   MQTT:%4")
                              .arg(masterCount)
                              .arg(onlineSlaveCount)
                              .arg(alarmCount)
                              .arg(mqttState));
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

    addSlaveButton->setEnabled(currentMasterSlot >= 0);
    refreshMasterLabels();
}

void PageStatus::setCurrentMaster(const QString &masterName, int slaveCount)
{
    currentMasterName = masterName;
    const int comboIndex = masterCombo->findText(masterName);
    if (comboIndex >= 0)
        masterCombo->setCurrentIndex(comboIndex);

    slaveCountLabel->setText(QString("Slaves:%1").arg(slaveCount));
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

        if (i == currentSlaveIndex) {
            if (deviceType == "sensor_th")
                sensorThUi->setOnline(online);
            else if (deviceType == "relay")
                relayUi->setOnline(online);
            else if (deviceType == "meter")
                meterUi->setOnline(online);
        }
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
    if (slave.deviceType == "sensor_th") {
        sensorThUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.slaveAddr);
        sensorThUi->clearData();
        sensorThUi->setOnline(slave.online);
        if (runtime.hasSensorTh)
            sensorThUi->setTemperatureHumidity(runtime.temperature, runtime.humidity, runtime.updateTime);
        detailStack->setCurrentWidget(sensorThUi);
    } else if (slave.deviceType == "relay") {
        relayUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.masterSlot, slave.slaveAddr);
        relayUi->clearData();
        relayUi->setOnline(slave.online);
        if (runtime.hasRelay)
            relayUi->setRelayStates(runtime.ledOn, runtime.fanOn, runtime.buzzerOn, runtime.updateTime);
        detailStack->setCurrentWidget(relayUi);
    } else if (slave.deviceType == "meter") {
        meterUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.slaveAddr);
        meterUi->clearData();
        meterUi->setOnline(slave.online);
        if (runtime.hasMeter) {
            meterUi->setMeterValues(runtime.voltage,
                                    runtime.current,
                                    runtime.power,
                                    runtime.energy,
                                    runtime.updateTime);
        }
        detailStack->setCurrentWidget(meterUi);
    }

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
        sensorThUi->setTemperatureHumidity(temperature, humidity, updateTime);
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
        relayUi->setRelayStates(ledOn, fanOn, buzzerOn, updateTime);
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
        meterUi->setMeterValues(voltage, current, power, energy, updateTime);
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
    summaryLabel->setFixedHeight(22);

    QLabel *masterLabel = new QLabel("Master:", this);
    masterLabel->setObjectName("DetailKey");

    masterCombo = new QComboBox(this);
    masterCombo->setObjectName("CompactCombo");
    masterCombo->setFixedWidth(138);

    slaveCountLabel = new QLabel(this);
    slaveCountLabel->setObjectName("DetailValue");

    addSlaveButton = new QPushButton("+ Add Slave", this);
    addSlaveButton->setObjectName("ActionButton");
    addSlaveButton->setFixedWidth(76);
    addSlaveButton->setEnabled(false);

    QHBoxLayout *masterRow = new QHBoxLayout;
    masterRow->setContentsMargins(0, 0, 0, 0);
    masterRow->setSpacing(5);
    masterRow->addWidget(masterLabel);
    masterRow->addWidget(masterCombo);
    masterRow->addWidget(slaveCountLabel);
    masterRow->addStretch();
    masterRow->addWidget(addSlaveButton);

    slaveListPanel = new QFrame(this);
    slaveListPanel->setObjectName("Panel");
    slaveListPanel->setFixedWidth(150);
    slaveListPanel->setCursor(Qt::PointingHandCursor);
    slaveListPanel->installEventFilter(this);

    listTitleLabel = new QLabel("Slave List (0)", slaveListPanel);
    listTitleLabel->setObjectName("PanelTitle");
    listTitleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    emptyListLabel = new QLabel("No slave devices", slaveListPanel);
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
    slavePanelLayout->setContentsMargins(5, 5, 5, 5);
    slavePanelLayout->setSpacing(4);
    slavePanelLayout->addWidget(listTitleLabel);
    slavePanelLayout->addWidget(slaveScrollArea, 1);

    detailStack = new QStackedWidget(this);
    detailStack->setObjectName("DetailStack");
    sensorThUi = new SensorThUi(this);
    relayUi = new RelayUi(this);
    meterUi = new MeterUi(this);
    detailStack->addWidget(sensorThUi);
    detailStack->addWidget(relayUi);
    detailStack->addWidget(meterUi);

    connect(relayUi, &RelayUi::relayCommandRequested,
            this, &PageStatus::relayCommandRequested);

    QHBoxLayout *bodyLayout = new QHBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(6);
    bodyLayout->addWidget(slaveListPanel);
    bodyLayout->addWidget(detailStack, 1);

    alarmLabel = new QLabel(this);
    alarmLabel->setObjectName("AlarmBar");
    alarmLabel->setFixedHeight(22);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);
    mainLayout->addWidget(summaryLabel);
    mainLayout->addLayout(masterRow);
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
}

void PageStatus::refreshMasterLabels()
{
    slaveCountLabel->setText(QString("Slaves:%1").arg(slaves.size()));
    if (listTitleLabel)
        listTitleLabel->setText(QString("Slave List (%1)").arg(slaves.size()));
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
    sensorThUi->setDeviceInfo("--", "--", 0);
    sensorThUi->setOnline(false);
    sensorThUi->clearData();

    relayUi->setDeviceInfo("--", "--", -1, 0);
    relayUi->setOnline(false);
    relayUi->clearData();

    meterUi->setDeviceInfo("--", "--", 0);
    meterUi->setOnline(false);
    meterUi->clearData();
}

void PageStatus::updateSlaveCardStyle(int index, bool selected)
{
    if (index < 0 || index >= slaveCards.size())
        return;

    const SlaveDeviceInfo &slave = slaves.at(index);
    SlaveCard &card = slaveCards[index];
    card.frame->setProperty("selected", selected);
    card.title->setText(QString("%1[%2] %3")
                            .arg(selected ? "> " : "  ")
                            .arg(slave.slaveAddr)
                            .arg(slave.displayName.isEmpty() ? slave.deviceType : slave.displayName));
    card.state->setText(slave.online ? "Online" : "Offline");
    card.state->setProperty("state", slave.online ? "online" : "offline");

    card.frame->style()->unpolish(card.frame);
    card.frame->style()->polish(card.frame);
    card.title->style()->unpolish(card.title);
    card.title->style()->polish(card.title);
    card.state->style()->unpolish(card.state);
    card.state->style()->polish(card.state);
}

QFrame *PageStatus::createSlaveCard(int index)
{
    QFrame *frame = new QFrame(slaveScrollArea);
    frame->setObjectName("SlaveCard");
    frame->setFixedHeight(38);

    QLabel *title = new QLabel(frame);
    title->setObjectName("SlaveTitle");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);
    QLabel *state = new QLabel(frame);
    state->setObjectName("SlaveState");
    state->setAttribute(Qt::WA_TransparentForMouseEvents);
    frame->setCursor(Qt::PointingHandCursor);
    frame->installEventFilter(this);

    QVBoxLayout *textLayout = new QVBoxLayout(frame);
    textLayout->setContentsMargins(7, 3, 7, 3);
    textLayout->setSpacing(1);
    textLayout->addWidget(title);
    textLayout->addWidget(state);

    SlaveCard card;
    card.frame = frame;
    card.title = title;
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
