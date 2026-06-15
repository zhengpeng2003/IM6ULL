#include "slavedetaildialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "../sensorui/relaydetailcardui.h"
#include "../sensorui/sensorthdetailcardui.h"

SlaveDetailDialog::SlaveDetailDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("SlavePopup");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(false);
    setFixedSize(330, 220);

    QLabel *titleLabel = new QLabel("从站详细信息", this);
    titleLabel->setObjectName("PopupTitle");

    QPushButton *closeButton = new QPushButton("×", this);
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
    sensorThUi = new SensorThDetailCardUi(detailStack);
    relayUi = new RelayDetailCardUi(detailStack);
    detailStack->addWidget(sensorThUi);
    detailStack->addWidget(relayUi);

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
    const QString portName = masterName.isEmpty()
        ? QString("RS485-%1").arg(slave.masterSlot + 1)
        : masterName;
    const QString typeName = displayNameForSlave(slave);

    if (slave.deviceType == "relay") {
        relayUi->setBaseInfo(portName,
                             slave.masterSlot,
                             slave.slaveAddr,
                             typeName,
                             slave.deviceType,
                             runtime.online);
        relayUi->setPollInterval(slave.pollIntervalMs);
        relayUi->setRelayChannels(runtime.hasRelay,
                                  relayChannelsForRuntime(runtime),
                                  runtime.updateTime);
        detailStack->setCurrentWidget(relayUi);
        return;
    }

    sensorThUi->setBaseInfo(portName,
                            slave.masterSlot,
                            slave.slaveAddr,
                            typeName,
                            slave.deviceType,
                            runtime.online);
    sensorThUi->setPollInterval(slave.pollIntervalMs);
    sensorThUi->clearData();

    if (slave.deviceType == "sensor_th") {
        sensorThUi->setTemperatureHumidity(runtime.hasSensorTh,
                                           runtime.temperature,
                                           runtime.humidity,
                                           runtime.updateTime);
    }

    detailStack->setCurrentWidget(sensorThUi);
}

QString SlaveDetailDialog::displayTypeName(const QString &deviceType) const
{
    if (deviceType == "sensor_th")
        return "温湿度传感器";
    if (deviceType == "relay")
        return "继电器";
    if (deviceType == "meter")
        return "电表";
    return deviceType;
}

QString SlaveDetailDialog::displayNameForSlave(const SlaveDeviceInfo &slave) const
{
    return slave.displayName.isEmpty() ? displayTypeName(slave.deviceType) : slave.displayName;
}

QVector<RelayChannelInfo> SlaveDetailDialog::relayChannelsForRuntime(const SlaveRuntimeInfo &runtime) const
{
    if (!runtime.relayChannels.isEmpty())
        return runtime.relayChannels;

    QVector<RelayChannelInfo> channels;
    const bool states[] = { runtime.ledOn, runtime.fanOn, runtime.buzzerOn };
    for (int i = 0; i < 3; ++i) {
        RelayChannelInfo info;
        info.channel = i + 1;
        info.key = QString("do%1").arg(i + 1);
        info.name = QString("DO%1").arg(i + 1);
        info.enabled = true;
        info.on = states[i];
        channels.append(info);
    }
    return channels;
}
