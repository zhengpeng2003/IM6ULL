#pragma once

#include <QDialog>
#include <QVector>

#include "../pages/PageStatus.h"

class QLabel;
class QEvent;
class QFrame;
class QVBoxLayout;

class SlaveListDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SlaveListDialog(QWidget *parent = nullptr);

    void setSlaveList(const QList<SlaveDeviceInfo> &slaves,
                      const QMap<QString, SlaveRuntimeInfo> &runtime,
                      const QString &masterName);

signals:
    void slaveActivated(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Row {
        QFrame *frame = nullptr;
        QLabel *title = nullptr;
        QLabel *meta = nullptr;
        QLabel *state = nullptr;
        int index = -1;
    };

    QString runtimeKey(const SlaveDeviceInfo &slave) const;
    QFrame *createRow(int index);
    void updateRow(Row &row, const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime);
    void rebuildRows();

    QLabel *titleLabel = nullptr;
    QLabel *emptyLabel = nullptr;
    QVBoxLayout *listLayout = nullptr;
    QList<SlaveDeviceInfo> currentSlaves;
    QMap<QString, SlaveRuntimeInfo> currentRuntime;
    QString currentMasterName;
    QVector<Row> rows;
};
