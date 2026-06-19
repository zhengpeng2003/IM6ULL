#pragma once

#include <QAbstractTableModel>
#include <QList>

#include "core/UiStateStore.h"

class DeviceStateListModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column {
        GatewayColumn = 0,
        PortColumn,
        DeviceIdColumn,
        DeviceNameColumn,
        DeviceTypeColumn,
        LifecycleColumn,
        LastUpdateColumn,
        ColumnCount
    };

    explicit DeviceStateListModel(UiStateStore *stateStore, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    void refreshFromStore();

private:
    UiStateStore *m_stateStore = nullptr;
    QList<DeviceState> m_rows;
};
