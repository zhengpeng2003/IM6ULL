// ============================
// sensor_ui/relaydetailcardui.cpp
// ============================

#include "relaydetailcardui.h"

#include <QHBoxLayout>

RelayDetailCardUi::RelayDetailCardUi(QWidget *parent)
    : DeviceDetailCardBaseUi(parent)
{
    moreButton = new QPushButton("更多通道 / 控制", this);
    moreButton->setObjectName("SmallActionButton");
    moreButton->setFixedWidth(110);
    moreButton->setVisible(false);

    QHBoxLayout *row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->addStretch();
    row->addWidget(moreButton);

    extraLayout()->addLayout(row);

    connect(moreButton,
            &QPushButton::clicked,
            this,
            &RelayDetailCardUi::openControlDialog);

    clearData();
}

void RelayDetailCardUi::setRelayChannels(bool hasData,
                                         const QVector<RelayChannelInfo> &channels,
                                         const QString &updateTime)
{
    currentChannels = channels;

    setRelayMetric(metricA, channels.value(0), hasData && channels.size() > 0);
    setRelayMetric(metricB, channels.value(1), hasData && channels.size() > 1);
    setRelayMetric(metricC, channels.value(2), hasData && channels.size() > 2);
    setRelayMetric(metricD, channels.value(3), hasData && channels.size() > 3);

    setMetricVisible(channels.size() > 0,
                     channels.size() > 1,
                     channels.size() > 2,
                     channels.size() > 3);

    moreButton->setVisible(channels.size() > 4);
    moreButton->setEnabled(detailStateLabel->text() == "在线");

    setLastUpdateTime(updateTime);

    if (controlDialog && controlDialog->isVisible()) {
        controlDialog->setRelayInfo(currentMasterSlot,
                                    currentSlaveAddr,
                                    detailPortLabel->text(),
                                    currentChannels);
    }
}

void RelayDetailCardUi::setRelayMetric(MetricCard &card,
                                       const RelayChannelInfo &channelInfo,
                                       bool valid)
{
    const QString name = channelInfo.name.isEmpty()
    ? QString("DO%1").arg(channelInfo.channel)
    : channelInfo.name;

    const QString icon = channelInfo.channel > 0
                             ? QString::number(channelInfo.channel)
                             : "D";

    setMetricCard(card,
                  icon,
                  name,
                  valid ? (channelInfo.on ? "开启" : "关闭") : "--",
                  "");
}

void RelayDetailCardUi::setOnline(bool online)
{
    DeviceDetailCardBaseUi::setOnline(online);

    if (moreButton)
        moreButton->setEnabled(online);
}

void RelayDetailCardUi::clearData()
{
    setMetricCard(metricA, "1", "DO1", "--", "");
    setMetricCard(metricB, "2", "DO2", "--", "");
    setMetricCard(metricC, "3", "DO3", "--", "");
    setMetricCard(metricD, "4", "DO4", "--", "");

    setMetricVisible(true, true, false, false);

    if (moreButton)
        moreButton->setVisible(false);

    setLastUpdateTime("--");
}

void RelayDetailCardUi::openControlDialog()
{
    if (!controlDialog) {
        controlDialog = new RelayControlDialog(this);

        connect(controlDialog,
                &RelayControlDialog::relayCommandRequested,
                this,
                &RelayDetailCardUi::relayCommandRequested);
    }

    controlDialog->setRelayInfo(currentMasterSlot,
                                currentSlaveAddr,
                                detailPortLabel->text(),
                                currentChannels);

    controlDialog->show();
    controlDialog->raise();
    controlDialog->activateWindow();
}