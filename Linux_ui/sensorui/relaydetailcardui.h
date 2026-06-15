// ============================
// sensor_ui/relaydetailcardui.h
// ============================

#ifndef RELAYDETAILCARDUI_H
#define RELAYDETAILCARDUI_H

#include "devicedetailcardbaseui.h"
#include "relaycontroldialog.h"

#include <QPushButton>
#include <QVector>

class RelayDetailCardUi : public DeviceDetailCardBaseUi
{
    Q_OBJECT

public:
    explicit RelayDetailCardUi(QWidget *parent = nullptr);

    void setRelayChannels(bool hasData,
                          const QVector<RelayChannelInfo> &channels,
                          const QString &updateTime);

    void setOnline(bool online);
    void clearData() override;

signals:
    void relayCommandRequested(int masterSlot,
                               int slaveAddr,
                               int channel,
                               bool on);

private:
    void setRelayMetric(MetricCard &card,
                        const RelayChannelInfo &channelInfo,
                        bool valid);

    void openControlDialog();

private:
    QPushButton *moreButton = nullptr;
    RelayControlDialog *controlDialog = nullptr;

    QVector<RelayChannelInfo> currentChannels;
};

#endif // RELAYDETAILCARDUI_H