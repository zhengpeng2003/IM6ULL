// ============================
// sensor_ui/sensorthdetailcardui.cpp
// ============================

#include "sensorthdetailcardui.h"

SensorThDetailCardUi::SensorThDetailCardUi(QWidget *parent)
    : DeviceDetailCardBaseUi(parent)
{
    clearData();
}

void SensorThDetailCardUi::setTemperatureHumidity(bool hasData,
                                                  double temperature,
                                                  double humidity,
                                                  const QString &updateTime)
{
    setMetricCard(metricA,
                  "T",
                  "温度",
                  hasData ? QString("%1").arg(temperature, 0, 'f', 1) : "--",
                  hasData ? "℃" : "");

    setMetricCard(metricB,
                  "H",
                  "湿度",
                  hasData ? QString("%1").arg(humidity, 0, 'f', 1) : "--",
                  hasData ? "%RH" : "");

    setMetricVisible(true, true, false, false);
    setLastUpdateTime(updateTime);
}

void SensorThDetailCardUi::clearData()
{
    setMetricCard(metricA, "T", "温度", "--", "");
    setMetricCard(metricB, "H", "湿度", "--", "");
    setMetricCard(metricC, "R", "状态", "--", "");
    setMetricCard(metricD, "P", "状态", "--", "");

    setMetricVisible(true, true, false, false);
    setLastUpdateTime("--");
}