#include "widget.h"
#include <algorithm>
#include <QDebug>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>

#include "pages/addslavedialog.h"

IpcClient *Widget::_Myclient = nullptr;   // ✅ static 定义只能在 cpp

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("MainWidget");
    setFixedSize(480, 272);
    initUI();

}

Widget::~Widget()
{
}

void Widget::initUI()
{
    _Myclient = new IpcClient(this);
    _Myclient->connectToServer("/tmp/device_ipc.sock");

    TopStatusBar *top = new TopStatusBar(this);
    BottomNavBar *bottom = new BottomNavBar(this);

    connect(bottom, &BottomNavBar::sigPageChanged,
            this, &Widget::slotChangePage);

    QStackedWidget *stack = new QStackedWidget(this);

    PageStatus  *pageStatus  = new PageStatus(this);
    PageSetting *pageSetting = new PageSetting(this);
    PageTrend   *pageTrend   = new PageTrend(this);
    Pageinfo    *pageInfo    = new Pageinfo(this);
    m_pageStatus = pageStatus;
    pageInfo->setIpcConnected(_Myclient->isConnected());

    stack->addWidget(pageStatus);
    stack->addWidget(pageTrend);
    stack->addWidget(pageSetting);
    stack->addWidget(pageInfo);

    connect(_Myclient,&IpcClient::devicetrend,pageTrend,&PageTrend::addData);
    connect(_Myclient,&IpcClient::deviceinfo,pageInfo,&Pageinfo::addInfo);
    connect(_Myclient, &IpcClient::connected, this, [pageInfo]() {
        pageInfo->setIpcConnected(true);
    });
    connect(_Myclient, &IpcClient::disconnected, this, [pageInfo]() {
        pageInfo->setIpcConnected(false);
    });
    connect(pageInfo, &Pageinfo::reconnectIpcRequested, this, [this, pageInfo]() {
        if (m_operationOverlay)
            m_operationOverlay->showLoading("Checking backend...");

        QTimer::singleShot(50, this, [this, pageInfo]() {
            const bool ok = _Myclient && _Myclient->connectToServer("/tmp/device_ipc.sock");
            pageInfo->setIpcConnected(ok);

            if (!m_operationOverlay)
                return;

            if (ok)
                m_operationOverlay->showSuccess("Backend connected");
            else
                m_operationOverlay->showFailure("Backend not running");
        });
    });
    connect(_Myclient, &IpcClient::deviceStatusUpdated,
            this, &Widget::handleDeviceStatus);
    connect(pageStatus, &PageStatus::masterChanged, this, [this](int masterSlot) {
        refreshHomeMasterAndSlaveList(masterSlot);
    });
    //connect(_Myclient,&IpcClient::devicesetting,pageSetting,&PageSetting::addSetting);重复连接
    connect(pageSetting, &PageSetting::scanPortsRequested, this, [this]() {
        if (m_operationOverlay)
            m_operationOverlay->showLoading("Scanning ports...");

        if (!sendCommand("scan_ports") && m_operationOverlay)
            m_operationOverlay->showFailure("Scan command failed");
    });

    connect(pageSetting,
            &PageSetting::connectMasterRequested,
            this,
            [this](int masterSlot,
                   const QString &deviceNode,
                   const QString &areaName,
                   int baudRate) {
                Q_UNUSED(areaName);

                if (m_operationOverlay)
                    m_operationOverlay->showLoading("Connecting port...");

                QJsonObject payload;
                payload.insert("slot", masterSlot);
                payload.insert("port", deviceNode);
                payload.insert("baud", baudRate);

                if (!sendCommand("connect_port", payload) && m_operationOverlay)
                    m_operationOverlay->showFailure("Connect command failed");
            });

    connect(pageSetting,
            &PageSetting::disconnectMasterRequested,
            this,
            [this](int masterSlot, const QString &deviceNode) {
                Q_UNUSED(deviceNode);

                if (m_operationOverlay)
                    m_operationOverlay->showLoading("Disconnecting port...");

                QJsonObject payload;
                payload.insert("slot", masterSlot);

                if (!sendCommand("disconnect_port", payload) && m_operationOverlay)
                    m_operationOverlay->showFailure("Disconnect command failed");
            });

    connect(_Myclient, &IpcClient::portsUpdated, this, [this, pageSetting](const QStringList &ports) {
        qDebug() << "[IPC][portsUpdated] count:" << ports.size() << "ports:" << ports;

        QList<MasterPortInfo> portInfos;
        for (int i = 0; i < ports.size(); ++i) {
            MasterPortInfo info;
            info.masterSlot = i;
            info.masterName = QString("Master %1").arg(i + 1);
            info.deviceNode = ports.at(i);
            info.areaName = QString();
            info.baudRate = 9600;
            info.connected = false;
            portInfos.append(info);
        }

        pageSetting->setPortList(portInfos);
        refreshHomeMasterAndSlaveList();

        if (m_operationOverlay)
            m_operationOverlay->showSuccess("Scan complete");
    });

    connect(_Myclient,
            &IpcClient::portStatusUpdated,
            this,
            [this, pageSetting](int slot,
                                const QString &port,
                                const QString &deviceType,
                                int baud,
                                bool connected,
                                const QString &message) {
                qDebug() << "[IPC][portStatusUpdated]"
                         << "slot:" << slot
                         << "port:" << port
                         << "deviceType:" << deviceType
                         << "baud:" << baud
                         << "connected:" << connected
                         << "message:" << message;

                if (connected) {
                    m_connectedMasterSlots.insert(slot);
                    refreshHomeMasterAndSlaveList(slot);
                } else {
                    m_connectedMasterSlots.remove(slot);
                    clearMasterRuntimeState(slot);
                    refreshHomeMasterAndSlaveList();
                }

                pageSetting->updateMasterConnectionState(slot, connected);
            });

    connect(pageStatus, &PageStatus::addSlaveRequested, this, [this](int masterSlot) {
        if (masterSlot < 0) {
            if (m_operationOverlay)
                m_operationOverlay->showFailure("No master selected");
            return;
        }

        if (!m_connectedMasterSlots.contains(masterSlot)) {
            if (m_operationOverlay)
                m_operationOverlay->showFailure("Master not connected");
            return;
        }

        if (m_addSlaveDialog) {
            m_addSlaveDialog->raise();
            m_addSlaveDialog->activateWindow();
            return;
        }

        AddSlaveDialog *dialog = new AddSlaveDialog(masterSlot, this);
        m_addSlaveDialog = dialog;
        connect(dialog, &QObject::destroyed, this, [this]() {
            m_addSlaveDialog = nullptr;
        });
        connect(dialog,
                &AddSlaveDialog::addSlaveRequested,
                this,
                [this](int slot,
                       int slaveId,
                       const QString &deviceType,
                       int pollIntervalMs) {
                    QJsonObject payload;
                    payload.insert("slot", slot);
                    payload.insert("slave_id", slaveId);
                    payload.insert("device_type", deviceType);
                    payload.insert("poll_interval_ms", pollIntervalMs);

                    quint32 seq = 0;
                    if (m_addSlaveDialog)
                        m_addSlaveDialog->setAdding();

                    if (!sendCommand("add_device", payload, &seq)) {
                        if (m_addSlaveDialog)
                            m_addSlaveDialog->setResult(false, "Send command failed");
                        return;
                    }

                    m_pendingAddSlave.active = true;
                    m_pendingAddSlave.seq = seq;
                    m_pendingAddSlave.masterSlot = slot;
                    m_pendingAddSlave.slaveId = slaveId;
                    m_pendingAddSlave.deviceType = deviceType;
        });
        dialog->show();
    });

    connect(pageStatus,
            &PageStatus::relayCommandRequested,
            this,
            [this](int masterSlot,
                   int slaveAddr,
                   const QString &channel,
                   bool on) {
                int bit = -1;
                if (channel == "led")
                    bit = 0;
                else if (channel == "fan")
                    bit = 1;
                else if (channel == "buzzer")
                    bit = 2;

                if (bit < 0) {
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("Unknown relay channel");
                    return;
                }

                const QString key = relayStateKey(masterSlot, slaveAddr);
                int states = m_relayStates.value(key, 0);
                if (on)
                    states |= (1 << bit);
                else
                    states &= ~(1 << bit);

                QJsonObject payload;
                payload.insert("slot", masterSlot);
                payload.insert("slave_id", slaveAddr);
                payload.insert("states", states);

                if (m_operationOverlay)
                    m_operationOverlay->showLoading("Sending relay command...");

                if (sendCommand("set_relay", payload)) {
                    m_relayStates.insert(key, states);
                    if (m_pageStatus) {
                        m_pageStatus->setRelayStates(masterSlot,
                                                     slaveAddr,
                                                     (states & 0x01) != 0,
                                                     (states & 0x02) != 0,
                                                     (states & 0x04) != 0,
                                                     QDateTime::currentDateTime().toString("HH:mm:ss"));
                    }
                } else if (m_operationOverlay) {
                    m_operationOverlay->showFailure("Relay command failed");
                }
            });

    connect(_Myclient,
            &IpcClient::commandAckReceived,
            this,
            [this](quint32 seq,
                   const QString &cmd,
                   const QString &status,
                   const QString &reason,
                   const QString &message) {
                qDebug() << "[IPC][ack]"
                         << "seq:" << seq
                         << "cmd:" << cmd
                         << "status:" << status
                         << "reason:" << reason
                         << "message:" << message;

                if (cmd == "add_device" &&
                    m_pendingAddSlave.active &&
                    m_pendingAddSlave.seq == seq) {
                    const bool ok = (status == "ok");
                    const QString text = !message.isEmpty()
                        ? message
                        : (!reason.isEmpty() ? reason : (ok ? "Add success" : "Add failed"));

                    if (m_addSlaveDialog)
                        m_addSlaveDialog->setResult(ok, ok ? "Add success" : text);

                    if (ok) {
                        SlaveDeviceInfo info;
                        info.masterSlot = m_pendingAddSlave.masterSlot;
                        info.slaveAddr = m_pendingAddSlave.slaveId;
                        info.deviceName = QString("Slave %1").arg(m_pendingAddSlave.slaveId);
                        info.displayName = m_pendingAddSlave.deviceType == "sensor_th"
                            ? "Temp/Humi"
                            : "Relay";
                        info.deviceType = m_pendingAddSlave.deviceType;
                        info.online = false;
                        m_slaveDevices.append(info);

                        refreshHomeMasterAndSlaveList(m_pendingAddSlave.masterSlot);
                    }

                    m_pendingAddSlave = PendingAddSlave();
                    return;
                }

                if (!m_operationOverlay)
                    return;

                if (status != "ok") {
                    const QString text = !message.isEmpty() ? message : reason;
                    m_operationOverlay->showFailure(text.isEmpty() ? "Command failed" : text);
                    return;
                }

                if (cmd == "connect_port")
                    m_operationOverlay->showSuccess("Connect complete");
                else if (cmd == "disconnect_port")
                    m_operationOverlay->showSuccess("Disconnect complete");
                else if (cmd == "add_device")
                    m_operationOverlay->showSuccess("Add slave complete");
                else if (cmd == "set_relay")
                    m_operationOverlay->showSuccess("Relay command complete");
            });

    connect(_Myclient, &IpcClient::errorOccured, this, [this](const QString &err) {
        qDebug() << "[IPC][error]" << err;
    });

    stack->setCurrentIndex(0);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(top);
    layout->addWidget(stack, 1);
    layout->addWidget(bottom);

    m_stack = stack;

    m_operationOverlay = new OperationOverlayWidget(this);
    m_operationOverlay->setGeometry(rect());
    m_operationOverlay->raise();
}

