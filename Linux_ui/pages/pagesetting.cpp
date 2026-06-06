#include "pagesetting.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

PageSetting::PageSetting(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();
    setUnscannedState();
}

void PageSetting::setUnscannedState()
{
    scanButton->setText("Scan");
    statusLabel->setText("State: not scanned");
    emptyWidget->setVisible(true);
    cardsWidget->setVisible(false);
    clearPortCards();
}

void PageSetting::setPortList(const QList<MasterPortInfo> &ports)
{
    scanButton->setText("Rescan");
    statusLabel->setText(QString("Ports found: %1").arg(ports.size()));
    emptyWidget->setVisible(ports.isEmpty());
    cardsWidget->setVisible(!ports.isEmpty());
    clearPortCards();

    for (const MasterPortInfo &info : ports)
        cardsLayout->addWidget(createPortCard(info));
    cardsLayout->addStretch();
}

void PageSetting::updateMasterConnectionState(int masterSlot, bool connected)
{
    for (PortCard *card : portCards) {
        if (card->info.masterSlot != masterSlot)
            continue;

        card->info.connected = connected;
        updateCardState(*card, connected);
        return;
    }
}

void PageSetting::initUI()
{
    QLabel *titleLabel = new QLabel("RS485 Port Setting", this);
    titleLabel->setObjectName("PanelTitle");

    scanButton = new QPushButton("Scan", this);
    scanButton->setObjectName("ActionButton");
    scanButton->setFixedWidth(78);

    QHBoxLayout *titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);
    titleRow->addWidget(titleLabel);
    titleRow->addStretch();
    titleRow->addWidget(scanButton);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("HintText");
    statusLabel->setFixedHeight(18);

    emptyWidget = new QWidget(this);
    emptyWidget->setObjectName("EmptyPanel");

    emptyTitleLabel = new QLabel("No RS485 ports", emptyWidget);
    emptyTitleLabel->setObjectName("EmptyTitle");
    emptyTitleLabel->setAlignment(Qt::AlignCenter);

    emptyHintLabel = new QLabel("Tap Scan, then update with setPortList().", emptyWidget);
    emptyHintLabel->setObjectName("HintText");
    emptyHintLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout *emptyLayout = new QVBoxLayout(emptyWidget);
    emptyLayout->setContentsMargins(0, 28, 0, 0);
    emptyLayout->setSpacing(8);
    emptyLayout->addWidget(emptyTitleLabel);
    emptyLayout->addWidget(emptyHintLabel);
    emptyLayout->addStretch();

    cardsWidget = new QWidget(this);
    cardsLayout = new QVBoxLayout(cardsWidget);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(4);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);
    mainLayout->addLayout(titleRow);
    mainLayout->addWidget(statusLabel);
    mainLayout->addWidget(emptyWidget, 1);
    mainLayout->addWidget(cardsWidget, 1);

    connect(scanButton, &QPushButton::clicked, this, [this]() {
        emit scanPortsRequested();
    });
}

void PageSetting::clearPortCards()
{
    while (QLayoutItem *item = cardsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget())
            widget->deleteLater();
        delete item;
    }
    qDeleteAll(portCards);
    portCards.clear();
}

QFrame *PageSetting::createPortCard(const MasterPortInfo &info)
{
    QFrame *cardFrame = new QFrame(this);
    cardFrame->setObjectName("PortCard");
    cardFrame->setFixedHeight(72);

    QLabel *nameLabel = new QLabel(info.masterName, cardFrame);
    nameLabel->setObjectName("SectionTitle");

    QLabel *nodeLabel = new QLabel(QString("Device: %1").arg(info.deviceNode), cardFrame);
    nodeLabel->setObjectName("HintText");

    QLabel *areaLabel = new QLabel("Area:", cardFrame);
    QLineEdit *areaEdit = new QLineEdit(info.areaName, cardFrame);
    areaEdit->setObjectName("CompactLineEdit");
    areaEdit->setFixedWidth(116);

    QLabel *baudLabel = new QLabel("Baud:", cardFrame);
    QComboBox *baudCombo = new QComboBox(cardFrame);
    baudCombo->setObjectName("CompactCombo");
    baudCombo->addItems({"9600", "19200", "38400", "115200"});
    baudCombo->setCurrentText(QString::number(info.baudRate));
    baudCombo->setFixedWidth(76);

    QLabel *stateLabel = new QLabel(cardFrame);
    QPushButton *actionButton = new QPushButton(cardFrame);
    actionButton->setFixedWidth(54);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(4);
    topRow->addWidget(nameLabel);
    topRow->addStretch();
    topRow->addWidget(stateLabel);
    topRow->addWidget(actionButton);

    QHBoxLayout *areaRow = new QHBoxLayout;
    areaRow->setContentsMargins(0, 0, 0, 0);
    areaRow->setSpacing(4);
    areaRow->addWidget(areaLabel);
    areaRow->addWidget(areaEdit);
    areaRow->addSpacing(6);
    areaRow->addWidget(baudLabel);
    areaRow->addWidget(baudCombo);
    areaRow->addStretch();

    QVBoxLayout *cardLayout = new QVBoxLayout(cardFrame);
    cardLayout->setContentsMargins(6, 4, 6, 4);
    cardLayout->setSpacing(2);
    cardLayout->addLayout(topRow);
    cardLayout->addWidget(nodeLabel);
    cardLayout->addLayout(areaRow);

    PortCard *card = new PortCard;
    card->frame = cardFrame;
    card->stateLabel = stateLabel;
    card->actionButton = actionButton;
    card->areaEdit = areaEdit;
    card->baudCombo = baudCombo;
    card->info = info;
    portCards.append(card);

    updateCardState(*card, info.connected);
    connect(actionButton, &QPushButton::clicked, this, [this, card]() {
        if (card->info.connected) {
            emit disconnectMasterRequested(card->info.masterSlot, card->info.deviceNode);
        } else {
            emit connectMasterRequested(card->info.masterSlot,
                                        card->info.deviceNode,
                                        card->areaEdit->text(),
                                        card->baudCombo->currentText().toInt());
        }
    });

    return cardFrame;
}

void PageSetting::updateCardState(PortCard &card, bool connected)
{
    card.stateLabel->setText(connected ? "State: connected" : "State: disconnected");
    card.stateLabel->setProperty("state", connected ? "online" : "offline");
    card.actionButton->setText(connected ? "Stop" : "Connect");
    card.actionButton->setObjectName(connected ? "DangerButton" : "ActionButton");
    card.areaEdit->setEnabled(!connected);
    card.baudCombo->setEnabled(!connected);

    card.stateLabel->style()->unpolish(card.stateLabel);
    card.stateLabel->style()->polish(card.stateLabel);
    card.actionButton->style()->unpolish(card.actionButton);
    card.actionButton->style()->polish(card.actionButton);
}
