#ifndef MQTTWIDGET_H
#define MQTTWIDGET_H

#include <QMainWindow>
//#include <QMqttClient>
#include <QtMqtt/QMqttClient>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "datasql.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MqttWidget;
}
QT_END_NAMESPACE

// MQTT客户端界面类，用于管理和显示MQTT连接、消息发布与订阅
class MqttWidget : public QWidget
{
    Q_OBJECT  // Qt元对象系统宏，支持信号槽机制

public:
    // 构造函数，初始化MQTT客户端界面
    explicit MqttWidget(QWidget *parent = nullptr);
    ~MqttWidget();

public: signals:
    // 信号：温湿度数据更新时触发，用于通知其他组件更新显示
    void  S_thDataUpdated(double temp, double humi);

    // 信号：接收到设备响应（ACK）时触发
    // LED设备响应，success表示操作是否成功
    void ledAckReceived(bool success, bool actualState);  // 添加实际状态参数
    void buzzerAckReceived(bool success, bool actualState);

public slots:
    // 设置MQTT客户端端口号
    void setClientPort(int p);
    // LED按钮点击槽函数，targetState为期望的LED状态（true开/false关）
    void onLedButtonClicked(bool targetState);
    // 蜂鸣器按钮点击槽函数，targetState为期望的蜂鸣器状态
    void onBuzzerButtonClicked(bool targetState);

private slots:
    // 连接/断开连接按钮点击槽函数
    void on_buttonConnect_clicked();
    // 退出按钮点击槽函数
    void on_buttonQuit_clicked();
    // 更新MQTT连接状态显示
    void updateLogStateChange();
    // 处理接收到的MQTT消息
    void handleMessage(const QByteArray &payload, const QString &topic);
    // 解析设备响应（ACK）消息
    void parseAckMessage(const QByteArray &payload, const QString &topic);
    // 解析传感器数据（温湿度数据）
    void parseSensorData(const QByteArray &payload);
    // 处理MQTT代理断开连接
    void brokerDisconnected();
    // 发布消息按钮点击槽函数
    void on_buttonPublish_clicked();
    // 订阅主题按钮点击槽函数
    void on_buttonSubscribe_clicked();

    // 带ACK确认的消息发布函数
    // cmdTopic: 命令主题，ackTopic: 响应主题，value: 设置的值
    void publishWithAck(const QString &cmdTopic, const QString &ackTopic, bool value);

private:
    // 等待ACK确认的结构体定义
    struct PendingAck {
        bool waiting = false;        // 是否正在等待ACK
        bool expectedValue = false;  // 期望的设置值
        QString ackTopic;            // 对应的ACK主题
    };

    PendingAck m_ledPending;      // LED操作等待ACK状态
    PendingAck m_buzzerPending;   // 蜂鸣器操作等待ACK状态
    Ui::MqttWidget *ui;           // 界面指针
    QMqttClient *m_client;        // MQTT客户端实例
    datasql m_sql;                //数据库
};

#endif // MQTTWIDGET_H
