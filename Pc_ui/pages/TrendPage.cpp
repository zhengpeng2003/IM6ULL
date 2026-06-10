#include "TrendPage.h"

#include "core/DataManager.h"

#include <limits>

#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

TrendPage::TrendPage(DataManager *data, QWidget *parent)
    : QWidget(parent), m_data(data)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("趋势分析"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *filters = new QHBoxLayout;
    m_pointCombo = new QComboBox(this);
    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(QStringLiteral("最近30分钟"), 30 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("最近1小时"), 60 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("最近6小时"), 6 * 60 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("全部历史"), 0);

    auto *queryButton = new QPushButton(QStringLiteral("查询"), this);
    filters->addWidget(m_pointCombo, 2);
    filters->addWidget(m_rangeCombo);
    filters->addWidget(queryButton);
    filters->addWidget(new QPushButton(QStringLiteral("暂停刷新"), this));
    filters->addWidget(new QPushButton(QStringLiteral("导出CSV"), this));
    layout->addLayout(filters);

    m_series = new QLineSeries(this);
    auto *chart = new QChart();
    chart->addSeries(m_series);
    chart->createDefaultAxes();
    chart->setTitle(QStringLiteral("历史趋势图"));
    auto *view = new QChartView(chart, this);
    layout->addWidget(view, 1);

    m_statsLabel = new QLabel(QStringLiteral("请选择测点后查询历史数据"), this);
    m_statsLabel->setObjectName("StatsLabel");
    layout->addWidget(m_statsLabel);

    connect(queryButton, &QPushButton::clicked, this, &TrendPage::queryHistory);

    if (m_data) {
        connect(m_data, &DataManager::realtimeDataUpdated,
                this, &TrendPage::refreshPointList);
    }

    refreshPointList();
}

void TrendPage::refreshPointList()
{
    if (!m_data || !m_pointCombo) {
        return;
    }

    const QString currentPointId = m_pointCombo->currentData().toString();
    QSignalBlocker blocker(m_pointCombo);
    m_pointCombo->clear();

    for (const RealtimeDeviceData &device : m_data->allRealtimeData()) {
        for (const TelemetryPointData &point : device.points) {
            if (point.pointId.isEmpty() || point.valueType == "text") {
                continue;
            }

            const QString label = QStringLiteral("%1 / %2 / %3")
                .arg(device.node.deviceName, point.pointName, point.pointId);
            m_pointCombo->addItem(label, point.pointId);
        }
    }

    const int index = m_pointCombo->findData(currentPointId);
    if (index >= 0) {
        m_pointCombo->setCurrentIndex(index);
    }
}

void TrendPage::queryHistory()
{
    const QString pointId = m_pointCombo ? m_pointCombo->currentData().toString() : QString();
    if (pointId.isEmpty()) {
        if (m_statsLabel) {
            m_statsLabel->setText(QStringLiteral("暂无可查询的测点"));
        }
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 rangeMs = m_rangeCombo ? m_rangeCombo->currentData().toLongLong() : 30 * 60 * 1000;
    const qint64 startMs = rangeMs > 0 ? now - rangeMs : 0;
    const qint64 endMs = rangeMs > 0 ? now : 0;

    emit historyQueryRequested(pointId, startMs, endMs, 1000);
}

void TrendPage::onHistoryPointsMessage(const QJsonObject &obj)
{
    if (!m_series || !m_statsLabel) {
        return;
    }

    m_series->clear();

    const QJsonArray points = obj.value(QStringLiteral("points")).toArray();
    if (points.isEmpty()) {
        m_statsLabel->setText(QStringLiteral("没有查询到历史数据"));
        return;
    }

    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    double sumValue = 0.0;
    int validCount = 0;

    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject point = value.toObject();
        if (!point.value(QStringLiteral("valid")).toBool(true)) {
            continue;
        }

        const qint64 timestampMs = point.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
        const double numberValue = point.value(QStringLiteral("numberValue")).toDouble();

        m_series->append(static_cast<qreal>(timestampMs), numberValue);
        minValue = qMin(minValue, numberValue);
        maxValue = qMax(maxValue, numberValue);
        sumValue += numberValue;
        ++validCount;
    }

    if (validCount == 0 || m_series->points().isEmpty()) {
        m_statsLabel->setText(QStringLiteral("历史数据均为无效点"));
        return;
    }

    if (QChart *chart = m_series->chart()) {
        chart->createDefaultAxes();
    }

    m_statsLabel->setText(QStringLiteral("当前值: %1    最大值: %2    最小值: %3    平均值: %4")
                              .arg(m_series->points().last().y(), 0, 'f', 2)
                              .arg(maxValue, 0, 'f', 2)
                              .arg(minValue, 0, 'f', 2)
                              .arg(sumValue / validCount, 0, 'f', 2));
}
