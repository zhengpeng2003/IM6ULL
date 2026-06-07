#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QJsonObject>
#include <QPointer>
#include <QSet>
#include <QHash>
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
    void handleDeviceStatus(const DataPack &pack);
    void updateSlaveOnline(int masterSlot,
                           int slaveAddr,
                           const QString &deviceType,
                           bool online);
    void refreshHomeMasterAndSlaveList(int preferredMasterSlot = -1);
    void clearMasterRuntimeState(int masterSlot);
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

    QStackedWidget *m_stack = nullptr;
    PageStatus *m_pageStatus = nullptr;
    OperationOverlayWidget *m_operationOverlay = nullptr;
    QPointer<AddSlaveDialog> m_addSlaveDialog;
    QSet<int> m_connectedMasterSlots;
    QList<SlaveDeviceInfo> m_slaveDevices;
    QHash<QString, int> m_relayStates;
    PendingAddSlave m_pendingAddSlave;
    quint32 m_nextCommandSeq = 1;
    Ui::Widget *ui = nullptr;
};

#endif // WIDGET_H
