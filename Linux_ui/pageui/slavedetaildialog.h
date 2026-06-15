#pragma once

#include <QDialog>

#include "../pages/PageStatus.h"

class RelayDetailCardUi;
class SensorThDetailCardUi;
class QStackedWidget;

class SlaveDetailDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SlaveDetailDialog(QWidget *parent = nullptr);

    void setSlave(const SlaveDeviceInfo &slave,
                  const SlaveRuntimeInfo &runtime,
                  const QString &masterName);
    bool isShowingSlave(int masterSlot, int slaveAddr, const QString &deviceType) const;

private:
    void applyRuntime(const SlaveDeviceInfo &slave,
                      const SlaveRuntimeInfo &runtime,
                      const QString &masterName);
    QString displayTypeName(const QString &deviceType) const;
    QString displayNameForSlave(const SlaveDeviceInfo &slave) const;
    QVector<RelayChannelInfo> relayChannelsForRuntime(const SlaveRuntimeInfo &runtime) const;

    QStackedWidget *detailStack = nullptr;
    SensorThDetailCardUi *sensorThUi = nullptr;
    RelayDetailCardUi *relayUi = nullptr;
    SlaveDeviceInfo currentSlave;
    bool hasCurrentSlave = false;
};
