#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <QWidget>
#include <QLabel>
#include "data/data_protocol.h"

class Pageinfo : public QWidget
{
    Q_OBJECT
public:
    explicit Pageinfo(QWidget *parent = nullptr);

    // 外部调用，用 DataPack 更新界面
    void addInfo(const DataPack &pack);

private:
    QLabel *kernelLabel;
    QLabel *archLabel;
    QLabel *osLabel;
    QLabel *screenLabel;

    void initUI();  // 初始化界面控件
};

#endif // PAGEINFO_H
