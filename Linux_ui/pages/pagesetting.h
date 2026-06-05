#pragma once
#include <QWidget>
#include "ui/switchbuttonwidget.h"
#include "data/data_protocol.h"
#include "ipc/ipc_client.h"  // 你现有的 IPC 客户端类
#include "ui/widget.h"
class PageSetting : public QWidget
{
    Q_OBJECT
public:
    explicit PageSetting(QWidget *parent = nullptr);

public slots:
    void addSetting(const DataPack &pack); // 板子消息过来更新 UI

private slots:
    void onLedChanged(bool state);
    void onFanChanged(bool state);
    void onBuzzerChanged(bool state);

private:
    void sendRelayStates();

    SwitchButtonWidget *ledSwitch;
    SwitchButtonWidget *fanSwitch;
    SwitchButtonWidget *buzzerSwitch;

    quint16 relayStates; // 当前三路状态
};
