#pragma once
#include <QWidget>
class DataManager;
class CommandManager;
class DeviceTreeWidget;
class QLabel;
class QPushButton;
class QTimer;

class MonitorPage : public QWidget
{
    Q_OBJECT
public:
    explicit MonitorPage(DataManager *data, CommandManager *command, QWidget *parent = nullptr);

private slots:
    void refreshDeviceTree();
    void onDeviceSelected(const QString &deviceKey);
    void refreshDetail();
    void sendFanCommand(bool on);

private:
    void setDetailText(const QString &text);

private:
    DataManager *m_data = nullptr;
    CommandManager *m_command = nullptr;
    DeviceTreeWidget *m_tree = nullptr;
    QLabel *m_detail = nullptr;
    QPushButton *m_fanOn = nullptr;
    QPushButton *m_fanOff = nullptr;
    QTimer *m_timer = nullptr;
    QString m_currentKey;
};
