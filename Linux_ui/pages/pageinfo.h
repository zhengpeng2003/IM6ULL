#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <QLabel>
#include <QWidget>

#include "data/data_protocol.h"

class Pageinfo : public QWidget
{
    Q_OBJECT
public:
    explicit Pageinfo(QWidget *parent = nullptr);
    void addInfo(const DataPack &pack);

private:
    void initUI();

    QLabel *hintLabel = nullptr;
    QLabel *kernelLabel = nullptr;
    QLabel *archLabel = nullptr;
    QLabel *osLabel = nullptr;
    QLabel *screenLabel = nullptr;
};

#endif // PAGEINFO_H
