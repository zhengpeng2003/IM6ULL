#include "CommandTaskPanel.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

CommandTaskPanel::CommandTaskPanel(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("CommandTaskPanel"));
    setWindowTitle(QStringLiteral("命令任务"));
    setAttribute(Qt::WA_StyledBackground, true);
    setWindowFlags(Qt::Tool);
    resize(820, 360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(10);

    auto *header = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("命令任务"), this);
    title->setObjectName(QStringLiteral("CommandTaskTitle"));
    m_countLabel = new QLabel(QStringLiteral("0 个进行中"), this);
    m_countLabel->setObjectName(QStringLiteral("CommandTaskCountLabel"));
    m_statusLabel = new QLabel(QStringLiteral("暂无命令任务"), this);
    m_statusLabel->setObjectName(QStringLiteral("CommandTaskStatusLabel"));
    m_clearButton = new QPushButton(QStringLiteral("清除已完成"), this);
    m_clearButton->setObjectName(QStringLiteral("CommandTaskClearButton"));
    m_closeButton = new QPushButton(QStringLiteral("关闭"), this);
    m_closeButton->setObjectName(QStringLiteral("CommandTaskCloseButton"));

    header->addWidget(title);
    header->addWidget(m_countLabel);
    header->addWidget(m_statusLabel);
    header->addStretch();
    header->addWidget(m_clearButton);
    header->addWidget(m_closeButton);
    layout->addLayout(header);

    m_table = new QTableWidget(this);
    m_table->setObjectName(QStringLiteral("CommandTaskTable"));
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("操作"),
        QStringLiteral("目标"),
        QStringLiteral("状态"),
        QStringLiteral("结果/原因")
    });
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ColumnResult, QHeaderView::Stretch);
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    connect(m_clearButton, &QPushButton::clicked, this, &CommandTaskPanel::clearFinishedTasks);
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::hide);
    updateSummary();
}

void CommandTaskPanel::upsertCommandTask(const QString &cmdId,
                                         const QString &commandType,
                                         const QString &gatewayId,
                                         const QString &portId,
                                         int deviceId,
                                         const QString &state,
                                         const QString &reason,
                                         const QString &message)
{
    if (cmdId.isEmpty()) {
        return;
    }

    int row = m_rowByCmdId.value(cmdId, -1);
    if (row < 0 || row >= m_table->rowCount()) {
        row = m_table->rowCount();
        m_table->insertRow(row);
        m_rowByCmdId.insert(cmdId, row);
        setReadonlyItem(row, ColumnTime, QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        if (QTableWidgetItem *timeItem = m_table->item(row, ColumnTime)) {
            timeItem->setData(Qt::UserRole, cmdId);
        }
    }

    const QString result = reason.isEmpty() ? message : reason;
    m_statusLabel->setText(QStringLiteral("%1：%2").arg(operationText(commandType), state));
    setReadonlyItem(row, ColumnOperation, operationText(commandType));
    setReadonlyItem(row, ColumnTarget, targetText(gatewayId, portId, deviceId));
    setReadonlyItem(row, ColumnState, state);
    setReadonlyItem(row, ColumnResult, result);

    if (QTableWidgetItem *stateItem = m_table->item(row, ColumnState)) {
        if (state.contains(QStringLiteral("成功"))) {
            stateItem->setForeground(QColor(QStringLiteral("#16A34A")));
        } else if (state.contains(QStringLiteral("失败")) || state.contains(QStringLiteral("超时"))) {
            stateItem->setForeground(QColor(QStringLiteral("#DC2626")));
        } else if (state.contains(QStringLiteral("等待"))) {
            stateItem->setForeground(QColor(QStringLiteral("#1E5AA8")));
        } else {
            stateItem->setForeground(QColor(QStringLiteral("#234F83")));
        }
    }

    updateSummary();
}

void CommandTaskPanel::clearFinishedTasks()
{
    for (int row = m_table->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *stateItem = m_table->item(row, ColumnState);
        if (!stateItem || !isFinishedState(stateItem->text())) {
            continue;
        }
        m_table->removeRow(row);
    }

    m_rowByCmdId.clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *timeItem = m_table->item(row, ColumnTime);
        const QString cmdId = timeItem ? timeItem->data(Qt::UserRole).toString() : QString();
        if (!cmdId.isEmpty()) {
            m_rowByCmdId.insert(cmdId, row);
        }
    }
    updateSummary();
}

void CommandTaskPanel::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

bool CommandTaskPanel::isFinishedState(const QString &state) const
{
    return state.contains(QStringLiteral("成功")) ||
           state.contains(QStringLiteral("失败"));
}

QString CommandTaskPanel::operationText(const QString &commandType) const
{
    if (commandType == QStringLiteral("add_device")) {
        return QStringLiteral("添加从站");
    }
    if (commandType == QStringLiteral("remove_device")) {
        return QStringLiteral("删除从站");
    }
    return commandType.isEmpty() ? QStringLiteral("-") : commandType;
}

QString CommandTaskPanel::targetText(const QString &gatewayId, const QString &portId, int deviceId) const
{
    return QStringLiteral("%1 / %2 / 从站 %3")
        .arg(gatewayId.isEmpty() ? QStringLiteral("-") : gatewayId,
             portId.isEmpty() ? QStringLiteral("-") : portId)
        .arg(deviceId);
}

void CommandTaskPanel::setReadonlyItem(int row, int column, const QString &text)
{
    QTableWidgetItem *item = m_table->item(row, column);
    if (!item) {
        item = new QTableWidgetItem;
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, column, item);
    }
    item->setText(text.isEmpty() ? QStringLiteral("-") : text);
}

void CommandTaskPanel::updateSummary()
{
    int running = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *stateItem = m_table->item(row, ColumnState);
        if (!stateItem || !isFinishedState(stateItem->text())) {
            ++running;
        }
    }
    m_countLabel->setText(QStringLiteral("%1 个进行中").arg(running));
    emit runningTaskCountChanged(running);
}
