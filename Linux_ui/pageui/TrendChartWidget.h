#pragma once

#include <QVector>
#include <QWidget>

class TrendChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);

    void setData(const QVector<double> &values, const QString &unit);
    void setRange(double minValue, double maxValue);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> dataValues;
    QString valueUnit = "C";
    double rangeMin = 0.0;
    double rangeMax = 100.0;
};