void Widget::slotChangePage(int index)
{
    m_stack->setCurrentIndex(index);
}

void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    if (m_operationOverlay)
        m_operationOverlay->setGeometry(rect());
}

void Widget::handleDeviceStatus(const DataPack &pack)
{
    const QString updateTime = pack.time.isValid()
        ? pack.time.toString("HH:mm:ss")
        : QDateTime::currentDateTime().toString("HH:mm:ss");

    for (const DeviceData &device : pack.devices) {
        if (!device.valid)
            continue;

        int masterSlot = -1;
        for (const SlaveDeviceInfo &slave : m_slaveDevices) {
            if (slave.slaveAddr == device.deviceId) {
                masterSlot = slave.masterSlot;
                break;
            }
        }
        if (masterSlot < 0 && m_connectedMasterSlots.size() == 1)
            masterSlot = *m_connectedMasterSlots.constBegin();
        if (masterSlot < 0)
            masterSlot = 0;
        if (!m_connectedMasterSlots.contains(masterSlot))
            continue;

        if (device.type == DEV_SENSOR_TH) {
            updateSlaveOnline(masterSlot, device.deviceId, "sensor_th", true);
            if (m_pageStatus) {
                m_pageStatus->setSensorThData(masterSlot,
                                              device.deviceId,
                                              device.temperature,
                                              device.humidity,
                                              updateTime);
            }
        } else if (device.type == DEV_RELAY) {
            updateSlaveOnline(masterSlot, device.deviceId, "relay", true);
            m_relayStates.insert(relayStateKey(masterSlot, device.deviceId),
                                 device.relayStates);
            if (m_pageStatus) {
                m_pageStatus->setRelayStates(masterSlot,
                                             device.deviceId,
                                             (device.relayStates & 0x01) != 0,
                                             (device.relayStates & 0x02) != 0,
                                             (device.relayStates & 0x04) != 0,
                                             updateTime);
            }
        }
    }

    refreshStatusSummary();
}

