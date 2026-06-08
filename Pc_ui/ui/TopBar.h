#pragma once
#include <QWidget>

class QLabel;
class QTimer;

class TopBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopBar(QWidget *parent = nullptr);

public slots:
    void setMqttState(bool connected);
    void setOnlineGatewayCount(int count);
    void setOnlineDeviceCount(int count);
    void setAlarmCount(int count);
    void updateCurrentTime();

private:
    QLabel *m_mqttLabel = nullptr;
    QLabel *m_gatewayLabel = nullptr;
    QLabel *m_deviceLabel = nullptr;
    QLabel *m_alarmLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QTimer *m_timer = nullptr;
};
