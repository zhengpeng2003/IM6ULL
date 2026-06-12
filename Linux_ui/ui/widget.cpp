#include "widget.h"
#include <algorithm>
#include <QDebug>
#include <QJsonDocument>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>

#include "pageui/addslavedialog.h"

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
    m_pageTrend = pageTrend;
    pageInfo->setIpcConnected(_Myclient->isConnected());

    stack->addWidget(pageStatus);
    stack->addWidget(pageTrend);
    stack->addWidget(pageSetting);
    stack->addWidget(pageInfo);

    connect(_Myclient,&IpcClient::devicetrend,pageTrend,&PageTrend::addData);
    connect(_Myclient,&IpcClient::deviceinfo,pageInfo,&Pageinfo::addInfo);
    connect(_Myclient, &IpcClient::connected, this, [this, top, pageInfo]() {
        handleIpcConnected(top, pageInfo);
    });
    connect(_Myclient, &IpcClient::disconnected, this, [this, top, pageInfo, pageSetting]() {
        handleIpcDisconnected(top, pageInfo, pageSetting);
    });
    connect(pageInfo, &Pageinfo::reconnectIpcRequested, this, [this, top, pageInfo]() {
        if (m_operationOverlay)
            m_operationOverlay->showLoading("正在检查后端...");

        QTimer::singleShot(50, this, [this, top, pageInfo]() {
            const bool ok = _Myclient && _Myclient->connectToServer("/tmp/device_ipc.sock");
            if (ok)
                handleIpcConnected(top, pageInfo);
            else {
                pageInfo->setIpcConnected(false);
                top->setBackendConnected(false);
                if (m_pageStatus)
                    m_pageStatus->setAlarmText("告警：IPC 未连接，请重新连接后端");
                refreshStatusSummary();
            }

            if (!m_operationOverlay)
                return;

            if (ok)
                m_operationOverlay->showSuccess("后端已连接");
            else
                m_operationOverlay->showFailure("后端未运行");
        });
    });
    connect(_Myclient, &IpcClient::deviceStatusUpdated,
            this, &Widget::handleDeviceStatus);
    connect(_Myclient,
            &IpcClient::deviceRegistered,
            this,
            [this](quint32 seq,
                   int slot,
                   int deviceId,
                   const QString &deviceName,
                   const QString &deviceType,
                   int pollIntervalMs) {
                Q_UNUSED(seq);
                Q_UNUSED(pollIntervalMs);

                if (slot < 0 || deviceId <= 0 || deviceType.isEmpty())
                    return;

                m_connectedMasterSlots.insert(slot);
                upsertRegisteredSlave(slot, deviceId, deviceName, deviceType);
                refreshHomeMasterAndSlaveList(slot);

                if (m_operationOverlay)
                    m_operationOverlay->showSuccess("从站已添加");
            });
    connect(pageStatus, &PageStatus::masterChanged, this, [this](int masterSlot) {
        refreshHomeMasterAndSlaveList(masterSlot);
    });
    //connect(_Myclient,&IpcClient::devicesetting,pageSetting,&PageSetting::addSetting);重复连接
    connect(pageSetting, &PageSetting::scanPortsRequested, this, [this]() {
        if (m_operationOverlay)
            m_operationOverlay->showLoading("正在扫描端口...");

        if (!sendCommand("scan_ports") && m_operationOverlay)
            m_operationOverlay->showFailure("扫描命令发送失败");
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
                    m_operationOverlay->showLoading("正在连接端口...");

                QJsonObject payload;
                payload.insert("slot", masterSlot);
                payload.insert("port", deviceNode);
                payload.insert("baud", baudRate);

                if (!sendCommand("connect_port", payload) && m_operationOverlay)
                    m_operationOverlay->showFailure("连接命令发送失败");
            });

    connect(pageSetting,
            &PageSetting::disconnectMasterRequested,
            this,
            [this](int masterSlot, const QString &deviceNode) {
                Q_UNUSED(deviceNode);

                if (m_operationOverlay)
                    m_operationOverlay->showLoading("正在断开端口...");

                QJsonObject payload;
                payload.insert("slot", masterSlot);

                if (!sendCommand("disconnect_port", payload) && m_operationOverlay)
                    m_operationOverlay->showFailure("断开命令发送失败");
            });

    connect(_Myclient, &IpcClient::portsUpdated, this, [this, pageSetting](const QStringList &ports) {
        qDebug() << "[IPC][portsUpdated] count:" << ports.size() << "ports:" << ports;

        QList<MasterPortInfo> portInfos;
        for (int i = 0; i < ports.size(); ++i) {
            MasterPortInfo info;
            info.masterSlot = i;
            info.masterName = QString("RS485-%1").arg(i + 1);
            info.deviceNode = ports.at(i);
            info.areaName = QString();
            info.baudRate = 9600;
            info.connected = false;
            portInfos.append(info);
        }

        pageSetting->setPortList(portInfos);
        refreshHomeMasterAndSlaveList();

        if (m_operationOverlay)
            m_operationOverlay->showSuccess("扫描完成");
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
                m_operationOverlay->showFailure("未选择端口");
            return;
        }

        if (!m_connectedMasterSlots.contains(masterSlot)) {
            if (m_operationOverlay)
                m_operationOverlay->showFailure("端口未连接");
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
                       int pollIntervalMs,
                       const QJsonObject &thresholdPayload) {
                    QJsonObject payload;
                    payload.insert("slot", slot);
                    payload.insert("slave_id", slaveId);
                    payload.insert("device_type", deviceType);
                    payload.insert("poll_interval_ms", pollIntervalMs);
                    for (auto it = thresholdPayload.constBegin();
                         it != thresholdPayload.constEnd();
                         ++it) {
                        payload.insert(it.key(), it.value());
                    }

                    quint32 seq = 0;
                    if (m_addSlaveDialog)
                        m_addSlaveDialog->setAdding();

                    if (!sendCommand("add_device", payload, &seq)) {
                        if (m_addSlaveDialog)
                        m_addSlaveDialog->setResult(false, "命令发送失败");
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
                    m_operationOverlay->showFailure("未知继电器通道");
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
                    m_operationOverlay->showLoading("正在发送继电器命令...");

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
                    m_operationOverlay->showFailure("继电器命令失败");
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
                        upsertRegisteredSlave(m_pendingAddSlave.masterSlot,
                                              m_pendingAddSlave.slaveId,
                                              QString("从站 %1").arg(m_pendingAddSlave.slaveId),
                                              m_pendingAddSlave.deviceType);
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
                    m_operationOverlay->showSuccess("连接完成");
                else if (cmd == "disconnect_port")
                    m_operationOverlay->showSuccess("断开完成");
                else if (cmd == "add_device")
                    m_operationOverlay->showSuccess("添加从站完成");
                else if (cmd == "set_relay")
                    m_operationOverlay->showSuccess("继电器命令完成");
            });

    connect(_Myclient,
            &IpcClient::emergencyReceived,
            this,
            [this](int level,
                   const QString &reason,
                   int deviceId,
                   const QString &deviceType,
                   const QString &pointKey,
                   double value,
                   double threshold,
                   double temp,
                   double humi) {
                Q_UNUSED(level);
                Q_UNUSED(deviceType);
                Q_UNUSED(temp);
                Q_UNUSED(humi);

                ++m_activeAlarmCount;

                const QString alarmName = !reason.isEmpty() ? reason : pointKey;
                const QString relation = (reason.endsWith("_low") || value < threshold)
                    ? "<"
                    : ">";

                if (m_pageStatus) {
                    m_pageStatus->setAlarmText(QString("告警：从站 %1 %2 %3 %4 %5")
                                                   .arg(deviceId)
                                                   .arg(alarmName)
                                                   .arg(value, 0, 'f', 1)
                                                   .arg(relation)
                                                   .arg(threshold, 0, 'f', 1));
                }
                refreshStatusSummary();
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

    if (_Myclient && _Myclient->isConnected())
        handleIpcConnected(top, pageInfo);
    else
        handleIpcDisconnected(top, pageInfo, pageSetting);
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

void Widget::handleIpcConnected(TopStatusBar *topBar, Pageinfo *pageInfo)
{
    if (topBar)
        topBar->setBackendConnected(true);
    if (pageInfo)
        pageInfo->setIpcConnected(true);
    if (m_pageStatus)
        m_pageStatus->setAlarmText("告警：--");
}

void Widget::handleIpcDisconnected(TopStatusBar *topBar,
                                   Pageinfo *pageInfo,
                                   PageSetting *pageSetting)
{
    if (topBar)
        topBar->setBackendConnected(false);
    if (pageInfo)
        pageInfo->setIpcConnected(false);
    if (pageSetting)
        pageSetting->setUnscannedState();

    m_connectedMasterSlots.clear();
    m_slaveDevices.clear();
    m_relayStates.clear();
    m_pendingAddSlave = PendingAddSlave();
    m_activeAlarmCount = 0;

    refreshHomeMasterAndSlaveList();
    refreshStatusSummary();

    if (m_pageStatus)
        m_pageStatus->setAlarmText("告警：IPC 未连接，请重新连接后端");

    if (m_addSlaveDialog)
        m_addSlaveDialog->setResult(false, "IPC 未连接");

    if (m_operationOverlay)
        m_operationOverlay->showFailure("IPC 未连接，请重新连接后端");
}

void Widget::handleDeviceStatus(const DataPack &pack)
{
    const QString updateTime = pack.time.isValid()
        ? pack.time.toString("HH:mm:ss")
        : QDateTime::currentDateTime().toString("HH:mm:ss");

    for (const DeviceData &device : pack.devices) {
        int masterSlot = pack.masterSlot;
        QString deviceType;
        for (const SlaveDeviceInfo &slave : m_slaveDevices) {
            if (slave.slaveAddr == device.deviceId &&
                (masterSlot < 0 || slave.masterSlot == masterSlot)) {
                masterSlot = slave.masterSlot;
                deviceType = slave.deviceType;
                break;
            }
        }
        if (masterSlot < 0 && m_connectedMasterSlots.size() == 1)
            masterSlot = *m_connectedMasterSlots.constBegin();
        if (masterSlot < 0)
            masterSlot = 0;
        if (!m_connectedMasterSlots.contains(masterSlot))
            continue;

        if (deviceType.isEmpty()) {
            if (device.type == DEV_SENSOR_TH)
                deviceType = "sensor_th";
            else if (device.type == DEV_RELAY)
                deviceType = "relay";
            else
                deviceType = "unknown";
        }

        if (!device.valid) {
            if (device.errorMessage == "device_offline" ||
                device.errorMessage == "modbus_timeout") {
                updateSlaveOnline(masterSlot, device.deviceId, deviceType, false);
            }
            continue;
        }

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

void Widget::upsertRegisteredSlave(int masterSlot,
                                   int slaveAddr,
                                   const QString &deviceName,
                                   const QString &deviceType)
{
    if (masterSlot < 0 || slaveAddr <= 0 || deviceType.isEmpty())
        return;

    const QString displayName = deviceType == "sensor_th"
        ? "温湿度传感器"
        : (deviceType == "relay" ? "继电器" : deviceType);
    const QString name = deviceName.isEmpty()
        ? QString("从站 %1").arg(slaveAddr)
        : deviceName;

    for (SlaveDeviceInfo &slave : m_slaveDevices) {
        if (slave.masterSlot != masterSlot ||
            slave.slaveAddr != slaveAddr ||
            slave.deviceType != deviceType) {
            continue;
        }

        slave.deviceName = name;
        slave.displayName = displayName;
        return;
    }

    SlaveDeviceInfo info;
    info.masterSlot = masterSlot;
    info.slaveAddr = slaveAddr;
    info.deviceName = name;
    info.displayName = displayName;
    info.deviceType = deviceType;
    info.online = false;
    m_slaveDevices.append(info);
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
    if (m_pageTrend) {
        m_pageTrend->setMasterList(masters);
        m_pageTrend->setSlaveList(m_slaveDevices);
    }

    int targetSlot = preferredMasterSlot >= 0
        ? preferredMasterSlot
        : m_pageStatus->currentMasterSlotValue();
    if (targetSlot >= 0 && !m_connectedMasterSlots.contains(targetSlot))
        targetSlot = -1;


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
                                       m_activeAlarmCount,
                                       "--");
    }
}

QString Widget::masterNameForSlot(int masterSlot) const
{
    return QString("RS485-%1").arg(masterSlot + 1);
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
