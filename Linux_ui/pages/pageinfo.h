#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <QLabel>
#include <QPushButton>
#include <QWidget>
#include<QVariant>
#include "data/data_protocol.h"

class Pageinfo : public QWidget
{
    Q_OBJECT
public:
    explicit Pageinfo(QWidget *parent = nullptr);
    void addInfo(const DataPack &pack);
    void setIpcConnected(bool connected);

signals:
    void reconnectIpcRequested();

private:
    void initUI();
    void updateIpcStatusLabel();

    QLabel *hintLabel = nullptr;
    QLabel *ipcStatusLabel = nullptr;
    QPushButton *reconnectButton = nullptr;
    QLabel *kernelLabel = nullptr;
    QLabel *archLabel = nullptr;
    QLabel *osLabel = nullptr;
    QLabel *screenLabel = nullptr;
    bool m_ipcConnected = false;
};

#endif // PAGEINFO_H
