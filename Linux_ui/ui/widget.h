#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QHash>
#include <QTimer>
#include "ui/TopStatusBar.h"
#include "ui/BottomNavBar.h"
#include "ui/operationoverlaywidget.h"
#include "pages/pagesetting.h"
#include "pages/pagetrend.h"
#include "pages/PageStatus.h"
#include "pages/pageinfo.h"
#include "ipc/ipc_client.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class AddSlaveDialog;

class Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

public slots:
    void slotChangePage(int index);

protected:
    void resizeEvent(QResizeEvent *event) override;

public:
    static IpcClient * _Myclient;

private:
    void initUI();
    void handleIpcConnected(TopStatusBar *topBar, Pageinfo *pageInfo);
    void handleIpcDisconnected(TopStatusBar *topBar,
                               Pageinfo *pageInfo,
                               PageSetting *pageSetting);
    void handleDeviceStatus(const DataPack &pack);
    void startIpcAutoReconnect();
    void stopIpcAutoReconnect();
    void requestRuntimeRefresh();
    void requestOfflineCacheConfig();
    void handleRuntimeStateSnapshot(const QJsonArray &ports);
    void handleOfflineCacheAck(const QString &cmd,
                               const QString &status,
                               const QString &reason,
                               const QJsonObject &ackRoot);
    int slotFromPortId(const QString &portId) const;
    void handleRemoteRemoveDeviceAck(const QJsonObject &ack);
    void upsertRegisteredSlave(int masterSlot,
                               int slaveAddr,
                               const QString &deviceName,
                               const QString &deviceType);
    void removeRegisteredSlave(int masterSlot,
                               int slaveAddr,
                               const QString &deviceType);
    void updateSlaveOnline(int masterSlot,
                           int slaveAddr,
                           const QString &deviceType,
                           bool online);
    void refreshHomeMasterAndSlaveList(int preferredMasterSlot = -1);
    void markMasterRuntimeOffline(int masterSlot);
    void refreshStatusSummary();
    QString masterNameForSlot(int masterSlot) const;
    QString relayStateKey(int masterSlot, int slaveAddr) const;
    bool sendCommand(const QString &cmd,
                     QJsonObject payload = QJsonObject(),
                     quint32 *seqOut = nullptr);

    struct PendingAddSlave {
        bool active = false;
        quint32 seq = 0;
        int masterSlot = 0;
        int slaveId = 0;
        QString deviceType;
    };

    struct PendingRemoveSlave {
        bool active = false;
        quint32 seq = 0;
        int masterSlot = 0;
        int slaveAddr = 0;
        QString deviceType;
    };

    QStackedWidget *m_stack = nullptr;
    PageStatus *m_pageStatus = nullptr;
    PageTrend *m_pageTrend = nullptr;
    OperationOverlayWidget *m_operationOverlay = nullptr;
    QPointer<AddSlaveDialog> m_addSlaveDialog;
    QSet<int> m_connectedMasterSlots;
    QList<SlaveDeviceInfo> m_slaveDevices;
    QHash<QString, int> m_relayStates;
    QHash<int, MasterPortInfo> m_runtimePorts;
    QTimer *m_ipcReconnectTimer = nullptr;
    PendingAddSlave m_pendingAddSlave;
    PendingRemoveSlave m_pendingRemoveSlave;
    int m_activeAlarmCount = 0;
    quint32 m_nextCommandSeq = 1;
    Ui::Widget *ui = nullptr;
};

#endif // WIDGET_H
