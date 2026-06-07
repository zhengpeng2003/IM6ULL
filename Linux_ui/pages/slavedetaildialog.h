#pragma once

#include <QDialog>

#include "PageStatus.h"

class MeterUi;
class RelayUi;
class SensorThUi;
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

    QStackedWidget *detailStack = nullptr;
    SensorThUi *sensorThUi = nullptr;
    RelayUi *relayUi = nullptr;
    MeterUi *meterUi = nullptr;
    SlaveDeviceInfo currentSlave;
    bool hasCurrentSlave = false;
};
