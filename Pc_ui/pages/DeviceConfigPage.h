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
    void addSlaveRequested(const QString &gatewayId, const QString &portId, int deviceId,
                           const QString &deviceType, int pollIntervalMs);
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
        QString devicePath;
        int masterSlot = 0;
        int baud = 9600;
        QString areaName;
        int slaveCount = 0;
        qint64 lastUpdateTime = 0;
        QString status;
    };

    QList<MasterRow> buildMasterRows() const;
    QString statusText(const QString &status) const;
    QString displayTime(qint64 timestampMs) const;
    void setupTable(QTableWidget *table) const;
    void addStatusItem(QTableWidget *table, int row, int column, const QString &status) const;
    void addDeleteButton(QTableWidget *table, int row, bool master, const QString &gatewayId,
                         const QString &portId, int deviceId = 0);
    void showAddSlaveDialog();
    void showScanPlaceholder();

private:
    DeviceManager *m_device = nullptr;
    ConfigManager *m_config = nullptr;
    QTableWidget *m_masterTable = nullptr;
    QTableWidget *m_slaveTable = nullptr;
    QTimer *m_refreshTimer = nullptr;
};
