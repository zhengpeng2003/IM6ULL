#pragma once
class MqttWidget;
class GpioWidget;
#include <QWidget>
class MainWidget : public QWidget
{
    Q_OBJECT
public:
    explicit MainWidget(QWidget *parent = nullptr);

    static MqttWidget *mqttWidget;
    static GpioWidget *gpioWidget;
};
