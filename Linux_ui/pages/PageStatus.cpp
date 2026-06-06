#include "PageStatus.h"

#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

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

void PageStatus::setMasterList(const QStringList &masters)
{
    const QString current = masterCombo->currentText();
    masterCombo->blockSignals(true);
    masterCombo->clear();
    masterCombo->addItems(masters);
    const int index = masterCombo->findText(current);
    if (index >= 0)
        masterCombo->setCurrentIndex(index);
    masterCombo->blockSignals(false);

    if (masterCombo->count() > 0) {
        currentMasterSlot = masterCombo->currentIndex();
        currentMasterName = masterCombo->currentText();
    } else {
        currentMasterSlot = -1;
        currentMasterName.clear();
    }
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
    currentSlaveIndex = slaves.isEmpty() ? -1 : 0;
    rebuildSlaveCards();
    refreshMasterLabels();
    if (currentSlaveIndex >= 0)
        selectSlave(currentSlaveIndex);
}

void PageStatus::selectSlave(int index)
{
    if (index < 0 || index >= slaves.size())
        return;

    currentSlaveIndex = index;
    refreshSlaveCards();

    const SlaveDeviceInfo &slave = slaves.at(index);
    if (slave.deviceType == "sensor_th") {
        sensorThUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.slaveAddr);
        sensorThUi->clearData();
        sensorThUi->setOnline(slave.online);
        detailStack->setCurrentWidget(sensorThUi);
    } else if (slave.deviceType == "relay") {
        relayUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.masterSlot, slave.slaveAddr);
        relayUi->clearData();
        relayUi->setOnline(slave.online);
        detailStack->setCurrentWidget(relayUi);
    } else if (slave.deviceType == "meter") {
        meterUi->setDeviceInfo(slave.deviceName, currentMasterName, slave.slaveAddr);
        meterUi->clearData();
        meterUi->setOnline(slave.online);
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
    if (isCurrentSlave(masterSlot, slaveAddr, "sensor_th"))
        sensorThUi->setTemperatureHumidity(temperature, humidity, updateTime);
}

void PageStatus::setRelayStates(int masterSlot,
                                int slaveAddr,
                                bool ledOn,
                                bool fanOn,
                                bool buzzerOn,
                                const QString &updateTime)
{
    if (isCurrentSlave(masterSlot, slaveAddr, "relay"))
        relayUi->setRelayStates(ledOn, fanOn, buzzerOn, updateTime);
}

void PageStatus::setMeterValues(int masterSlot,
                                int slaveAddr,
                                const QString &voltage,
                                const QString &current,
                                const QString &power,
                                const QString &energy,
                                const QString &updateTime)
{
    if (isCurrentSlave(masterSlot, slaveAddr, "meter"))
        meterUi->setMeterValues(voltage, current, power, energy, updateTime);
}

bool PageStatus::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (int i = 0; i < slaveCards.size(); ++i) {
            if (watched == slaveCards.at(i).frame) {
                selectSlave(i);
                return true;
            }
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

    QHBoxLayout *masterRow = new QHBoxLayout;
    masterRow->setContentsMargins(0, 0, 0, 0);
    masterRow->setSpacing(5);
    masterRow->addWidget(masterLabel);
    masterRow->addWidget(masterCombo);
    masterRow->addWidget(slaveCountLabel);
    masterRow->addStretch();
    masterRow->addWidget(addSlaveButton);

    QFrame *slaveListPanel = new QFrame(this);
    slaveListPanel->setObjectName("Panel");
    slaveListPanel->setFixedWidth(150);

    QLabel *listTitle = new QLabel("Slave List", slaveListPanel);
    listTitle->setObjectName("PanelTitle");

    emptyListLabel = new QLabel("No slave devices", slaveListPanel);
    emptyListLabel->setObjectName("HintText");
    emptyListLabel->setAlignment(Qt::AlignCenter);

    slaveListLayout = new QVBoxLayout(slaveListPanel);
    slaveListLayout->setContentsMargins(5, 5, 5, 5);
    slaveListLayout->setSpacing(4);
    slaveListLayout->addWidget(listTitle);

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
        currentMasterSlot = index;
        currentMasterName = masterCombo->currentText();
        refreshMasterLabels();
        if (currentSlaveIndex >= 0)
            selectSlave(currentSlaveIndex);
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
}

void PageStatus::refreshSlaveCards()
{
    for (int i = 0; i < slaveCards.size(); ++i)
        updateSlaveCardStyle(i, i == currentSlaveIndex);
}

void PageStatus::rebuildSlaveCards()
{
    while (slaveListLayout->count() > 1) {
        QLayoutItem *item = slaveListLayout->takeAt(1);
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
    QFrame *frame = new QFrame(this);
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

bool PageStatus::isCurrentSlave(int masterSlot, int slaveAddr, const QString &deviceType) const
{
    if (currentSlaveIndex < 0 || currentSlaveIndex >= slaves.size())
        return false;

    const SlaveDeviceInfo &slave = slaves.at(currentSlaveIndex);
    return slave.masterSlot == masterSlot &&
           slave.slaveAddr == slaveAddr &&
           slave.deviceType == deviceType;
}
