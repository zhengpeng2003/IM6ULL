#include "DeviceTreeWidget.h"
#include <QHeaderView>
#include <QHash>
#include <QSet>

namespace {

constexpr int DeviceKeyRole = Qt::UserRole;
constexpr int NodeKeyRole = Qt::UserRole + 1;
constexpr int PointIdRole = Qt::UserRole + 2;
constexpr int PointNameRole = Qt::UserRole + 3;
constexpr int PointUnitRole = Qt::UserRole + 4;

void collectExpandedNodeKeys(QTreeWidgetItem *item, QSet<QString> &expandedKeys)
{
    if (!item) return;

    const QString nodeKey = item->data(0, NodeKeyRole).toString();
    if (item->isExpanded() && !nodeKey.isEmpty()) {
        expandedKeys.insert(nodeKey);
    }

    for (int i = 0; i < item->childCount(); ++i) {
        collectExpandedNodeKeys(item->child(i), expandedKeys);
    }
}

void restoreExpandedNodeKeys(QTreeWidgetItem *item, const QSet<QString> &expandedKeys)
{
    if (!item) return;

    const QString nodeKey = item->data(0, NodeKeyRole).toString();
    item->setExpanded(!nodeKey.isEmpty() && expandedKeys.contains(nodeKey));

    for (int i = 0; i < item->childCount(); ++i) {
        restoreExpandedNodeKeys(item->child(i), expandedKeys);
    }
}

QTreeWidgetItem *findDeviceItem(QTreeWidgetItem *item, const QString &deviceKey)
{
    if (!item) return nullptr;

    if (item->data(0, DeviceKeyRole).toString() == deviceKey) {
        return item;
    }

    for (int i = 0; i < item->childCount(); ++i) {
        if (auto *found = findDeviceItem(item->child(i), deviceKey)) {
            return found;
        }
    }

    return nullptr;
}

QTreeWidgetItem *findPointItem(QTreeWidgetItem *item, const QString &pointId)
{
    if (!item) return nullptr;

    if (item->data(0, PointIdRole).toString() == pointId) {
        return item;
    }

    for (int i = 0; i < item->childCount(); ++i) {
        if (auto *found = findPointItem(item->child(i), pointId)) {
            return found;
        }
    }

    return nullptr;
}

} // namespace

DeviceTreeWidget::DeviceTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
    setHeaderHidden(true);
    setObjectName("DeviceTree");
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int){
        const QString pointId = item->data(0, PointIdRole).toString();
        if (!pointId.isEmpty()) {
            emit pointSelected(pointId,
                               item->data(0, DeviceKeyRole).toString(),
                               item->data(0, PointNameRole).toString(),
                               item->data(0, PointUnitRole).toString());
            return;
        }

        const QString deviceKey = item->data(0, DeviceKeyRole).toString();
        if (!deviceKey.isEmpty()) emit deviceSelected(deviceKey);
    });
}

void DeviceTreeWidget::setDevices(const QList<DeviceNode> &devices)
{
    const QSet<QString> expandedKeys = expandedNodeKeys();
    const QString selectedDeviceKey = currentItem()
        ? currentItem()->data(0, DeviceKeyRole).toString()
        : QString();

    clear();
    QHash<QString, QTreeWidgetItem*> nodeMap;

    for (const auto &dev : devices) {
        const QString factoryKey = dev.factoryId;
        auto *factoryItem = nodeMap.value(factoryKey, nullptr);
        if (!factoryItem) {
            factoryItem = new QTreeWidgetItem(this, QStringList() << dev.factoryId);
            factoryItem->setData(0, NodeKeyRole, factoryKey);
            nodeMap.insert(factoryKey, factoryItem);
        }

        const QString areaKey = factoryKey + "/" + dev.areaId;
        auto *areaItem = nodeMap.value(areaKey, nullptr);
        if (!areaItem) {
            areaItem = new QTreeWidgetItem(factoryItem, QStringList() << dev.areaName);
            areaItem->setData(0, NodeKeyRole, areaKey);
            nodeMap.insert(areaKey, areaItem);
        }

        const QString gwKey = areaKey + "/" + dev.gatewayId;
        auto *gwItem = nodeMap.value(gwKey, nullptr);
        if (!gwItem) {
            gwItem = new QTreeWidgetItem(areaItem, QStringList() << dev.gatewayId);
            gwItem->setData(0, NodeKeyRole, gwKey);
            nodeMap.insert(gwKey, gwItem);
        }

        const QString masterKey = gwKey + "/" + QString::number(dev.masterSlot);
        auto *masterItem = nodeMap.value(masterKey, nullptr);
        if (!masterItem) {
            masterItem = new QTreeWidgetItem(gwItem, QStringList() << QString("RS485-%1 %2").arg(dev.masterSlot + 1).arg(dev.masterName));
            masterItem->setData(0, NodeKeyRole, masterKey);
            nodeMap.insert(masterKey, masterItem);
        }

        auto *slaveItem = new QTreeWidgetItem(masterItem, QStringList() << QString("从站%1 %2").arg(dev.slaveAddr).arg(dev.deviceName));
        const QString slaveKey = dev.key();
        slaveItem->setData(0, DeviceKeyRole, slaveKey);
        slaveItem->setData(0, NodeKeyRole, slaveKey);
    }

    restoreExpandedNodeKeys(expandedKeys);
    if (!selectedDeviceKey.isEmpty()) {
        QTreeWidgetItem *selectedItem = findDeviceItem(selectedDeviceKey);
        setCurrentItem(selectedItem);
        if (!selectedItem) {
            emit selectionLost();
        }
    }
}

