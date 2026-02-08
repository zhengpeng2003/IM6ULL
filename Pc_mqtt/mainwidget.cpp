#include "MainWidget.h"

#include "MqttWidget.h"
#include "GpioWidget.h"
#include <QVBoxLayout>
MqttWidget* MainWidget::mqttWidget = nullptr;
GpioWidget* MainWidget::gpioWidget = nullptr;
MainWidget::MainWidget(QWidget *parent)
    : QWidget(parent)
{
    mqttWidget = new MqttWidget(this);
    gpioWidget = new GpioWidget(this);
    connect(mqttWidget,&MqttWidget::S_thDataUpdated,gpioWidget,&GpioWidget::tempupdate);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 上面 1
    mainLayout->addWidget(mqttWidget, 1);

    // 下面 3
    mainLayout->addWidget(gpioWidget, 3);

    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    setLayout(mainLayout);
}
