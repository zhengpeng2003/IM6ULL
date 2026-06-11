#pragma once

#include <QList>
#include <QWidget>

#include "model/DeviceModel.h"

class ConfigManager;
class DeviceManager;
class QTableWidget;
class QTimer;

class DeviceConfigPage : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceConfigPage(DeviceManager *device, ConfigManager *config, QWidget *parent = nullptr);

signals:
    void deleteMasterDataRequested(const QString &gatewayId, const QString &portId);
    void deleteDeviceDataRequested(const QString &gatewayId, const QString &portId, int deviceId);

public slots:
    void refreshTables();

private:
    struct MasterRow
    {
        QString gatewayId;
        QString gatewayName;
        QString portId;
        QString portName;
        int masterSlot = 0;
        int baud = 9600;
        QString areaName;
        int slaveCount = 0;
        qint64 lastUpdateTime = 0;
        bool online = false;
    };

    QList<MasterRow> buildMasterRows(const QList<DeviceNode> &devices) const;
    bool isOnline(qint64 lastUpdateTime) const;
    QString statusText(bool online) const;
    QString displayTime(qint64 timestampMs) const;
    void setupTable(QTableWidget *table) const;
    void addStatusItem(QTableWidget *table, int row, int column, bool online) const;
    void addDeleteButton(QTableWidget *table, int row, bool master, const QString &gatewayId,
                         const QString &portId, int deviceId = 0);
    void showScanPlaceholder();

private:
    DeviceManager *m_device = nullptr;
    ConfigManager *m_config = nullptr;
    QTableWidget *m_masterTable = nullptr;
    QTableWidget *m_slaveTable = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