void DeviceTreeWidget::setRealtimeDevices(const QList<RealtimeDeviceData> &devices, bool includePoints)
{
    const QString selectedPointId = currentItem()
        ? currentItem()->data(0, PointIdRole).toString()
        : QString();
    const QString selectedDeviceKey = currentItem()
        ? currentItem()->data(0, DeviceKeyRole).toString()
        : QString();

    QList<DeviceNode> nodes;
    nodes.reserve(devices.size());
    for (const RealtimeDeviceData &device : devices) {
        nodes.append(device.node);
    }

    setDevices(nodes);

    if (!includePoints) {
        return;
    }

    for (const RealtimeDeviceData &device : devices) {
        QTreeWidgetItem *deviceItem = findDeviceItem(device.node.key());
        if (!deviceItem) {
            continue;
        }

        for (const TelemetryPointData &point : device.points) {
            if (point.pointId.isEmpty() || point.valueType == "text") {
                continue;
            }

            QString pointName = point.pointName.isEmpty() ? point.pointKey : point.pointName;
            if (pointName.isEmpty()) {
                pointName = point.pointId;
            }

            QString label = pointName;
            if (!point.unit.isEmpty()) {
                label += QStringLiteral(" (%1)").arg(point.unit);
            }

            auto *pointItem = new QTreeWidgetItem(deviceItem, QStringList() << label);
            pointItem->setData(0, DeviceKeyRole, device.node.key());
            pointItem->setData(0, NodeKeyRole, device.node.key() + "/point/" + point.pointId);
            pointItem->setData(0, PointIdRole, point.pointId);
            pointItem->setData(0, PointNameRole, pointName);
            pointItem->setData(0, PointUnitRole, point.unit);
        }
    }

    if (!selectedPointId.isEmpty()) {
        if (QTreeWidgetItem *pointItem = findPointItem(selectedPointId)) {
            setCurrentItem(pointItem);
            return;
        }
        emit selectionLost();
        return;
    }

    if (!selectedDeviceKey.isEmpty()) {
        QTreeWidgetItem *selectedItem = findDeviceItem(selectedDeviceKey);
        setCurrentItem(selectedItem);
        if (!selectedItem) {
            emit selectionLost();
        }
    }
}

QSet<QString> DeviceTreeWidget::expandedNodeKeys() const
{
    QSet<QString> expandedKeys;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        collectExpandedNodeKeys(topLevelItem(i), expandedKeys);
    }
    return expandedKeys;
}

void DeviceTreeWidget::restoreExpandedNodeKeys(const QSet<QString> &expandedKeys)
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        ::restoreExpandedNodeKeys(topLevelItem(i), expandedKeys);
    }
}

QTreeWidgetItem *DeviceTreeWidget::findDeviceItem(const QString &deviceKey) const
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (auto *found = ::findDeviceItem(topLevelItem(i), deviceKey)) {
            return found;
        }
    }
    return nullptr;
}

QTreeWidgetItem *DeviceTreeWidget::findPointItem(const QString &pointId) const
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        if (auto *found = ::findPointItem(topLevelItem(i), pointId)) {
            return found;
        }
    }
    return nullptr;
}

QTreeWidgetItem *DeviceTreeWidget::ensureChild(QTreeWidgetItem *parent, const QString &text, const QString &key)
{
    for (int i = 0; i < parent->childCount(); ++i) {
        auto *child = parent->child(i);
        if (child->data(0, NodeKeyRole).toString() == key) return child;
    }
    auto *item = new QTreeWidgetItem(parent, QStringList() << text);
    item->setData(0, NodeKeyRole, key);
    return item;
}
