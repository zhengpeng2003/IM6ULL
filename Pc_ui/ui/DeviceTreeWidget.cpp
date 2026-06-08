#include "DeviceTreeWidget.h"
#include <QHeaderView>

DeviceTreeWidget::DeviceTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setObjectName("DeviceTree");
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int){
        const QString deviceKey = item->data(0, Qt::UserRole).toString();
        if (!deviceKey.isEmpty()) emit deviceSelected(deviceKey);
    });
}

void DeviceTreeWidget::setDevices(const QList<DeviceNode> &devices)
{
    clear();
    QHash<QString, QTreeWidgetItem*> nodeMap;

    for (const auto &dev : devices) {
        const QString factoryKey = dev.factoryId;
        auto *factoryItem = nodeMap.value(factoryKey, nullptr);
        if (!factoryItem) {
            factoryItem = new QTreeWidgetItem(this, QStringList() << dev.factoryId);
            nodeMap.insert(factoryKey, factoryItem);
        }

        const QString areaKey = factoryKey + "/" + dev.areaId;
        auto *areaItem = nodeMap.value(areaKey, nullptr);
        if (!areaItem) {
            areaItem = new QTreeWidgetItem(factoryItem, QStringList() << dev.areaName);
            nodeMap.insert(areaKey, areaItem);
        }

        const QString gwKey = areaKey + "/" + dev.gatewayId;
        auto *gwItem = nodeMap.value(gwKey, nullptr);
        if (!gwItem) {
            gwItem = new QTreeWidgetItem(areaItem, QStringList() << dev.gatewayId);
            nodeMap.insert(gwKey, gwItem);
        }

        const QString masterKey = gwKey + "/" + QString::number(dev.masterSlot);
        auto *masterItem = nodeMap.value(masterKey, nullptr);
        if (!masterItem) {
            masterItem = new QTreeWidgetItem(gwItem, QStringList() << QString("RS485-%1 %2").arg(dev.masterSlot + 1).arg(dev.masterName));
            nodeMap.insert(masterKey, masterItem);
        }

        auto *slaveItem = new QTreeWidgetItem(masterItem, QStringList() << QString("从站%1 %2").arg(dev.slaveAddr).arg(dev.deviceName));
        slaveItem->setData(0, Qt::UserRole, dev.key());
    }
    expandAll();
}

QTreeWidgetItem *DeviceTreeWidget::ensureChild(QTreeWidgetItem *parent, const QString &text, const QString &key)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        auto *child = parent->child(i);
        if (child->data(0, Qt::UserRole + 1).toString() == key) return child;
    }
    auto *item = new QTreeWidgetItem(parent, QStringList() << text);
    item->setData(0, Qt::UserRole + 1, key);
    return item;
}
