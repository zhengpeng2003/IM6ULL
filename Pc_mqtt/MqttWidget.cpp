#include "MqttWidget.h"
#include <QtCore/QDateTime>
#include <QtMqtt/QMqttClient>
#include <QtWidgets/QMessageBox>
#include <qtimer.h>
#include "ui_MqttWidget.h"

// 构造函数：初始化MQTT客户端界面
MqttWidget::MqttWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MqttWidget)  // 初始化界面
{
    ui->setupUi(this);

    // 创建MQTT客户端实例
    m_client = new QMqttClient(this);
    // 从界面读取并设置MQTT服务器主机名
    m_client->setHostname(ui->lineEditHost->text());
    // 从界面读取并设置MQTT服务器端口号
    m_client->setPort(ui->spinBoxPort->value());

    // 连接MQTT客户端状态变化信号到更新日志槽函数
    connect(m_client, &QMqttClient::stateChanged, this, &MqttWidget::updateLogStateChange);
    // 连接MQTT客户端断开连接信号到处理槽函数
    connect(m_client, &QMqttClient::disconnected, this, &MqttWidget::brokerDisconnected);
    // 连接MQTT客户端消息接收信号到处理槽函数
    connect(m_client, &QMqttClient::messageReceived, this,
            [this](const QByteArray &payload, const QMqttTopicName &topic) {
                // 使用lambda表达式转发消息给handleMessage函数
                this->handleMessage(payload, topic.name());
            });

    // 连接MQTT客户端心跳响应信号
    connect(m_client, &QMqttClient::pingResponseReceived, this, [this]() {
        const QString content = QDateTime::currentDateTime().toString()
        + QLatin1String(" PingResponse")
            + QLatin1Char('\n');
        // 可以在此处添加日志记录代码
    });

    // 连接界面主机名输入框文本变化信号到MQTT客户端设置主机名函数
    connect(ui->lineEditHost, &QLineEdit::textChanged, m_client, &QMqttClient::setHostname);
    // 连接界面端口号输入框值变化信号到设置客户端端口函数
    connect(ui->spinBoxPort, QOverload<int>::of(&QSpinBox::valueChanged), this, &MqttWidget::setClientPort);

    // 初始更新连接状态显示
    updateLogStateChange();
    if (!m_sql.init()) {
        qCritical() << "Database init failed!";
    }
}

// 析构函数：清理资源
MqttWidget::~MqttWidget()
{
    delete ui;
}

// 连接/断开连接按钮点击处理函数
void MqttWidget::on_buttonConnect_clicked()
{
    // 如果当前是断开状态，则尝试连接
    if (m_client->state() == QMqttClient::Disconnected) {
        // 连接时禁用主机名和端口输入框
        ui->lineEditHost->setEnabled(false);
        ui->spinBoxPort->setEnabled(false);
        ui->buttonConnect->setText(tr("Disconnect"));  // 按钮文本改为"断开连接"
        m_client->connectToHost();  // 连接到MQTT服务器

    } else {
        // 如果已连接，则断开连接
        ui->lineEditHost->setEnabled(true);
        ui->spinBoxPort->setEnabled(true);
        ui->buttonConnect->setText(tr("Connect"));  // 按钮文本改为"连接"
        m_client->disconnectFromHost();  // 断开MQTT连接
    }
}

// 退出按钮点击处理函数
void MqttWidget::on_buttonQuit_clicked()
{
    QApplication::quit();  // 退出应用程序
}

// 更新MQTT连接状态日志
void MqttWidget::updateLogStateChange()
{
    const QString content = QDateTime::currentDateTime().toString()
    + QLatin1String(": State Change")
        + QString::number(m_client->state())  // 获取当前连接状态
        + QLatin1Char('\n');
    // 可以在此处添加日志显示代码，如：ui->textEditLog->append(content);
}

// MQTT代理断开连接处理函数
void MqttWidget::brokerDisconnected()
{
    // 断开连接后启用主机名和端口输入框
    ui->lineEditHost->setEnabled(true);
    ui->spinBoxPort->setEnabled(true);
    // 将按钮文本改回"连接"
    ui->buttonConnect->setText(tr("Connect"));
}

