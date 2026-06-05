#ifndef PAGETREND_H
#define PAGETREND_H

#include <QWidget>
#include <QVector>
#include "data/data_protocol.h"

class PageTrend : public QWidget
{
    Q_OBJECT
public:
    explicit PageTrend(QWidget *parent = nullptr);

public slots:
    void addData(const DataPack &pack);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> tempList;
    QVector<double> humiList;

    int maxPoints = 240;

    /* ========= 基准值（第一次数据） ========= */
    bool baseInited = false;
    double tempBase = 0.0;
    double humiBase = 0.0;

    /* ========= 显示比例（像素映射用） ========= */
    double tempPixelScale = 3.0;   // ⭐ 1℃ = 3 像素（你可调）
    double humiPixelScale = 2.0;   // ⭐ 1% = 2 像素
};

#endif // PAGETREND_H
