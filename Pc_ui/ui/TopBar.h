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
    void setOnlineGatewayCount(int count);
    void setOnlineDeviceCount(int count);
    void setAlarmCount(int count);
    void setServiceOnline(bool online);
    void updateCurrentTime();

private:
    QLabel *m_gatewayLabel = nullptr;
    QLabel *m_deviceLabel = nullptr;
    QLabel *m_alarmLabel = nullptr;
    QLabel *m_serviceLabel = nullptr;
    QLabel *m_timeLabel = nullptr;
    QTimer *m_timer = nullptr;
    int m_onlineGatewayCount = -1;
    int m_onlineDeviceCount = -1;
    int m_alarmCount = -1;
    bool m_serviceOnline = false;
};