// 设置MQTT客户端端口号
void MqttWidget::setClientPort(int p)
{
    m_client->setPort(p);  // 设置MQTT客户端端口
}

// 发布消息按钮点击处理函数
void MqttWidget::on_buttonPublish_clicked()
{
    // 获取界面输入的主题和消息内容
    QString topic = ui->lineEditTopic->text();
    QByteArray message = ui->lineEditMessage->text().toUtf8();

    // 发布消息，如果返回-1表示发布失败
    if (m_client->publish(topic, message) == -1)
        QMessageBox::critical(this, QLatin1String("Error"), QLatin1String("Could not publish message"));
}

// 订阅主题按钮点击处理函数
void MqttWidget::on_buttonSubscribe_clicked()
{
    // 检查 MQTT 连接状态
    if (m_client->state() != QMqttClient::Connected) {
        QMessageBox::critical(this, QLatin1String("Error"),
                              QLatin1String("Not connected to broker!"));
        return;
    }

    bool allSuccess = true;
    QStringList failedTopics;

    // 订阅用户输入的主题（如果有）
    QString userTopic = ui->lineEditTopic->text().trimmed();
    if (!userTopic.isEmpty()) {
        auto sub = m_client->subscribe(userTopic);
        if (!sub) {
            allSuccess = false;
            failedTopics << userTopic;
        }
    }

    // 订阅 LED ACK
    auto ledSub = m_client->subscribe(QStringLiteral("imx6ull/gpio/led/ack"));
    if (!ledSub) {
        allSuccess = false;
        failedTopics << "imx6ull/gpio/led/ack";
    }

    // 订阅蜂鸣器 ACK
    auto buzzerSub = m_client->subscribe(QStringLiteral("imx6ull/gpio/buzzer/ack"));
    if (!buzzerSub) {
        allSuccess = false;
        failedTopics << "imx6ull/gpio/buzzer/ack";
    }

    // 检查订阅结果
    if (!allSuccess) {
        QMessageBox::critical(this, QLatin1String("Error"),
                              QString("Could not subscribe to: %1")
                                  .arg(failedTopics.join(", ")));
    } else {
        QMessageBox::information(this, QLatin1String("Success"),
                                 QLatin1String("All topics subscribed!"));
    }
}

// LED按钮点击处理函数
void MqttWidget::onLedButtonClicked(bool targetState)
{
    // 发布LED控制命令并等待ACK响应
    // cmdTopic: 命令主题，ackTopic: 响应主题
    publishWithAck("imx6ull/gpio/led/set", "imx6ull/gpio/led/ack", targetState);
}

// 蜂鸣器按钮点击处理函数
void MqttWidget::onBuzzerButtonClicked(bool targetState)
{
    // 发布蜂鸣器控制命令并等待ACK响应
    publishWithAck("imx6ull/gpio/buzzer/set", "imx6ull/gpio/buzzer/ack", targetState);
}

// 带ACK确认的消息发布函数
void MqttWidget::publishWithAck(const QString &cmdTopic, const QString &ackTopic, bool value)
{
    if (!m_client || m_client->state() != QMqttClient::Connected) {
        QMessageBox::critical(this, "MQTT Error", "Not connected!");
        return;
    }

    // 检查是否已有等待中的操作
    if (ackTopic.contains("led") && m_ledPending.waiting) {
        qDebug() << "[MQTT] LED operation already pending, ignoring new request";
        return;
    }
    if (ackTopic.contains("buzzer") && m_buzzerPending.waiting) {
        qDebug() << "[MQTT] Buzzer operation already pending, ignoring new request";
        return;
    }

    // 设置等待状态
    if (ackTopic.contains("led")) {
        m_ledPending = {true, value, ackTopic};
    } else if (ackTopic.contains("buzzer")) {
        m_buzzerPending = {true, value, ackTopic};
    }

    QByteArray payload = value ? "1" : "0";
    auto res = m_client->publish(cmdTopic, payload);

    if (res == -1) {
        // 发布失败，清除等待状态
        if (ackTopic.contains("led")) {
            m_ledPending.waiting = false;
            emit ledAckReceived(false, value);  // 修复：传入期望的 value，不是 false
        } else if (ackTopic.contains("buzzer")) {  // 修复：加上 else if
            m_buzzerPending.waiting = false;
            emit buzzerAckReceived(false, value);  // 修复：传入期望的 value
        }
        QMessageBox::critical(this, "MQTT Error", "Publish failed!");
        return;
    }

    qDebug() << "[MQTT] Sent" << cmdTopic << "=" << value
             << "waiting ACK on" << ackTopic;
}
// 处理接收到的MQTT消息
void MqttWidget::handleMessage(const QByteArray &payload, const QString &topic)
{
    //数据库信息插入
    m_sql.insertData(topic, payload);
    // 1. 先判断是否是ACK响应消息
    if (topic.contains("/ack")) {
        parseAckMessage(payload, topic);  // 解析ACK消息
        return;
    }

    // 2. 判断是否是传感器数据消息
    if (topic.contains("device/data")) {
        parseSensorData(payload);  // 解析传感器数据
    }
}

