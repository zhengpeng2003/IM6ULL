#include "slavedetaildialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "sensorui/sensorui.h"

SlaveDetailDialog::SlaveDetailDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("SlavePopup");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(false);
    setFixedSize(330, 220);

    QLabel *titleLabel = new QLabel("Slave Detail", this);
    titleLabel->setObjectName("PopupTitle");

    QPushButton *closeButton = new QPushButton("X", this);
    closeButton->setObjectName("PopupCloseButton");
    closeButton->setFixedSize(24, 22);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeButton);

    detailStack = new QStackedWidget(this);
    detailStack->setObjectName("PopupDetailStack");
    sensorThUi = new SensorThUi(this);
    relayUi = new RelayUi(this);
    relayUi->setControlsVisible(false);
    meterUi = new MeterUi(this);
    detailStack->addWidget(sensorThUi);
    detailStack->addWidget(relayUi);
    detailStack->addWidget(meterUi);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 7, 8, 8);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(detailStack, 1);
}

void SlaveDetailDialog::setSlave(const SlaveDeviceInfo &slave,
                                 const SlaveRuntimeInfo &runtime,
                                 const QString &masterName)
{
    currentSlave = slave;
    hasCurrentSlave = true;
    applyRuntime(slave, runtime, masterName);
}

bool SlaveDetailDialog::isShowingSlave(int masterSlot, int slaveAddr, const QString &deviceType) const
{
    return hasCurrentSlave &&
           currentSlave.masterSlot == masterSlot &&
           currentSlave.slaveAddr == slaveAddr &&
           currentSlave.deviceType == deviceType;
}

void SlaveDetailDialog::applyRuntime(const SlaveDeviceInfo &slave,
                                     const SlaveRuntimeInfo &runtime,
                                     const QString &masterName)
{
    if (slave.deviceType == "sensor_th") {
        sensorThUi->setDeviceInfo(slave.deviceName, masterName, slave.slaveAddr);
        sensorThUi->clearData();
        sensorThUi->setOnline(runtime.online);
        if (runtime.hasSensorTh)
            sensorThUi->setTemperatureHumidity(runtime.temperature, runtime.humidity, runtime.updateTime);
        detailStack->setCurrentWidget(sensorThUi);
    } else if (slave.deviceType == "relay") {
        relayUi->setDeviceInfo(slave.deviceName, masterName, slave.masterSlot, slave.slaveAddr);
        relayUi->clearData();
        relayUi->setOnline(runtime.online);
        if (runtime.hasRelay)
            relayUi->setRelayStates(runtime.ledOn, runtime.fanOn, runtime.buzzerOn, runtime.updateTime);
        detailStack->setCurrentWidget(relayUi);
    } else {
        meterUi->setDeviceInfo(slave.deviceName, masterName, slave.slaveAddr);
        meterUi->clearData();
        meterUi->setOnline(runtime.online);
        if (runtime.hasMeter) {
            meterUi->setMeterValues(runtime.voltage,
                                    runtime.current,
                                    runtime.power,
                                    runtime.energy,
                                    runtime.updateTime);
        }
        detailStack->setCurrentWidget(meterUi);
    }
}
