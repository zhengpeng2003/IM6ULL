#pragma once
#include <QTreeWidget>
#include <QSet>
#include "model/DeviceModel.h"
#include "model/TelemetryModel.h"

class DeviceTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeWidget(QWidget *parent = nullptr);
    void setDevices(const QList<DeviceNode> &devices);
    void setRealtimeDevices(const QList<RealtimeDeviceData> &devices, bool includePoints);

signals:
    void deviceSelected(const QString &deviceKey);
    void pointSelected(const QString &pointId, const QString &deviceKey, const QString &pointName, const QString &unit);

private:
    QSet<QString> expandedNodeKeys() const;
    void restoreExpandedNodeKeys(const QSet<QString> &expandedKeys);
    QTreeWidgetItem *findDeviceItem(const QString &deviceKey) const;
    QTreeWidgetItem *findPointItem(const QString &pointId) const;
    QTreeWidgetItem *ensureChild(QTreeWidgetItem *parent, const QString &text, const QString &key);
};
