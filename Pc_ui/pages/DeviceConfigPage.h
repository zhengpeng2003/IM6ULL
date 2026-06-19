#pragma once

#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QWidget>

#include "model/DeviceModel.h"

class ConfigManager;
class DataManager;
class DeviceManager;
class UiStateStore;
class QHideEvent;
class QShowEvent;
class QTableWidget;
class QPushButton;
class QTimer;

class DeviceConfigPage : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceConfigPage(DeviceManager *device, ConfigManager *config,
                              DataManager *dataManager, UiStateStore *stateStore,
                              QWidget *parent = nullptr);

signals:
    void addSlaveRequested(const QString &gatewayId, const QString &portId, int deviceId,
                           const QString &deviceType, int pollIntervalMs);
    void syncConfigRequested(const QJsonArray &targets);
    void deleteMasterDataRequested(const QString &gatewayId, const QString &portId);
    void deleteDeviceDataRequested(const QString &gatewayId, const QString &portId, int deviceId);

public slots:
    void scheduleRefreshTables();
    void refreshTables();
    void refreshRealtimeColumns();
    void onSyncConfigResult(const QJsonObject &root);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

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
    QString realtimeTemperatureText(const QString &deviceKey) const;
    QString realtimeHumidityText(const QString &deviceKey) const;
    QString realtimeRelayText(const QString &deviceKey) const;
    QString realtimeSourceText(const QString &deviceKey) const;
    void showAddSlaveDialog();
    void showSyncConfigDialog();
    void showScanPlaceholder();

private:
    DeviceManager *m_device = nullptr;
    ConfigManager *m_config = nullptr;
    DataManager *m_dataManager = nullptr;
    UiStateStore *m_stateStore = nullptr;
    QTableWidget *m_masterTable = nullptr;
    QTableWidget *m_slaveTable = nullptr;
    QPushButton *m_syncButton = nullptr;
    QTimer *m_refreshTimer = nullptr;
    QTimer *m_realtimeTimer = nullptr;
    QHash<QString, int> m_slaveRowByDeviceKey;
    bool m_refreshDirty = false;
};
