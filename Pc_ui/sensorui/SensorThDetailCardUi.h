#pragma once

#include "sensorui/DeviceDetailCardBaseUi.h"

class QLabel;
class QVBoxLayout;
class QWidget;

class SensorThDetailCardUi : public DeviceDetailCardBaseUi
{
    Q_OBJECT
public:
    explicit SensorThDetailCardUi(QWidget *parent = nullptr);

    void setDeviceData(const RealtimeDeviceData &data) override;

private:
    void setMetric(QWidget *card, QLabel *valueLabel, const QString &value);
    void clearExtraRows();

private:
    QWidget *m_temperatureCard = nullptr;
    QWidget *m_humidityCard = nullptr;
    QLabel *m_temperatureValue = nullptr;
    QLabel *m_humidityValue = nullptr;
    QWidget *m_extraCard = nullptr;
    QVBoxLayout *m_extraLayout = nullptr;
};
