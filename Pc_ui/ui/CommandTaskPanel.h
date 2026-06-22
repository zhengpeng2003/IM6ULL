#pragma once

#include <QHash>
#include <QWidget>

class QLabel;
class QPushButton;
class QTableWidget;
class QCloseEvent;

class CommandTaskPanel : public QWidget
{
    Q_OBJECT
public:
    explicit CommandTaskPanel(QWidget *parent = nullptr);

public slots:
    void upsertCommandTask(const QString &cmdId,
                           const QString &commandType,
                           const QString &gatewayId,
                           const QString &portId,
                           int deviceId,
                           const QString &state,
                           const QString &reason,
                           const QString &message);
    void clearFinishedTasks();

signals:
    void runningTaskCountChanged(int count);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum Column
    {
        ColumnTime = 0,
        ColumnOperation,
        ColumnTarget,
        ColumnState,
        ColumnResult,
        ColumnCount
    };

    bool isFinishedState(const QString &state) const;
    QString operationText(const QString &commandType) const;
    QString targetText(const QString &gatewayId, const QString &portId, int deviceId) const;
    void setReadonlyItem(int row, int column, const QString &text);
    void updateSummary();

    QLabel *m_countLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QHash<QString, int> m_rowByCmdId;
};
