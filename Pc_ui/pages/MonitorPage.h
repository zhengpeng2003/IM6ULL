#pragma once
#include <QWidget>
class DataManager;
class CommandManager;
class UiStateStore;
class DeviceTreeWidget;
class QLabel;
class QPushButton;
class QShowEvent;
class QTimer;

class MonitorPage : public QWidget
{
    Q_OBJECT
public:
    explicit MonitorPage(DataManager *data, CommandManager *command,
                         UiStateStore *stateStore, QWidget *parent = nullptr);

private slots:
    void scheduleRefreshDeviceTree();
    void scheduleRefreshDetail();
    void refreshDeviceTree();
    void onDeviceSelected(const QString &deviceKey);
    void refreshDetail();
    void sendFanCommand(bool on);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void setDetailText(const QString &text);

private:
    DataManager *m_data = nullptr;
    CommandManager *m_command = nullptr;
    UiStateStore *m_stateStore = nullptr;
    DeviceTreeWidget *m_tree = nullptr;
    QLabel *m_detail = nullptr;
    QPushButton *m_fanOn = nullptr;
    QPushButton *m_fanOff = nullptr;
    QString m_currentKey;
    QTimer *m_treeRefreshTimer = nullptr;
    QTimer *m_detailRefreshTimer = nullptr;
    bool m_treeRefreshDirty = false;
    bool m_detailRefreshDirty = false;
};
