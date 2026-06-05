#ifndef PAGETREND_H
#define PAGETREND_H

#include <QVector>
#include <QWidget>

#include "data/data_protocol.h"

class PageTrend : public QWidget
{
    Q_OBJECT
public:
    explicit PageTrend(QWidget *parent = nullptr);

public slots:
    void addData(const DataPack &pack);
    void onPortStatusUpdated(int slot,
                             const QString &port,
                             const QString &deviceType,
                             int baud,
                             bool connected,
                             const QString &message);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> tempList;
    QVector<double> humiList;

    int maxPoints = 240;
    bool baseInited = false;
    bool sensorConnected = false;
    double tempBase = 0.0;
    double humiBase = 0.0;
    double tempPixelScale = 3.0;
    double humiPixelScale = 2.0;
};

#endif // PAGETREND_H
