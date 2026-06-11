#pragma once

#include <QVector>
#include <QWidget>
#include <QStringList>

class TrendChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TrendChartWidget(QWidget *parent = nullptr);

    void setData(const QVector<double> &values, const QString &unit);
    void setRange(double minValue, double maxValue);
    void setAxisLabels(const QStringList &labels);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVector<double> dataValues;
    QStringList axisLabels;
    QString valueUnit = "C";
    double rangeMin = 0.0;
    double rangeMax = 100.0;
};
