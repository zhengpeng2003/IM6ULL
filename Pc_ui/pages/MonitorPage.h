#pragma once
#include <QVariant>
#include <QWidget>
class DataManager;
class CommandManager;
class UiStateStore;
class DeviceTreeWidget;
class DeviceDetailCardBaseUi;
class RelayDetailCardUi;
class SensorThDetailCardUi;
class QShowEvent;
class QStackedWidget;
class QScrollArea;
class QTimer;
struct DeviceNode;

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
    void refreshRelayPendingState();
    void onRelayCommandRequested(const DeviceNode &node, const QString &channel,
                                 bool on, const QVariantMap &channels);

protected:
    void showEvent(QShowEvent *event) override;

private:
    DataManager *m_data = nullptr;
    CommandManager *m_command = nullptr;
    UiStateStore *m_stateStore = nullptr;
    DeviceTreeWidget *m_tree = nullptr;
    QScrollArea *m_detailScrollArea = nullptr;
    QStackedWidget *m_detailStack = nullptr;
    DeviceDetailCardBaseUi *m_emptyCard = nullptr;
    DeviceDetailCardBaseUi *m_baseCard = nullptr;
    SensorThDetailCardUi *m_sensorThCard = nullptr;
    RelayDetailCardUi *m_relayCard = nullptr;
    QString m_currentKey;
    QTimer *m_treeRefreshTimer = nullptr;
    QTimer *m_detailRefreshTimer = nullptr;
    bool m_treeRefreshDirty = false;
    bool m_detailRefreshDirty = false;
};
