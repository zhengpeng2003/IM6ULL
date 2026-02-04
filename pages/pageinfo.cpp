#include "pageinfo.h"
#include <QVBoxLayout>
#include <QDebug>

Pageinfo::Pageinfo(QWidget *parent)
    : QWidget(parent)
{
    initUI();
}

void Pageinfo::initUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    kernelLabel = new QLabel("Kernel: N/A", this);
    archLabel   = new QLabel("Arch: N/A", this);
    osLabel     = new QLabel("OS: N/A", this);
    screenLabel = new QLabel("Screen: N/A", this);

    layout->addWidget(kernelLabel);
    layout->addWidget(archLabel);
    layout->addWidget(osLabel);
    layout->addWidget(screenLabel);

    setLayout(layout);
}

void Pageinfo::addInfo(const DataPack &pack)
{
    qDebug()<<"singal is emit"<<endl;
    for (const auto &dev : pack.devices) {
        if (dev.type == DEV_SYSINFO && dev.valid) {
            const auto &sys = dev.sys;
            qDebug() << "[PageInfo] Update SYSINFO:"
                     << "kernel:" << sys.kernel
                     << "arch:"   << sys.arch
                     << "os:"     << sys.os
                     << "screen:" << sys.screenW << "x" << sys.screenH;

            // 更新界面
            kernelLabel->setText(QString("Kernel: %1").arg(sys.kernel));
            archLabel->setText(QString("Arch: %1").arg(sys.arch));
            osLabel->setText(QString("OS: %1").arg(sys.os));
            screenLabel->setText(QString("Screen: %1 x %2").arg(sys.screenW).arg(sys.screenH));
        }
    }
}
