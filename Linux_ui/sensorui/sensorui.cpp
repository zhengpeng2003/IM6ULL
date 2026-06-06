#include "sensorui.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

DetailBaseUi::DetailBaseUi(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    setObjectName("DetailPanel");

    QLabel *titleLabel = new QLabel(title, this);
    titleLabel->setObjectName("PanelTitle");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(7, 5, 7, 5);
    layout->setSpacing(4);
    layout->addWidget(titleLabel);
}

QLabel *DetailBaseUi::addRow(QGridLayout *grid, int row, const QString &key, const QString &value)
{
    QLabel *keyLabel = new QLabel(key, this);
    keyLabel->setObjectName("DetailKey");

    QLabel *valueLabel = new QLabel(value, this);
    valueLabel->setObjectName("DetailValue");
    valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    grid->addWidget(keyLabel, row, 0);
    grid->addWidget(valueLabel, row, 1);
    return valueLabel;
}

void DetailBaseUi::setStateLabel(QLabel *label, bool online)
{
    label->setText(online ? "Online" : "Offline");
    label->setProperty("state", online ? "online" : "offline");
    label->style()->unpolish(label);
    label->style()->polish(label);
}

SensorThUi::SensorThUi(QWidget *parent)
    : DetailBaseUi("Slave Detail", parent)
{
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(1, 1);

    deviceNameLabel = addRow(grid, 0, "Device:");
    masterNameLabel = addRow(grid, 1, "Master:");
    slaveAddrLabel = addRow(grid, 2, "Addr:");
    addRow(grid, 3, "Type:", "sensor_th");
    stateLabel = addRow(grid, 4, "State:");
    temperatureLabel = addRow(grid, 5, "Temp:");
    humidityLabel = addRow(grid, 6, "Humidity:");
    updateTimeLabel = addRow(grid, 7, "Update:");

    qobject_cast<QVBoxLayout *>(layout())->addLayout(grid);
    qobject_cast<QVBoxLayout *>(layout())->addStretch();
    clearData();
}

void SensorThUi::setDeviceInfo(const QString &deviceName, const QString &masterName, int slaveAddr)
{
    deviceNameLabel->setText(deviceName);
    masterNameLabel->setText(masterName);
    slaveAddrLabel->setText(QString::number(slaveAddr));
}

void SensorThUi::setOnline(bool online)
{
    setStateLabel(stateLabel, online);
    if (!online)
        clearData();
}

void SensorThUi::setTemperatureHumidity(double temperature, double humidity, const QString &updateTime)
{
    temperatureLabel->setText(QString("%1 C").arg(temperature, 0, 'f', 1));
    humidityLabel->setText(QString("%1 %").arg(humidity, 0, 'f', 1));
    updateTimeLabel->setText(updateTime);
}

void SensorThUi::clearData()
{
    temperatureLabel->setText("--");
    humidityLabel->setText("--");
    updateTimeLabel->setText("--");
}

RelayUi::RelayUi(QWidget *parent)
    : DetailBaseUi("Slave Detail", parent)
{
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(1);
    grid->setColumnStretch(1, 1);

    deviceNameLabel = addRow(grid, 0, "Device:");
    masterNameLabel = addRow(grid, 1, "Master:");
    slaveAddrLabel = addRow(grid, 2, "Addr:");
    addRow(grid, 3, "Type:", "relay");
    stateLabel = addRow(grid, 4, "State:");
    ledLabel = addRow(grid, 5, "LED:");
    fanLabel = addRow(grid, 6, "FAN:");
    buzzerLabel = addRow(grid, 7, "BUZZER:");
    updateTimeLabel = addRow(grid, 8, "Update:");

    QWidget *ledRow = createButtonRow("LED", "led", ledOnButton, ledOffButton);
    QWidget *fanRow = createButtonRow("FAN", "fan", fanOnButton, fanOffButton);
    QWidget *buzzerRow = createButtonRow("BUZZER", "buzzer", buzzerOnButton, buzzerOffButton);

    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());
    mainLayout->addLayout(grid);
    mainLayout->addWidget(ledRow);
    mainLayout->addWidget(fanRow);
    mainLayout->addWidget(buzzerRow);
    mainLayout->addStretch();
    clearData();
    setControlEnabled(false);
}

