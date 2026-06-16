#include "widget.h"
#include <algorithm>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QVBoxLayout>
#include <QDateTime>
#include <QTimer>
#include <QMessageBox>

#include "../pageui/addslavedialog.h"

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
    m_ipcReconnectTimer = new QTimer(this);
    m_ipcReconnectTimer->setInterval(2000);
    connect(m_ipcReconnectTimer, &QTimer::timeout, this, [this]() {
        if (!_Myclient || _Myclient->isConnected()) {
            stopIpcAutoReconnect();
            return;
        }

        if (_Myclient->connectToServer("/tmp/device_ipc.sock", 300)) {
            stopIpcAutoReconnect();
        }
    });

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
    connect(pageInfo, &Pageinfo::offlineCacheConfigChanged,
            this,
            [this, pageInfo](bool cacheEnabled, bool flushEnabled) {
                QJsonObject payload;
                payload.insert("cache_enabled", cacheEnabled);
                payload.insert("flush_enabled", flushEnabled);
                if (!sendCommand("set_offline_cache_config", payload)) {
                    if (pageInfo)
                        //pageInfo->setOfflineCacheBusy(false);
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("缓存配置发送失败");
                }
            });
    connect(pageInfo, &Pageinfo::offlineCacheRefreshRequested,
            this,
            [this]() {
                if (!sendCommand("get_offline_cache_config") && m_operationOverlay)
                    m_operationOverlay->showFailure("刷新缓存状态失败");
            });
    connect(pageInfo, &Pageinfo::clearOfflineCacheRequested,
            this,
            [this, pageInfo]() {
                if (!sendCommand("clear_offline_cache")) {
                    if (pageInfo)
                        //pageInfo->setOfflineCacheBusy(false);
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("清除缓存命令失败");
                }
            });
    connect(pageInfo, &Pageinfo::flushOfflineCacheRequested,
            this,
            [this, pageInfo]() {
                if (!sendCommand("flush_offline_cache")) {
                    if (pageInfo)
                        //pageInfo->setOfflineCacheBusy(false);
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("发送缓存命令失败");
                }
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

                upsertRegisteredSlave(slot, deviceId, deviceName, deviceType);
                refreshHomeMasterAndSlaveList(slot);
            });
    connect(_Myclient,
            &IpcClient::runtimeStateReceived,
            this,
            [this](quint32 seq, const QJsonArray &ports) {
                Q_UNUSED(seq);
                handleRuntimeStateSnapshot(ports);
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

                if (QMessageBox::question(this,
                                          "断开端口",
                                          "确定断开当前端口通信吗？端口配置会保留，重启后可能自动恢复。",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) != QMessageBox::Yes)
                    return;

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
        QSet<int> addedSlots;
        for (int i = 0; i < ports.size(); ++i) {
            MasterPortInfo info;
            info.deviceNode = ports.at(i);
            info.masterSlot = i;
            for (auto it = m_runtimePorts.constBegin(); it != m_runtimePorts.constEnd(); ++it) {
                if (it.value().deviceNode == info.deviceNode) {
                    info.masterSlot = it.key();
                    break;
                }
            }
            info.masterName = masterNameForSlot(info.masterSlot);
            info.areaName = QString();
            info.baudRate = 9600;
            info.connected = false;
            if (m_runtimePorts.contains(info.masterSlot)) {
                const MasterPortInfo cached = m_runtimePorts.value(info.masterSlot);
                info.baudRate = cached.baudRate > 0 ? cached.baudRate : info.baudRate;
                info.connected = cached.connected && cached.deviceNode == info.deviceNode;
            }
            portInfos.append(info);
            addedSlots.insert(info.masterSlot);
        }

        for (auto it = m_runtimePorts.constBegin(); it != m_runtimePorts.constEnd(); ++it) {
            const MasterPortInfo cached = it.value();
            if (addedSlots.contains(it.key()) || cached.deviceNode.isEmpty())
                continue;

            portInfos.append(cached);
            addedSlots.insert(it.key());
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
                    MasterPortInfo info = m_runtimePorts.value(slot);
                    info.masterSlot = slot;
                    info.masterName = masterNameForSlot(slot);
                    info.deviceNode = port;
                    info.baudRate = baud > 0 ? baud : 9600;
                    info.connected = true;
                    m_runtimePorts.insert(slot, info);
                    refreshHomeMasterAndSlaveList(slot);
                    if (message != "restored")
                        (void)sendCommand("get_runtime_state");
                } else {
                    m_connectedMasterSlots.remove(slot);
                    MasterPortInfo info = m_runtimePorts.value(slot);
                    info.masterSlot = slot;
                    info.masterName = masterNameForSlot(slot);
                    if (!port.isEmpty())
                        info.deviceNode = port;
                    if (baud > 0)
                        info.baudRate = baud;
                    info.connected = false;
                    m_runtimePorts.insert(slot, info);
                    markMasterRuntimeOffline(slot);
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
            &PageStatus::removeSlaveRequested,
            this,
            [this](int masterSlot, int slaveAddr, const QString &deviceType) {
                if (masterSlot < 0 || slaveAddr <= 0) {
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("从站信息无效");
                    return;
                }

                if (!m_connectedMasterSlots.contains(masterSlot)) {
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("端口未连接");
                    return;
                }

                if (QMessageBox::question(this,
                                          "删除从站",
                                          "确认删除该从站配置？",
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) != QMessageBox::Yes)
                    return;

                QJsonObject payload;
                payload.insert("slot", masterSlot);
                payload.insert("slave_id", slaveAddr);
                payload.insert("device_type", deviceType);

                quint32 seq = 0;
                if (m_operationOverlay)
                    m_operationOverlay->showLoading("正在删除从站...");

                if (!sendCommand("remove_device", payload, &seq)) {
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure("删除命令发送失败");
                    return;
                }

                m_pendingRemoveSlave.active = true;
                m_pendingRemoveSlave.seq = seq;
                m_pendingRemoveSlave.masterSlot = masterSlot;
                m_pendingRemoveSlave.slaveAddr = slaveAddr;
                m_pendingRemoveSlave.deviceType = deviceType;
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
                payload.insert("device_id", slaveAddr);
                payload.insert("states", relayStatesToArray(states));

                if (m_operationOverlay)
                    m_operationOverlay->showLoading("继电器命令执行中...");

                quint32 seq = 0;
                if (sendCommand("set_relay", payload, &seq)) {
                    m_pendingRelay.active = true;
                    m_pendingRelay.seq = seq;
                    m_pendingRelay.masterSlot = masterSlot;
                    m_pendingRelay.slaveAddr = slaveAddr;
                    m_pendingRelay.deviceId = slaveAddr;
                    m_pendingRelay.oldStates = m_relayStates.value(key, 0);
                    m_pendingRelay.expectedStates = states;
                    m_pendingRelay.startTime = QDateTime::currentDateTime();
                    m_pendingRelay.ackReceived = false;
                    m_pendingRelay.confirmedByTelemetry = false;
                } else if (m_operationOverlay) {
                    m_operationOverlay->showFailure("继电器命令发送失败");
                }
            });

    connect(_Myclient,
            &IpcClient::commandAckReceived,
            this,
            [this](quint32 seq,
                   const QString &cmd,
                   const QString &status,
                   const QString &reason,
                   const QString &message,
                   const QJsonObject &ackRoot) {
                qDebug() << "[IPC][ack]"
                         << "seq:" << seq
                         << "cmd:" << cmd
                         << "status:" << status
                         << "reason:" << reason
                         << "message:" << message;

                const bool wasPending = m_pendingCommands.contains(seq);
                completePendingCommand(seq);

                const bool ok = ackSucceeded(status, ackRoot);
                const QString text = ackDisplayText(cmd, ok, reason, message);

                if (cmd == "get_offline_cache_config" ||
                    cmd == "set_offline_cache_config" ||
                    cmd == "clear_offline_cache" ||
                    cmd == "flush_offline_cache") {
                    handleOfflineCacheAck(cmd, ok ? "ok" : "failed", reason, message, ackRoot);
                    return;
                }

                if (!ok) {
                    if (cmd == "add_device" && m_pendingAddSlave.active && m_pendingAddSlave.seq == seq) {
                        if (m_addSlaveDialog)
                            m_addSlaveDialog->setResult(false, text);
                        m_pendingAddSlave = PendingAddSlave();
                    }
                    if (cmd == "remove_device" && m_pendingRemoveSlave.active && m_pendingRemoveSlave.seq == seq)
                        m_pendingRemoveSlave = PendingRemoveSlave();
                    if (cmd == "set_relay" && m_pendingRelay.active && m_pendingRelay.seq == seq)
                        failPendingRelay(text);
                    if (m_operationOverlay)
                        m_operationOverlay->showFailure(text);
                    return;
                }

                if (cmd == "add_device" && m_pendingAddSlave.active && m_pendingAddSlave.seq == seq) {
                    if (m_addSlaveDialog)
                        m_addSlaveDialog->setResult(true, "添加从站完成");
                    m_pendingAddSlave = PendingAddSlave();
                    requestRuntimeRefresh();
                    return;
                }

                if (cmd == "remove_device") {
                    handleRemoteRemoveDeviceAck(ackRoot);
                    if (m_pendingRemoveSlave.active && m_pendingRemoveSlave.seq == seq) {
                        m_pendingRemoveSlave = PendingRemoveSlave();
                    }
                    if (m_operationOverlay)
                        m_operationOverlay->showSuccess("从站已删除");
                    requestRuntimeRefresh();
                    return;
                }

                if (cmd == "set_relay" && m_pendingRelay.active && m_pendingRelay.seq == seq) {
                    m_pendingRelay.ackReceived = true;
                    if (m_operationOverlay)
                        m_operationOverlay->showLoading("写入成功，等待状态回读");
                    requestRuntimeRefresh();
                    QTimer::singleShot(8000, this, [this, seq]() {
                        if (!m_pendingRelay.active || m_pendingRelay.seq != seq || m_pendingRelay.confirmedByTelemetry)
                            return;
                        m_pendingRelay = PendingRelayCommand();
                        if (m_operationOverlay)
                            m_operationOverlay->showFailure("继电器写入成功，但状态未确认，请刷新状态");
                    });
                    return;
                }

                if (!wasPending)
                    return;

                if (m_operationOverlay && cmd != "scan_ports" && cmd != "get_runtime_state")
                    m_operationOverlay->showSuccess(text);

                if (cmd == "connect_port" || cmd == "disconnect_port" ||
                    cmd == "set_device_threshold" || cmd == "add_device" ||
                    cmd == "remove_device") {
                    requestRuntimeRefresh();
                }
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
        if (!_Myclient || !_Myclient->isConnected())
            startIpcAutoReconnect();
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
    stopIpcAutoReconnect();
    if (topBar)
        topBar->setBackendConnected(true);
    if (pageInfo)
        pageInfo->setIpcConnected(true);
    if (m_pageStatus)
        m_pageStatus->setAlarmText("告警：--");

    requestRuntimeRefresh();
    requestOfflineCacheConfig();
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

    for (SlaveDeviceInfo &slave : m_slaveDevices) {
        slave.online = false;
        if (m_pageStatus)
            m_pageStatus->updateSlaveOnline(slave.masterSlot, slave.slaveAddr, slave.deviceType, false);
    }
    m_connectedMasterSlots.clear();
    m_relayStates.clear();
    m_runtimePorts.clear();
    failAllPendingCommands("IPC 未连接");
    m_pendingAddSlave = PendingAddSlave();
    m_pendingRemoveSlave = PendingRemoveSlave();
    m_activeAlarmCount = 0;

    refreshHomeMasterAndSlaveList();
    refreshStatusSummary();

    if (m_pageStatus)
        m_pageStatus->setAlarmText("告警：IPC 未连接，请重新连接后端");

    if (m_addSlaveDialog)
        m_addSlaveDialog->setResult(false, "IPC 未连接");

    if (m_operationOverlay)
        m_operationOverlay->showFailure("IPC 未连接，请重新连接后端");

    startIpcAutoReconnect();
}

void Widget::startIpcAutoReconnect()
{
    if (!m_ipcReconnectTimer || (_Myclient && _Myclient->isConnected()))
        return;

    if (!m_ipcReconnectTimer->isActive())
        m_ipcReconnectTimer->start();
}

void Widget::stopIpcAutoReconnect()
{
    if (m_ipcReconnectTimer && m_ipcReconnectTimer->isActive())
        m_ipcReconnectTimer->stop();
}

void Widget::requestRuntimeRefresh()
{
    if (!_Myclient || !_Myclient->isConnected())
        return;

    (void)sendCommand("get_runtime_state");
    (void)sendCommand("scan_ports");
}

void Widget::requestOfflineCacheConfig()
{
    if (!_Myclient || !_Myclient->isConnected())
        return;

    (void)sendCommand("get_offline_cache_config");
}

void Widget::handleOfflineCacheAck(const QString &cmd,
                                   const QString &status,
                                   const QString &reason,
                                   const QString &message,
                                   const QJsonObject &ackRoot)
{
    Pageinfo *pageInfo = nullptr;
    if (m_stack)
        pageInfo = qobject_cast<Pageinfo *>(m_stack->widget(3));

    if (status != "ok" && reason == "unknown_command") {
        if (pageInfo)
            pageInfo->setOfflineCacheUnsupported();
        if (m_operationOverlay && cmd != "get_offline_cache_config")
            m_operationOverlay->showFailure("缓存命令不支持");
        return;
    }

    const bool cacheEnabled = ackRoot.contains("cache_enabled")
                                  ? ackRoot.value("cache_enabled").toBool(false)
                                  : ackRoot.value("cacheEnabled").toBool(false);
    const bool flushEnabled = ackRoot.contains("flush_enabled")
                                  ? ackRoot.value("flush_enabled").toBool(false)
                                  : ackRoot.value("flushEnabled").toBool(false);
    const int pendingCount = ackRoot.contains("pending_count")
                                 ? ackRoot.value("pending_count").toInt(0)
                                 : ackRoot.value("pendingCount").toInt(0);
    if (pageInfo)
        pageInfo->updateOfflineCacheStatus(cacheEnabled, flushEnabled, pendingCount);

    if (!m_operationOverlay || cmd == "get_offline_cache_config")
        return;

    if (status != "ok") {
        QString failure = !message.isEmpty() ? message : (reason.isEmpty() ? "缓存命令失败" : reason);
        if (reason == "mqtt_disconnected" || reason == "mqtt_not_connected")
            failure = "MQTT 未连接，无法发送缓存";
        else if (reason == "offline_cache_flush_disabled")
            failure = "自动发送未开启";
        else if (reason == "offline_cache_flush_failed")
            failure = "离线缓存发送失败";
        else if (reason == "offline_cache_config_save_failed")
            failure = "缓存配置保存失败";
        else if (reason == "offline_cache_clear_failed")
            failure = "离线缓存清空失败";
        m_operationOverlay->showFailure(failure);
        return;
    }

    if (cmd == "set_offline_cache_config") {
        m_operationOverlay->showSuccess("缓存配置已保存");
        requestOfflineCacheConfig();
    } else if (cmd == "clear_offline_cache") {
        m_operationOverlay->showSuccess("缓存队列已清空");
        requestOfflineCacheConfig();
    } else if (cmd == "flush_offline_cache") {
        m_operationOverlay->showSuccess("已触发发送，正在刷新缓存状态");
        requestOfflineCacheConfig();
    }
}

QJsonArray Widget::relayStatesToArray(int states) const
{
    QJsonArray array;
    for (int bit = 0; bit < 4; ++bit)
        array.append((states & (1 << bit)) != 0);
    return array;
}

void Widget::failPendingRelay(const QString &message)
{
    if (!m_pendingRelay.active)
        return;
    const QString key = relayStateKey(m_pendingRelay.masterSlot, m_pendingRelay.slaveAddr);
    m_relayStates.insert(key, m_pendingRelay.oldStates);
    if (m_pageStatus) {
        const int states = m_pendingRelay.oldStates;
        m_pageStatus->setRelayStates(m_pendingRelay.masterSlot,
                                     m_pendingRelay.slaveAddr,
                                     (states & 0x01) != 0,
                                     (states & 0x02) != 0,
                                     (states & 0x04) != 0,
                                     QDateTime::currentDateTime().toString("HH:mm:ss"));
    }
    m_pendingRelay = PendingRelayCommand();
    if (m_operationOverlay)
        m_operationOverlay->showFailure(message.isEmpty() ? QStringLiteral("继电器命令失败") : message);
}

void Widget::handleRelayTelemetryConfirm(int masterSlot, int slaveAddr, int states, const QString &updateTime)
{
    const QString key = relayStateKey(masterSlot, slaveAddr);
    m_relayStates.insert(key, states);
    if (m_pageStatus) {
        m_pageStatus->setRelayStates(masterSlot,
                                     slaveAddr,
                                     (states & 0x01) != 0,
                                     (states & 0x02) != 0,
                                     (states & 0x04) != 0,
                                     updateTime);
    }

    if (!m_pendingRelay.active ||
        m_pendingRelay.masterSlot != masterSlot ||
        m_pendingRelay.slaveAddr != slaveAddr) {
        return;
    }

    if (states == m_pendingRelay.expectedStates) {
        m_pendingRelay.confirmedByTelemetry = true;
        m_pendingRelay = PendingRelayCommand();
        if (m_operationOverlay)
            m_operationOverlay->showSuccess("现场状态已确认");
    }
}

void Widget::handleRuntimeStateSnapshot(const QJsonArray &ports)
{
    QSet<int> snapshotSlots;
    QSet<QString> snapshotSlaveKeys;

    for (const QJsonValue &portValue : ports) {
        if (!portValue.isObject())
            continue;

        const QJsonObject portObj = portValue.toObject();
        const int slot = portObj.value("slot").toInt(-1);
        if (slot < 0)
            continue;

        const QString port = portObj.value("port").toString();
        const int baud = portObj.value("baud").toInt(9600);
        const bool connected = portObj.value("connected").toBool(false);
        snapshotSlots.insert(slot);

        MasterPortInfo info = m_runtimePorts.value(slot);
        info.masterSlot = slot;
        info.masterName = masterNameForSlot(slot);
        if (!port.isEmpty())
            info.deviceNode = port;
        info.baudRate = baud > 0 ? baud : info.baudRate;
        info.connected = connected;
        m_runtimePorts.insert(slot, info);

        if (connected)
            m_connectedMasterSlots.insert(slot);
        else
            m_connectedMasterSlots.remove(slot);

        const QJsonArray devices = portObj.value("devices").toArray();
        for (const QJsonValue &deviceValue : devices) {
            if (!deviceValue.isObject())
                continue;

            const QJsonObject deviceObj = deviceValue.toObject();
            const int deviceId = deviceObj.value("deviceId").toInt(deviceObj.value("slave_id").toInt());
            const QString deviceType = deviceObj.value("deviceType").toString();
            if (deviceId <= 0 || deviceType.isEmpty())
                continue;

            snapshotSlaveKeys.insert(QString("%1:%2:%3").arg(slot).arg(deviceId).arg(deviceType));
            upsertRegisteredSlave(slot,
                                  deviceId,
                                  deviceObj.value("deviceName").toString(),
                                  deviceType);
        }
    }

    for (int i = m_slaveDevices.size() - 1; i >= 0; --i) {
        const SlaveDeviceInfo &slave = m_slaveDevices.at(i);
        if (!snapshotSlots.contains(slave.masterSlot))
            continue;

        const QString key = QString("%1:%2:%3")
            .arg(slave.masterSlot)
            .arg(slave.slaveAddr)
            .arg(slave.deviceType);
        if (!snapshotSlaveKeys.contains(key)) {
            const int slaveSlot = slave.masterSlot;
            const int slaveAddr = slave.slaveAddr;
            const QString deviceType = slave.deviceType;
            removeRegisteredSlave(slaveSlot, slaveAddr, deviceType);
        }
    }

    refreshHomeMasterAndSlaveList();
    refreshStatusSummary();
}

int Widget::slotFromPortId(const QString &portId) const
{
    if (portId == "port_001" || portId == "RS485-1")
        return 0;
    if (portId == "port_002" || portId == "RS485-2")
        return 1;
    return -1;
}

void Widget::handleRemoteRemoveDeviceAck(const QJsonObject &ack)
{
    int masterSlot = ack.value("slot").toInt(-1);
    int slaveAddr = ack.value("slave_id").toInt(ack.value("deviceId").toInt(0));
    const QString deviceType = ack.value("deviceType").toString(ack.value("device_type").toString());

    const QJsonObject target = ack.value("target").toObject();
    if (masterSlot < 0 && !target.isEmpty())
        masterSlot = slotFromPortId(target.value("portId").toString());
    if (slaveAddr <= 0)
        slaveAddr = target.value("deviceId").toInt(target.value("slave_id").toInt(0));
    if (masterSlot < 0 && m_pendingRemoveSlave.active)
        masterSlot = m_pendingRemoveSlave.masterSlot;
    if (slaveAddr <= 0 && m_pendingRemoveSlave.active)
        slaveAddr = m_pendingRemoveSlave.slaveAddr;
    Q_UNUSED(deviceType);

    if (masterSlot < 0 || slaveAddr <= 0)
        return;

    bool removed = false;
    for (int i = m_slaveDevices.size() - 1; i >= 0; --i) {
        const SlaveDeviceInfo slave = m_slaveDevices.at(i);
        if (slave.masterSlot != masterSlot || slave.slaveAddr != slaveAddr)
            continue;

        removeRegisteredSlave(slave.masterSlot, slave.slaveAddr, slave.deviceType);
        removed = true;
    }

    if (removed)
        refreshHomeMasterAndSlaveList(masterSlot);
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
            updateSlaveOnline(masterSlot, device.deviceId, deviceType, false);
            if (m_pageStatus) {
                const QString reason = device.errorMessage.isEmpty() ? QStringLiteral("数据无效") : device.errorMessage;
                m_pageStatus->setAlarmText(QStringLiteral("告警：从站 %1 %2").arg(device.deviceId).arg(reason));
            }
            continue;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (device.timestampMs > 0 && nowMs - device.timestampMs > 30000) {
            updateSlaveOnline(masterSlot, device.deviceId, deviceType, false);
            if (m_pageStatus)
                m_pageStatus->setAlarmText(QStringLiteral("告警：从站 %1 数据已过期，保留最后值").arg(device.deviceId));
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
            handleRelayTelemetryConfirm(masterSlot, device.deviceId, device.relayStates, updateTime);
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

        slave.displayName = name.isEmpty() ? displayName : name;
        return;
    }

    SlaveDeviceInfo info;
    info.masterSlot = masterSlot;
    info.slaveAddr = slaveAddr;
    info.displayName = name.isEmpty() ? displayName : name;
    info.deviceType = deviceType;
    info.online = false;
    m_slaveDevices.append(info);
}

void Widget::removeRegisteredSlave(int masterSlot,
                                   int slaveAddr,
                                   const QString &deviceType)
{
    for (int i = m_slaveDevices.size() - 1; i >= 0; --i) {
        const SlaveDeviceInfo &slave = m_slaveDevices.at(i);
        if (slave.masterSlot != masterSlot ||
            slave.slaveAddr != slaveAddr ||
            slave.deviceType != deviceType) {
            continue;
        }

        m_slaveDevices.removeAt(i);
    }

    m_relayStates.remove(relayStateKey(masterSlot, slaveAddr));
    if (m_pageStatus)
        m_pageStatus->removeSlave(masterSlot, slaveAddr, deviceType);
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
    m_pageStatus->setMasterList(masters);

    int targetSlot = preferredMasterSlot >= 0
        ? preferredMasterSlot
        : m_pageStatus->currentMasterSlotValue();
    if (targetSlot >= 0 && !m_connectedMasterSlots.contains(targetSlot))
        targetSlot = -1;
    if (targetSlot < 0 && !connectedSlots.isEmpty())
        targetSlot = connectedSlots.first();


    QList<SlaveDeviceInfo> visibleSlaves;
    if (targetSlot >= 0) {
        for (const SlaveDeviceInfo &slave : m_slaveDevices) {
            if (slave.masterSlot == targetSlot)
                visibleSlaves.append(slave);
        }
    }

    m_pageStatus->setCurrentMaster(targetSlot,
                                   targetSlot >= 0 ? masterNameForSlot(targetSlot) : QString(),
                                   visibleSlaves.size());
    m_pageStatus->setSlaveList(visibleSlaves);
    refreshStatusSummary();
}

void Widget::markMasterRuntimeOffline(int masterSlot)
{
    for (SlaveDeviceInfo &slave : m_slaveDevices) {
        if (slave.masterSlot == masterSlot)
            slave.online = false;
    }

    const QString keyPrefix = QString("%1:").arg(masterSlot);
    const QList<QString> relayKeys = m_relayStates.keys();
    for (const QString &key : relayKeys) {
        if (key.startsWith(keyPrefix))
            m_relayStates.remove(key);
    }

    if (m_pendingAddSlave.active && m_pendingAddSlave.masterSlot == masterSlot)
        m_pendingAddSlave = PendingAddSlave();
    if (m_pendingRemoveSlave.active && m_pendingRemoveSlave.masterSlot == masterSlot)
        m_pendingRemoveSlave = PendingRemoveSlave();
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

bool Widget::ackSucceeded(const QString &status, const QJsonObject &ackRoot) const
{
    if (ackRoot.contains("ok"))
        return ackRoot.value("ok").toBool(false);
    return status == "ok";
}

QString Widget::ackDisplayText(const QString &cmd,
                               bool ok,
                               const QString &reason,
                               const QString &message) const
{
    if (!message.isEmpty())
        return message;
    if (!reason.isEmpty())
        return reason;

    if (!ok)
        return "命令执行失败";
    if (cmd == "connect_port")
        return "连接完成";
    if (cmd == "disconnect_port")
        return "端口通信已断开，配置已保留";
    if (cmd == "add_device")
        return "添加从站完成";
    if (cmd == "remove_device")
        return "删除从站完成";
    if (cmd == "set_device_threshold")
        return "阈值已保存";
    return "命令执行完成";
}

void Widget::completePendingCommand(quint32 seq)
{
    auto it = m_pendingCommands.find(seq);
    if (it == m_pendingCommands.end())
        return;
    if (it->timer) {
        it->timer->stop();
        it->timer->deleteLater();
    }
    m_pendingCommands.erase(it);
}

void Widget::failAllPendingCommands(const QString &message)
{
    const QList<quint32> seqs = m_pendingCommands.keys();
    for (quint32 seq : seqs)
        completePendingCommand(seq);
    m_pendingAddSlave = PendingAddSlave();
    m_pendingRemoveSlave = PendingRemoveSlave();
    m_pendingRelay = PendingRelayCommand();
    if (m_addSlaveDialog)
        m_addSlaveDialog->setResult(false, message);
}

bool Widget::sendCommand(const QString &cmd, QJsonObject payload, quint32 *seqOut)
{
    if (!_Myclient || !_Myclient->isConnected()) {
        if (m_operationOverlay)
            m_operationOverlay->showFailure("后端未连接");
        return false;
    }

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

    const bool sent = _Myclient->sendMessage(msg);
    qDebug() << "[IPC][sendCommandResult]"
             << "seq:" << seq
             << "cmd:" << cmd
             << "ok:" << sent;

    if (!sent)
        return false;

    PendingCommand pending;
    pending.cmd = cmd;
    pending.startedAt.start();
    pending.timer = new QTimer(this);
    pending.timer->setSingleShot(true);
    pending.timer->setInterval(5000);
    connect(pending.timer, &QTimer::timeout, this, [this, seq]() {
        auto it = m_pendingCommands.find(seq);
        if (it == m_pendingCommands.end())
            return;
        const QString timedOutCmd = it->cmd;
        completePendingCommand(seq);
        if (m_pendingAddSlave.active && m_pendingAddSlave.seq == seq) {
            if (m_addSlaveDialog)
                m_addSlaveDialog->setResult(false, "通信超时，请刷新状态");
            m_pendingAddSlave = PendingAddSlave();
        }
        if (m_pendingRemoveSlave.active && m_pendingRemoveSlave.seq == seq)
            m_pendingRemoveSlave = PendingRemoveSlave();
        if (m_pendingRelay.active && m_pendingRelay.seq == seq)
            failPendingRelay("命令超时");
        if (timedOutCmd == "set_offline_cache_config" ||
            timedOutCmd == "clear_offline_cache" ||
            timedOutCmd == "flush_offline_cache") {
            if (m_stack) {
                Pageinfo *pageInfo = qobject_cast<Pageinfo *>(m_stack->widget(3));
                //if (pageInfo)
                    //pageInfo->setOfflineCacheBusy(false);
            }
        }
        if (m_operationOverlay && timedOutCmd != "get_runtime_state" && timedOutCmd != "set_relay")
            m_operationOverlay->showFailure("通信超时，请刷新状态");
    });
    pending.timer->start();
    m_pendingCommands.insert(seq, pending);
    return true;
}