void Widget::updateSlaveOnline(int masterSlot,
                               int slaveAddr,
                               const QString &deviceType,
                               bool online)
{
    for (SlaveDeviceInfo &slave : m_slaveDevices) {
        if (slave.masterSlot != masterSlot ||
            slave.slaveAddr != slaveAddr ||
            slave.deviceType != deviceType) {
            continue;
        }

        slave.online = online;
        if (m_pageStatus)
            m_pageStatus->updateSlaveOnline(masterSlot, slaveAddr, deviceType, online);
        return;
    }
}

void Widget::refreshHomeMasterAndSlaveList(int preferredMasterSlot)
{
    if (!m_pageStatus)
        return;

    QList<int> connectedSlots = m_connectedMasterSlots.values();
    std::sort(connectedSlots.begin(), connectedSlots.end());

    QList<MasterStatusInfo> masters;
    for (int slot : connectedSlots) {
        MasterStatusInfo info;
        info.masterSlot = slot;
        info.masterName = masterNameForSlot(slot);
        masters.append(info);
    }

    int targetSlot = preferredMasterSlot >= 0
        ? preferredMasterSlot
        : m_pageStatus->currentMasterSlotValue();
    if (targetSlot >= 0 && !m_connectedMasterSlots.contains(targetSlot))
        targetSlot = -1;

    m_pageStatus->setMasterList(masters, targetSlot);

    const int currentSlot = m_pageStatus->currentMasterSlotValue();
    QList<SlaveDeviceInfo> visibleSlaves;
    if (currentSlot >= 0) {
        for (const SlaveDeviceInfo &slave : m_slaveDevices) {
            if (slave.masterSlot == currentSlot)
                visibleSlaves.append(slave);
        }
    }

    m_pageStatus->setSlaveList(visibleSlaves);
    refreshStatusSummary();
}

