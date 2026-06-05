#include "pageinfo.h"

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

    hintLabel = new QLabel("系统信息", this);
    hintLabel->setObjectName("SectionTitle");

    kernelLabel = new QLabel("Kernel: 等待后台信息", this);
    archLabel = new QLabel("Arch: 等待后台信息", this);
    osLabel = new QLabel("OS: 等待后台信息", this);
    screenLabel = new QLabel("Screen: 等待后台信息", this);

    kernelLabel->setObjectName("InfoLine");
    archLabel->setObjectName("InfoLine");
    osLabel->setObjectName("InfoLine");
    screenLabel->setObjectName("InfoLine");

    layout->addWidget(hintLabel);
    layout->addWidget(kernelLabel);
    layout->addWidget(archLabel);
    layout->addWidget(osLabel);
    layout->addWidget(screenLabel);
    layout->addStretch();
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
