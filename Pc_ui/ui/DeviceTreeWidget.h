#pragma once
#include <QTreeWidget>
#include <QSet>
#include "model/DeviceModel.h"

class DeviceTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    explicit DeviceTreeWidget(QWidget *parent = nullptr);
    void setDevices(const QList<DeviceNode> &devices);

signals:
    void deviceSelected(const QString &deviceKey);

private:
    QSet<QString> expandedNodeKeys() const;
    void restoreExpandedNodeKeys(const QSet<QString> &expandedKeys);
    QTreeWidgetItem *findDeviceItem(const QString &deviceKey) const;
    QTreeWidgetItem *ensureChild(QTreeWidgetItem *parent, const QString &text, const QString &key);
};
