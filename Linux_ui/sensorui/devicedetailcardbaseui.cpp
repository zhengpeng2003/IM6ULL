// ============================
// sensor_ui/devicedetailcardbaseui.cpp
// ============================

#include "devicedetailcardbaseui.h"

#include <QHBoxLayout>
#include <QStyle>

DeviceDetailCardBaseUi::DeviceDetailCardBaseUi(QWidget *parent)
    : QFrame(parent)
{
    setObjectName("DetailCard");

    detailTitleLabel = new QLabel("--", this);
    detailTitleLabel->setObjectName("DetailTitle");

    detailStateLabel = new QLabel("离线", this);
    detailStateLabel->setObjectName("SlaveState");

    removeSlaveButton = new QPushButton("移除", this);
    removeSlaveButton->setObjectName("SmallGhostButton");
    removeSlaveButton->setFixedWidth(58);

    QHBoxLayout *titleLayout = new QHBoxLayout;
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(6);
    titleLayout->addWidget(detailTitleLabel, 1);
    titleLayout->addWidget(detailStateLabel);
    titleLayout->addWidget(removeSlaveButton);

    detailPortLabel = new QLabel("端口：--", this);
    detailAddrLabel = new QLabel("地址：--", this);
    detailTypeLabel = new QLabel("类型：--", this);
    detailPortLabel->setObjectName("DetailKey");
    detailAddrLabel->setObjectName("DetailKey");
    detailTypeLabel->setObjectName("DetailKey");

    QHBoxLayout *baseInfoLayout = new QHBoxLayout;
    baseInfoLayout->setContentsMargins(0, 0, 0, 0);
    baseInfoLayout->setSpacing(10);
    baseInfoLayout->addWidget(detailPortLabel);
    baseInfoLayout->addWidget(detailAddrLabel);
    baseInfoLayout->addWidget(detailTypeLabel);
    baseInfoLayout->addStretch();

    metricPanel = new QFrame(this);
    metricPanel->setObjectName("MetricPanel");
    metricGrid = new QGridLayout(metricPanel);
    metricGrid->setContentsMargins(0, 0, 0, 0);
    metricGrid->setSpacing(6);

    metricA = createMetricCard("A", "--");
    metricB = createMetricCard("B", "--");
    metricC = createMetricCard("C", "--");
    metricD = createMetricCard("D", "--");

    metricGrid->addWidget(metricA.frame, 0, 0);
    metricGrid->addWidget(metricB.frame, 0, 1);
    metricGrid->addWidget(metricC.frame, 1, 0);
    metricGrid->addWidget(metricD.frame, 1, 1);

    pollIntervalLabel = new QLabel("轮询：--", this);
    lastUpdateLabel = new QLabel("更新时间：--", this);
    pollIntervalLabel->setObjectName("DetailKey");
    lastUpdateLabel->setObjectName("DetailKey");

    QHBoxLayout *footerLayout = new QHBoxLayout;
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(8);
    footerLayout->addWidget(pollIntervalLabel);
    footerLayout->addWidget(lastUpdateLabel, 1);

    extraAreaLayout = new QVBoxLayout;
    extraAreaLayout->setContentsMargins(0, 0, 0, 0);
    extraAreaLayout->setSpacing(4);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(6);
    mainLayout->addLayout(titleLayout);
    mainLayout->addLayout(baseInfoLayout);
    mainLayout->addWidget(metricPanel, 1);
    mainLayout->addLayout(extraAreaLayout);
    mainLayout->addLayout(footerLayout);

    connect(removeSlaveButton, &QPushButton::clicked, this, [this]() {
        emit removeSlaveRequested(currentMasterSlot,
                                  currentSlaveAddr,
                                  currentDeviceType);
    });
}

void DeviceDetailCardBaseUi::setBaseInfo(const QString &portName,
                                         int masterSlot,
                                         int slaveAddr,
                                         const QString &typeName,
                                         const QString &deviceType,
                                         bool online)
{
    currentMasterSlot = masterSlot;
    currentSlaveAddr = slaveAddr;
    currentDeviceType = deviceType;

    detailTitleLabel->setText(typeName.isEmpty() ? deviceType : typeName);
    detailPortLabel->setText(QString("端口：%1").arg(portName.isEmpty() ? QString("--") : portName));
    detailAddrLabel->setText(QString("地址：%1").arg(slaveAddr > 0 ? QString::number(slaveAddr) : "--"));
    detailTypeLabel->setText(QString("类型：%1").arg(deviceType.isEmpty() ? QString("--") : deviceType));
    setOnline(online);
}

void DeviceDetailCardBaseUi::setPollInterval(int pollIntervalMs)
{
    pollIntervalLabel->setText(pollIntervalMs > 0
                                   ? QString("轮询：%1 ms").arg(pollIntervalMs)
                                   : "轮询：--");
}

void DeviceDetailCardBaseUi::setLastUpdateTime(const QString &updateTime)
{
    lastUpdateLabel->setText(QString("更新时间：%1").arg(updateTime.isEmpty() ? "--" : updateTime));
}

void DeviceDetailCardBaseUi::setOnline(bool online)
{
    detailStateLabel->setText(online ? "在线" : "离线");
    detailStateLabel->setProperty("state", online ? "online" : "offline");
    detailStateLabel->style()->unpolish(detailStateLabel);
    detailStateLabel->style()->polish(detailStateLabel);
}

DeviceDetailCardBaseUi::MetricCard DeviceDetailCardBaseUi::createMetricCard(const QString &iconText,
                                                                            const QString &name)
{
    MetricCard card;
    card.frame = new QFrame(metricPanel);
    card.frame->setObjectName("MetricCard");
    card.icon = new QLabel(iconText, card.frame);
    card.name = new QLabel(name, card.frame);
    card.value = new QLabel("--", card.frame);
    card.unit = new QLabel(QString(), card.frame);
    card.icon->setObjectName("MetricIcon");
    card.name->setObjectName("DetailKey");
    card.value->setObjectName("MetricValue");
    card.unit->setObjectName("DetailKey");

    QHBoxLayout *layout = new QHBoxLayout(card.frame);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(4);
    layout->addWidget(card.icon);
    layout->addWidget(card.name);
    layout->addStretch();
    layout->addWidget(card.value);
    layout->addWidget(card.unit);
    return card;
}

void DeviceDetailCardBaseUi::setMetricCard(MetricCard &card,
                                           const QString &iconText,
                                           const QString &name,
                                           const QString &value,
                                           const QString &unit)
{
    card.icon->setText(iconText);
    card.name->setText(name);
    card.value->setText(value);
    card.unit->setText(unit);
}

void DeviceDetailCardBaseUi::setMetricVisible(bool a, bool b, bool c, bool d)
{
    metricA.frame->setVisible(a);
    metricB.frame->setVisible(b);
    metricC.frame->setVisible(c);
    metricD.frame->setVisible(d);
}

QVBoxLayout *DeviceDetailCardBaseUi::extraLayout()
{
    return extraAreaLayout;
}
