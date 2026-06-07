#include "slavelistdialog.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

SlaveListDialog::SlaveListDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName("SlavePopup");
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setModal(false);
    setFixedSize(360, 220);

    titleLabel = new QLabel(this);
    titleLabel->setObjectName("PopupTitle");

    QPushButton *closeButton = new QPushButton("X", this);
    closeButton->setObjectName("PopupCloseButton");
    closeButton->setFixedSize(24, 22);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    titleLayout->addWidget(closeButton);

    QWidget *content = new QWidget(this);
    content->setObjectName("PopupListContent");
    listLayout = new QVBoxLayout(content);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(4);

    emptyLabel = new QLabel("No slave devices", content);
    emptyLabel->setObjectName("HintText");
    emptyLabel->setAlignment(Qt::AlignCenter);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("PopupScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(content);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 7, 8, 8);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(titleLayout);
    mainLayout->addWidget(scrollArea, 1);
}

void SlaveListDialog::setSlaveList(const QList<SlaveDeviceInfo> &slaves,
                                   const QMap<QString, SlaveRuntimeInfo> &runtime,
                                   const QString &masterName)
{
    currentSlaves = slaves;
    currentRuntime = runtime;
    currentMasterName = masterName;
    titleLabel->setText(QString("Slave List (%1)").arg(currentSlaves.size()));
    rebuildRows();
}

bool SlaveListDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        for (const Row &row : rows) {
            if (watched == row.frame) {
                emit slaveActivated(row.index);
                return true;
            }
        }
    }

    return QDialog::eventFilter(watched, event);
}

QString SlaveListDialog::runtimeKey(const SlaveDeviceInfo &slave) const
{
    return QString("%1:%2:%3")
        .arg(slave.masterSlot)
        .arg(slave.slaveAddr)
        .arg(slave.deviceType);
}

QFrame *SlaveListDialog::createRow(int index)
{
    QFrame *frame = new QFrame(this);
    frame->setObjectName("SlavePopupRow");
    frame->setFixedHeight(42);
    frame->setCursor(Qt::PointingHandCursor);
    frame->installEventFilter(this);

    QLabel *title = new QLabel(frame);
    title->setObjectName("SlaveTitle");
    title->setAttribute(Qt::WA_TransparentForMouseEvents);

    QLabel *meta = new QLabel(frame);
    meta->setObjectName("DetailKey");
    meta->setAttribute(Qt::WA_TransparentForMouseEvents);

    QLabel *state = new QLabel(frame);
    state->setObjectName("SlaveState");
    state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    state->setAttribute(Qt::WA_TransparentForMouseEvents);

    QVBoxLayout *textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);
    textLayout->addWidget(title);
    textLayout->addWidget(meta);

    QHBoxLayout *rowLayout = new QHBoxLayout(frame);
    rowLayout->setContentsMargins(7, 4, 7, 4);
    rowLayout->setSpacing(6);
    rowLayout->addLayout(textLayout, 1);
    rowLayout->addWidget(state);

    Row row;
    row.frame = frame;
    row.title = title;
    row.meta = meta;
    row.state = state;
    row.index = index;
    rows.append(row);

    return frame;
}

void SlaveListDialog::updateRow(Row &row,
                                const SlaveDeviceInfo &slave,
                                const SlaveRuntimeInfo &runtime)
{
    const QString name = slave.displayName.isEmpty() ? slave.deviceType : slave.displayName;
    row.title->setText(QString("[%1] %2").arg(slave.slaveAddr).arg(name));
    row.meta->setText(QString("%1  %2").arg(currentMasterName).arg(slave.deviceName));
    row.state->setText(runtime.online ? "Online" : "Offline");
    row.state->setProperty("state", runtime.online ? "online" : "offline");
    row.state->style()->unpolish(row.state);
    row.state->style()->polish(row.state);
}

void SlaveListDialog::rebuildRows()
{
    while (listLayout->count() > 0) {
        QLayoutItem *item = listLayout->takeAt(0);
        if (QWidget *widget = item->widget()) {
            if (widget != emptyLabel)
                widget->deleteLater();
        }
        delete item;
    }
    rows.clear();

    emptyLabel->setVisible(currentSlaves.isEmpty());
    if (currentSlaves.isEmpty()) {
        listLayout->addWidget(emptyLabel, 1);
        return;
    }

    for (int i = 0; i < currentSlaves.size(); ++i) {
        QFrame *rowFrame = createRow(i);
        listLayout->addWidget(rowFrame);
        SlaveRuntimeInfo runtime = currentRuntime.value(runtimeKey(currentSlaves.at(i)));
        runtime.online = currentSlaves.at(i).online || runtime.online;
        updateRow(rows.last(), currentSlaves.at(i), runtime);
    }
    listLayout->addStretch();
}
