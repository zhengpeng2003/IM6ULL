// ============================
// sensor_ui/relaycontroldialog.cpp
// ============================

#include "relaycontroldialog.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

RelayControlDialog::RelayControlDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("继电器控制");
    setModal(false);
    resize(360, 260);

    titleLabel = new QLabel("继电器控制", this);
    titleLabel->setObjectName("DetailTitle");

    metaLabel = new QLabel("--", this);
    metaLabel->setObjectName("DetailValue");

    contentWidget = new QWidget(this);
    contentWidget->setObjectName("RelayControlContent");

    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("SlaveListScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setWidget(contentWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(6);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(metaLabel);
    mainLayout->addWidget(scrollArea, 1);
}

void RelayControlDialog::setRelayInfo(int masterSlot,
                                      int slaveAddr,
                                      const QString &portName,
                                      const QVector<RelayChannelInfo> &channels)
{
    currentMasterSlot = masterSlot;
    currentSlaveAddr = slaveAddr;
    currentPortName = portName;
    currentChannels = channels;

    titleLabel->setText(QString("继电器控制（地址：%1）").arg(slaveAddr));

    metaLabel->setText(QString("端口：%1    通道数：%2")
                           .arg(portName.isEmpty() ? "--" : portName)
                           .arg(channels.size()));

    rebuildChannelRows();
}

void RelayControlDialog::rebuildChannelRows()
{
    while (contentLayout->count() > 0) {
        QLayoutItem *item = contentLayout->takeAt(0);

        if (QWidget *widget = item->widget())
            widget->deleteLater();

        delete item;
    }

    if (currentChannels.isEmpty()) {
        QLabel *emptyLabel = new QLabel("暂无继电器通道", contentWidget);
        emptyLabel->setObjectName("HintText");
        emptyLabel->setAlignment(Qt::AlignCenter);
        contentLayout->addWidget(emptyLabel, 1);
        return;
    }

    for (const RelayChannelInfo &channelInfo : currentChannels) {
        QWidget *row = new QWidget(contentWidget);
        row->setObjectName("ControlRow");

        QLabel *nameLabel = new QLabel(channelInfo.name.isEmpty()
                                           ? QString("DO%1").arg(channelInfo.channel)
                                           : channelInfo.name,
                                       row);
        nameLabel->setObjectName("DetailKey");
        nameLabel->setFixedWidth(58);

        QLabel *stateLabel = new QLabel(channelInfo.on ? "当前：开启" : "当前：关闭", row);
        stateLabel->setObjectName("DetailValue");

        QPushButton *onButton = new QPushButton("开", row);
        QPushButton *offButton = new QPushButton("关", row);

        onButton->setObjectName("SmallActionButton");
        offButton->setObjectName("SmallGhostButton");

        onButton->setFixedWidth(34);
        offButton->setFixedWidth(34);

        onButton->setEnabled(channelInfo.enabled);
        offButton->setEnabled(channelInfo.enabled);

        const int channel = channelInfo.channel;

        connect(onButton, &QPushButton::clicked, this, [this, channel]() {
            emit relayCommandRequested(currentMasterSlot,
                                       currentSlaveAddr,
                                       channel,
                                       true);
        });

        connect(offButton, &QPushButton::clicked, this, [this, channel]() {
            emit relayCommandRequested(currentMasterSlot,
                                       currentSlaveAddr,
                                       channel,
                                       false);
        });

        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);
        rowLayout->addWidget(nameLabel);
        rowLayout->addWidget(stateLabel, 1);
        rowLayout->addWidget(onButton);
        rowLayout->addWidget(offButton);

        contentLayout->addWidget(row);
    }

    contentLayout->addStretch();
}