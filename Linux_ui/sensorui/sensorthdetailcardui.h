// ============================
// sensor_ui/sensorthdetailcardui.h
// ============================

#ifndef SENSORTHDETAILCARDUI_H
#define SENSORTHDETAILCARDUI_H

#include "devicedetailcardbaseui.h"

class SensorThDetailCardUi : public DeviceDetailCardBaseUi
{
    Q_OBJECT

public:
    explicit SensorThDetailCardUi(QWidget *parent = nullptr);

    void setTemperatureHumidity(bool hasData,
                                double temperature,
                                double humidity,
                                const QString &updateTime);

    void clearData() override;
};

#endif // SENSORTHDETAILCARDUI_H