#include "TrendChartWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>

TrendChartWidget::TrendChartWidget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("TrendChart");
    setMinimumHeight(80);
}

void TrendChartWidget::setData(const QVector<double> &values, const QString &unit)
{
    dataValues = values;
    valueUnit = unit;
    update();
}

void TrendChartWidget::setRange(double minValue, double maxValue)
{
    rangeMin = minValue;
    rangeMax = maxValue;
    update();
}

void TrendChartWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#101820"));

    QRect plotRect = rect().adjusted(34, 6, -8, -18);
    if (plotRect.width() <= 0 || plotRect.height() <= 0)
        return;

    painter.setPen(QPen(QColor("#2E4053"), 1));
    for (int i = 0; i < 4; ++i) {
        const double ratio = i / 3.0;
        const int y = plotRect.top() + qRound(plotRect.height() * ratio);
        const double value = rangeMax - (rangeMax - rangeMin) * ratio;
        painter.drawLine(plotRect.left(), y, plotRect.right(), y);
        painter.setPen(QColor("#95A5A6"));
        painter.drawText(0, y - 7, 32, 14, Qt::AlignRight | Qt::AlignVCenter,
                         QString("%1%2").arg(value, 0, 'f', 0).arg(valueUnit));
        painter.setPen(QPen(QColor("#2E4053"), 1));
    }
    painter.drawLine(plotRect.bottomLeft(), plotRect.bottomRight());

    if (dataValues.size() < 2 || qFuzzyCompare(rangeMin, rangeMax)) {
        painter.setPen(QColor("#95A5A6"));
        painter.drawText(rect(), Qt::AlignCenter, "No trend data");
        return;
    }

    QPainterPath path;
    auto pointForIndex = [this, &plotRect](int index) {
        const double xRatio = dataValues.size() <= 1 ? 0.0 : index / double(dataValues.size() - 1);
        const double yRatio = (dataValues.at(index) - rangeMin) / (rangeMax - rangeMin);
        const double x = plotRect.left() + xRatio * plotRect.width();
        const double y = plotRect.bottom() - qBound(0.0, yRatio, 1.0) * plotRect.height();
        return QPointF(x, y);
    };

    path.moveTo(pointForIndex(0));
    for (int i = 1; i < dataValues.size(); ++i)
        path.lineTo(pointForIndex(i));

    painter.setPen(QPen(QColor("#00BFA5"), 2));
    painter.drawPath(path);
}
