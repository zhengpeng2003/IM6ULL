#include "DeviceStateListModel.h"

#include <QDateTime>

DeviceStateListModel::DeviceStateListModel(UiStateStore *stateStore, QObject *parent)
    : QAbstractTableModel(parent)
    , m_stateStore(stateStore)
{
    if (m_stateStore) {
        connect(m_stateStore, &UiStateStore::stateChanged,
                this, &DeviceStateListModel::refreshFromStore);
        refreshFromStore();
    }
}

int DeviceStateListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

int DeviceStateListModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DeviceStateListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }

    const DeviceState &state = m_rows.at(index.row());
    const DeviceNode &node = state.node;

    if (role == Qt::UserRole) {
        return node.key();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (index.column()) {
    case GatewayColumn:
        return node.gatewayName.isEmpty() ? node.gatewayId : node.gatewayName;
    case PortColumn:
        return node.port;
    case DeviceIdColumn:
        return node.slaveAddr > 0 ? node.slaveAddr : node.deviceId;
    case DeviceNameColumn:
        return node.deviceName;
    case DeviceTypeColumn:
        return node.deviceType;
    case LifecycleColumn:
        return state.lifecycleState;
    case LastUpdateColumn:
        return node.lastUpdateTime > 0
            ? QDateTime::fromMSecsSinceEpoch(node.lastUpdateTime).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("-");
    default:
        return QVariant();
    }
}

QVariant DeviceStateListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QVariant();
    }

    switch (section) {
    case GatewayColumn:
        return QStringLiteral("网关");
    case PortColumn:
        return QStringLiteral("端口");
    case DeviceIdColumn:
        return QStringLiteral("设备地址");
    case DeviceNameColumn:
        return QStringLiteral("设备名称");
    case DeviceTypeColumn:
        return QStringLiteral("设备类型");
    case LifecycleColumn:
        return QStringLiteral("状态");
    case LastUpdateColumn:
        return QStringLiteral("最后更新");
    default:
        return QVariant();
    }
}

void DeviceStateListModel::refreshFromStore()
{
    beginResetModel();
    m_rows = m_stateStore ? m_stateStore->deviceStates() : QList<DeviceState>();
    endResetModel();
}
