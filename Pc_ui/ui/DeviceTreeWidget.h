#pragma once
#include <QTreeWidget>
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
    QTreeWidgetItem *ensureChild(QTreeWidgetItem *parent, const QString &text, const QString &key);
};
