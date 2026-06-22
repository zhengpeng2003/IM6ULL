#pragma once

#include "sensorui/DeviceDetailCardBaseUi.h"

#include <QMap>
#include <QSet>
#include <QVariant>

class QVBoxLayout;

class RelayDetailCardUi : public DeviceDetailCardBaseUi
{
    Q_OBJECT
public:
    explicit RelayDetailCardUi(QWidget *parent = nullptr);

    void setDeviceData(const RealtimeDeviceData &data) override;
    void setPendingRelayChannels(const QSet<QString> &channels);

signals:
    void relayCommandRequested(const DeviceNode &node,
                               const QString &channel,
                               bool on,
                               const QVariantMap &channels);

private:
    void clearRows();
    QVariantMap channelsToVariantMap(const QMap<QString, bool> &channels) const;
    QMap<QString, QString> pointNamesByChannel(const RealtimeDeviceData &data) const;

private:
    QWidget *m_channelCard = nullptr;
    QVBoxLayout *m_channelLayout = nullptr;
    RealtimeDeviceData m_data;
    QSet<QString> m_pendingChannels;
};
