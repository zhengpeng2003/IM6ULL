#include "pageinfo.h"

#include <QHBoxLayout>
#include <QStyle>
#include <QVBoxLayout>

Pageinfo::Pageinfo(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void Pageinfo::initUI()
{
    setObjectName("PageArea");

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    hintLabel = new QLabel("System Info", this);
    hintLabel->setObjectName("SectionTitle");

    ipcStatusLabel = new QLabel(this);
    ipcStatusLabel->setObjectName("InfoLine");

    reconnectButton = new QPushButton("Reconnect", this);
    reconnectButton->setObjectName("ActionButton");

    kernelLabel = new QLabel("Kernel: waiting backend info", this);
    archLabel = new QLabel("Arch: waiting backend info", this);
    osLabel = new QLabel("OS: waiting backend info", this);
    screenLabel = new QLabel("Screen: waiting backend info", this);

    kernelLabel->setObjectName("InfoLine");
    archLabel->setObjectName("InfoLine");
    osLabel->setObjectName("InfoLine");
    screenLabel->setObjectName("InfoLine");

    QHBoxLayout *ipcLayout = new QHBoxLayout();
    ipcLayout->setContentsMargins(0, 0, 0, 0);
    ipcLayout->setSpacing(8);
    ipcLayout->addWidget(ipcStatusLabel, 1);
    ipcLayout->addWidget(reconnectButton);

    layout->addWidget(hintLabel);
    layout->addLayout(ipcLayout);
    layout->addWidget(kernelLabel);
    layout->addWidget(archLabel);
    layout->addWidget(osLabel);
    layout->addWidget(screenLabel);
    layout->addStretch();

    updateIpcStatusLabel();

    connect(reconnectButton, &QPushButton::clicked,
            this, &Pageinfo::reconnectIpcRequested);
}

void Pageinfo::addInfo(const DataPack &pack)
{
    for (const auto &dev : pack.devices) {
        if (dev.type != DEV_SYSINFO || !dev.valid)
            continue;

        const auto &sys = dev.sys;
        kernelLabel->setText(QString("Kernel: %1").arg(sys.kernel));
        archLabel->setText(QString("Arch: %1").arg(sys.arch));
        osLabel->setText(QString("OS: %1").arg(sys.os));
        screenLabel->setText(QString("Screen: %1 x %2").arg(sys.screenW).arg(sys.screenH));
    }
}

void Pageinfo::setIpcConnected(bool connected)
{
    if (m_ipcConnected == connected)
        return;

    m_ipcConnected = connected;
    updateIpcStatusLabel();
}

void Pageinfo::updateIpcStatusLabel()
{
    if (!ipcStatusLabel || !reconnectButton)
        return;

    ipcStatusLabel->setText(m_ipcConnected
        ? "IPC: connected"
        : "IPC: disconnected");
    ipcStatusLabel->setProperty("state", m_ipcConnected ? "online" : "offline");
    ipcStatusLabel->style()->unpolish(ipcStatusLabel);
    ipcStatusLabel->style()->polish(ipcStatusLabel);
    reconnectButton->setEnabled(!m_ipcConnected);
}