void Widget::clearMasterRuntimeState(int masterSlot)
{
    for (int i = m_slaveDevices.size() - 1; i >= 0; --i) {
        if (m_slaveDevices.at(i).masterSlot == masterSlot)
            m_slaveDevices.removeAt(i);
    }

    const QString keyPrefix = QString("%1:").arg(masterSlot);
    const QList<QString> relayKeys = m_relayStates.keys();
    for (const QString &key : relayKeys) {
        if (key.startsWith(keyPrefix))
            m_relayStates.remove(key);
    }

    if (m_pendingAddSlave.active && m_pendingAddSlave.masterSlot == masterSlot)
        m_pendingAddSlave = PendingAddSlave();
}

void Widget::refreshStatusSummary()
{
    int onlineSlaveCount = 0;
    for (const SlaveDeviceInfo &slave : m_slaveDevices) {
        if (slave.online && m_connectedMasterSlots.contains(slave.masterSlot))
            ++onlineSlaveCount;
    }

    if (m_pageStatus) {
        m_pageStatus->setMasterSummary(m_connectedMasterSlots.size(),
                                       onlineSlaveCount,
                                       0,
                                       "--");
    }
}

QString Widget::masterNameForSlot(int masterSlot) const
{
    return QString("Master %1").arg(masterSlot + 1);
}

QString Widget::relayStateKey(int masterSlot, int slaveAddr) const
{
    return QString("%1:%2").arg(masterSlot).arg(slaveAddr);
}

bool Widget::sendCommand(const QString &cmd, QJsonObject payload, quint32 *seqOut)
{
    const quint32 seq = m_nextCommandSeq++;
    if (seqOut)
        *seqOut = seq;

    payload.insert("type", "command");
    payload.insert("cmd", cmd);
    payload.insert("seq", static_cast<int>(seq));

    const QByteArray msg = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    qDebug().noquote() << "[IPC][sendCommand]"
                       << "seq:" << seq
                       << "cmd:" << cmd
                       << "json:" << QString::fromUtf8(msg);

    const bool ok = _Myclient && _Myclient->sendMessage(msg);
    qDebug() << "[IPC][sendCommandResult]"
             << "seq:" << seq
             << "cmd:" << cmd
             << "ok:" << ok;

    return ok;
}