void RelayUi::setDeviceInfo(const QString &deviceName,
                            const QString &masterName,
                            int masterSlot,
                            int slaveAddr)
{
    currentMasterSlot = masterSlot;
    currentSlaveAddr = slaveAddr;
    deviceNameLabel->setText(deviceName);
    masterNameLabel->setText(masterName);
    slaveAddrLabel->setText(QString::number(slaveAddr));
}

void RelayUi::setOnline(bool online)
{
    setStateLabel(stateLabel, online);
    setControlEnabled(online);
    if (!online)
        clearData();
}

void RelayUi::setRelayStates(bool ledOn, bool fanOn, bool buzzerOn, const QString &updateTime)
{
    ledLabel->setText(ledOn ? "On" : "Off");
    fanLabel->setText(fanOn ? "On" : "Off");
    buzzerLabel->setText(buzzerOn ? "On" : "Off");
    updateTimeLabel->setText(updateTime);
}

void RelayUi::clearData()
{
    ledLabel->setText("--");
    fanLabel->setText("--");
    buzzerLabel->setText("--");
    updateTimeLabel->setText("--");
}

QWidget *RelayUi::createButtonRow(const QString &channelText,
                                  const QString &channel,
                                  QPushButton *&onButton,
                                  QPushButton *&offButton)
{
    QWidget *row = new QWidget(this);
    row->setObjectName("ControlRow");

    QLabel *label = new QLabel(channelText, row);
    label->setObjectName("DetailKey");

    onButton = new QPushButton(channelText + " On", row);
    offButton = new QPushButton(channelText + " Off", row);
    onButton->setObjectName("SmallActionButton");
    offButton->setObjectName("SmallGhostButton");

    QHBoxLayout *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(4);
    rowLayout->addWidget(label);
    rowLayout->addStretch();
    rowLayout->addWidget(onButton);
    rowLayout->addWidget(offButton);

    connect(onButton, &QPushButton::clicked, this, [this, channel]() {
        emit relayCommandRequested(currentMasterSlot, currentSlaveAddr, channel, true);
    });
    connect(offButton, &QPushButton::clicked, this, [this, channel]() {
        emit relayCommandRequested(currentMasterSlot, currentSlaveAddr, channel, false);
    });

    return row;
}

void RelayUi::setControlEnabled(bool enabled)
{
    const QList<QPushButton *> buttons = {
        ledOnButton, ledOffButton, fanOnButton, fanOffButton, buzzerOnButton, buzzerOffButton
    };
    for (QPushButton *button : buttons) {
        if (button)
            button->setEnabled(enabled);
    }
}

MeterUi::MeterUi(QWidget *parent)
    : DetailBaseUi("Slave Detail", parent)
{
    QGridLayout *grid = new QGridLayout;
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(5);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(1, 1);

    deviceNameLabel = addRow(grid, 0, "Device:");
    masterNameLabel = addRow(grid, 1, "Master:");
    slaveAddrLabel = addRow(grid, 2, "Addr:");
    addRow(grid, 3, "Type:", "meter");
    stateLabel = addRow(grid, 4, "State:");
    voltageLabel = addRow(grid, 5, "Voltage:");
    currentLabel = addRow(grid, 6, "Current:");
    powerLabel = addRow(grid, 7, "Power:");
    energyLabel = addRow(grid, 8, "Energy:");
    updateTimeLabel = addRow(grid, 9, "Update:");

    qobject_cast<QVBoxLayout *>(layout())->addLayout(grid);
    qobject_cast<QVBoxLayout *>(layout())->addStretch();
    clearData();
}

void MeterUi::setDeviceInfo(const QString &deviceName, const QString &masterName, int slaveAddr)
{
    deviceNameLabel->setText(deviceName);
    masterNameLabel->setText(masterName);
    slaveAddrLabel->setText(QString::number(slaveAddr));
}

void MeterUi::setOnline(bool online)
{
    setStateLabel(stateLabel, online);
    if (!online)
        clearData();
}

void MeterUi::setMeterValues(const QString &voltage,
                             const QString &current,
                             const QString &power,
                             const QString &energy,
                             const QString &updateTime)
{
    voltageLabel->setText(voltage);
    currentLabel->setText(current);
    powerLabel->setText(power);
    energyLabel->setText(energy);
    updateTimeLabel->setText(updateTime);
}

void MeterUi::clearData()
{
    voltageLabel->setText("--");
    currentLabel->setText("--");
    powerLabel->setText("--");
    energyLabel->setText("--");
    updateTimeLabel->setText("--");
}
