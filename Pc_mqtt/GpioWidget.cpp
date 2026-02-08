#include "GpioWidget.h"
#include "MqttWidget.h"
#include "ui_GpioWidget.h"
#include "mybtn.h"

#include <QGridLayout>
#include <QCoreApplication>
#include <QDir>
#include <QFile>

GpioWidget::GpioWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::GpioWidget)
{
    ui->setupUi(this);

    m_gridLayout = new QGridLayout(this);
    m_gridLayout->setSpacing(10);
    m_gridLayout->setContentsMargins(10,10,10,10);

    InitMyBtns();
    InitLayout();
}

GpioWidget::~GpioWidget()
{
    delete ui;
}

void GpioWidget::InitMyBtns()
{
    // 创建按钮
    MyBtn *ledBtn =
        new MyBtn(":/images/ledoff.png",
                  ":/images/ledon.png",
                  this);

    MyBtn *buzzerBtn =
        new MyBtn(":/images/buzzeroff.png",
                  ":/images/buzzeron.png",
                  this);

    MyBtn *exitBtn =
        new MyBtn(":/images/exit.png",
                  ":/images/exit.png",
                  this);

    tempWidget = new TempInfoWidget(this);

    // 退出按钮
    connect(exitBtn, &QPushButton::clicked,
            this, &GpioWidget::onExitButtonClicked);

    //------------------- 信号连接到 MQTT Widget -------------------
    //LED 按钮点击
    connect(ledBtn, &MyBtn::gpioClicked,
            MainWidget::mqttWidget, &MqttWidget::onLedButtonClicked);

    connect(buzzerBtn, &MyBtn::gpioClicked,
            MainWidget::mqttWidget, &MqttWidget::onBuzzerButtonClicked);

    // ACK 回来更新按钮状态
    connect(MainWidget::mqttWidget, &MqttWidget::ledAckReceived,
            ledBtn, &MyBtn::setState);

    connect(MainWidget::mqttWidget, &MqttWidget::buzzerAckReceived,
            buzzerBtn, &MyBtn::setState);

    // 添加按钮到列表管理
    m_buttons << ledBtn << buzzerBtn << exitBtn;
}

void GpioWidget::InitLayout()
{
    const int BTN_W = 150;
    const int BTN_H = 80;

    // LED
    m_buttons[0]->setFixedSize(BTN_W, BTN_H);
    m_gridLayout->addWidget(m_buttons[0], 0, 0);

    // BUZZER
    m_buttons[1]->setFixedSize(BTN_W, BTN_H);
    m_gridLayout->addWidget(m_buttons[1], 0, 1);

    // TEMP 信息框
    m_gridLayout->addWidget(tempWidget, 1, 0);

    // EXIT
    m_buttons[2]->setFixedSize(BTN_W, BTN_H);
    m_gridLayout->addWidget(m_buttons[2], 1, 1);
}


void GpioWidget::onExitButtonClicked()
{
    qApp->quit();
}

void GpioWidget::tempupdate(double temp, double humi)
{
    tempWidget->setTemperature(temp);
    tempWidget->setHumidity(humi);
}