// 解析传感器数据（温湿度数据）
void MqttWidget::parseSensorData(const QByteArray &payload)
{
    QJsonParseError err;
    // 将接收到的JSON数据解析为文档
    QJsonDocument doc = QJsonDocument::fromJson(payload, &err);

    // 检查JSON解析是否出错
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return;
    }

    // 检查文档是否为JSON对象
    if (!doc.isObject())
        return;

    // 获取根JSON对象
    QJsonObject root = doc.object();

    // 检查是否包含devices数组
    if (!root.contains("devices") || !root.value("devices").isArray())
        return;

    // 获取devices数组
    QJsonArray devices = root.value("devices").toArray();
    if (devices.isEmpty())
        return;

    // 获取第一个设备对象
    QJsonObject dev = devices.first().toObject();

    // 检查设备类型是否为温湿度传感器
    if (dev.value("type").toString() != "sensor_th")
        return;

    // 检查传感器数据是否有效
    if (dev.value("valid").toInt() != 1)
        return;

    // 提取温度和湿度数据
    double temp = dev.value("temp").toDouble();
    double humi = dev.value("humi").toDouble();

    // 调试输出：显示提取的温湿度数据
    qDebug() << "[MQTT] Sensor data - TEMP:" << temp << "HUMI:" << humi;

    // 发出信号，通知其他组件（如GPIO页面）更新温湿度显示
    emit S_thDataUpdated(temp, humi);
}

// 解析ACK响应消息
void MqttWidget::parseAckMessage(const QByteArray &payload, const QString &topic)
{
    QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (!obj.contains("result") || !obj.contains("value")) return;

    int result = obj["result"].toInt();
    int valueInt = obj["value"].toInt();
    bool success = (result == 0);
    bool valueBool = (valueInt != 0);

    // LED ACK 处理
    if (topic == m_ledPending.ackTopic && m_ledPending.waiting) {
        if (valueBool == m_ledPending.expectedValue) {
            m_ledPending.waiting = false;
            emit ledAckReceived(success, valueBool);
            qDebug() << "[MQTT] LED ACK success:" << success
                     << "value:" << valueInt;
        } else {
            qDebug() << "[MQTT] LED ACK value mismatch! expected:"
                     << m_ledPending.expectedValue << "got:" << valueBool
                     << "(int:" << valueInt << ")";
            // 修复：即使不匹配也要发射信号，避免一直 waiting
            m_ledPending.waiting = false;
            emit ledAckReceived(false, valueBool);
        }
    }
    // 蜂鸣器类似 - 关键修复：else if！
    else if (topic == m_buzzerPending.ackTopic && m_buzzerPending.waiting) {
        if (valueBool == m_buzzerPending.expectedValue) {
            m_buzzerPending.waiting = false;
            emit buzzerAckReceived(success, valueBool);
            qDebug() << "[MQTT] Buzzer ACK success:" << success  // 修复拼写 Buuzer→Buzzer
                     << "value:" << valueInt;
        } else {
            qDebug() << "[MQTT] Buzzer ACK value mismatch! expected:"  // 修复拼写
                     << m_buzzerPending.expectedValue << "got:" << valueBool
                     << "(int:" << valueInt << ")";
            // 修复：即使不匹配也要发射信号
            m_buzzerPending.waiting = false;
            emit buzzerAckReceived(false, valueBool);
        }
    }
}
