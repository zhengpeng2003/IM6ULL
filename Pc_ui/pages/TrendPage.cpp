#include "TrendPage.h"

#include "core/DataManager.h"
#include "ui/DeviceTreeWidget.h"

#include <limits>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QtGlobal>
#include <QVBoxLayout>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLegend>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace {

QString displayTime(qint64 timestampMs)
{
    if (timestampMs <= 0) {
        return QStringLiteral("--");
    }

    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString axisTimeFormat(qint64 startMs, qint64 endMs)
{
    const qint64 spanMs = endMs - startMs;
    if (spanMs <= 60LL * 60LL * 1000LL) {
        return QStringLiteral("HH:mm");
    }
    if (spanMs <= 24LL * 60LL * 60LL * 1000LL) {
        return QStringLiteral("HH:mm");
    }
    return QStringLiteral("MM-dd HH:mm");
}

} // namespace

TrendPage::TrendPage(DataManager *data, QWidget *parent)
    : QWidget(parent), m_data(data)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);

    auto *title = new QLabel(QStringLiteral("趋势分析"), this);
    title->setObjectName("PageTitle");
    layout->addWidget(title);

    auto *body = new QHBoxLayout;
    m_tree = new DeviceTreeWidget(this);
    m_tree->setMinimumWidth(260);
    body->addWidget(m_tree, 1);

    auto *right = new QVBoxLayout;
    auto *filters = new QHBoxLayout;
    m_rangeCombo = new QComboBox(this);
    m_rangeCombo->addItem(QStringLiteral("最近30分钟"), 30 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("最近1小时"), 60 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("最近6小时"), 6 * 60 * 60 * 1000);
    m_rangeCombo->addItem(QStringLiteral("全部历史"), 0);

    auto *queryButton = new QPushButton(QStringLiteral("查询"), this);
    filters->addWidget(new QLabel(QStringLiteral("时间范围:"), this));
    filters->addWidget(m_rangeCombo);
    filters->addWidget(queryButton);
    filters->addStretch();
    right->addLayout(filters);

    m_series = new QLineSeries(this);
    m_axisX = new QDateTimeAxis(this);
    m_axisY = new QValueAxis(this);
    m_axisX->setTickCount(11);
    m_axisX->setFormat(QStringLiteral("HH:mm"));
    m_axisY->setLabelFormat(QStringLiteral("%.2f"));

    auto *chart = new QChart();
    chart->addSeries(m_series);
    chart->addAxis(m_axisX, Qt::AlignBottom);
    chart->addAxis(m_axisY, Qt::AlignLeft);
    m_series->attachAxis(m_axisX);
    m_series->attachAxis(m_axisY);
    chart->legend()->hide();
    chart->setTitle(QStringLiteral("历史趋势图"));

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(now - 30LL * 60LL * 1000LL),
                      QDateTime::fromMSecsSinceEpoch(now));
    m_axisY->setRange(0.0, 1.0);

    auto *view = new QChartView(chart, this);
    right->addWidget(view, 3);

    m_statsLabel = new QLabel(QStringLiteral("请选择左侧测点后查询历史数据"), this);
    m_statsLabel->setObjectName("StatsLabel");
    right->addWidget(m_statsLabel);

    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(4);
    m_historyTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("时间")
        << QStringLiteral("数值")
        << QStringLiteral("文本值")
        << QStringLiteral("状态"));
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    right->addWidget(m_historyTable, 2);

    body->addLayout(right, 3);
    layout->addLayout(body, 1);

    connect(queryButton, &QPushButton::clicked, this, &TrendPage::queryHistory);
    connect(m_tree, &DeviceTreeWidget::pointSelected, this, &TrendPage::onPointSelected);
    connect(m_tree, &DeviceTreeWidget::selectionLost, this, [this]() {
        clearCurrentSelection(QStringLiteral("请选择左侧测点后查询历史数据"));
    });

    if (m_data) {
        connect(m_data, &DataManager::realtimeDataUpdated,
                this, &TrendPage::refreshPointTree);
    }

    refreshPointTree();
}

void TrendPage::refreshPointTree()
{
    if (!m_data || !m_tree) {
        return;
    }

    m_tree->setRealtimeDevices(m_data->allRealtimeData(), true);
    if (m_data->allRealtimeData().isEmpty()) {
        clearCurrentSelection(QStringLiteral("暂无可用测点"));
    }
}

void TrendPage::onPointSelected(const QString &pointId, const QString &deviceKey, const QString &pointName, const QString &unit)
{
    m_currentPointId = pointId;
    m_currentDeviceKey = deviceKey;
    m_currentPointName = pointName;
    m_currentPointUnit = unit;

    const QString unitText = unit.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(unit);
    m_statsLabel->setText(QStringLiteral("当前测点: %1%2\npointId: %3\n请点击查询历史数据")
                              .arg(pointName.isEmpty() ? pointId : pointName, unitText, pointId));
    requestTableHistory();
}

void TrendPage::queryHistory()
{
    if (m_currentPointId.isEmpty()) {
        clearChartView(QStringLiteral("请选择左侧测点后查询历史数据"));
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 rangeMs = m_rangeCombo ? m_rangeCombo->currentData().toLongLong() : 30LL * 60LL * 1000LL;
    m_lastStartMs = rangeMs > 0 ? now - rangeMs : 0;
    m_lastEndMs = rangeMs > 0 ? now : 0;

    m_series->clear();
    resetAxesToRequestRange(m_lastStartMs, m_lastEndMs);
    m_statsLabel->setText(QStringLiteral("正在查询历史数据..."));

    HistoryRequest request;
    request.type = HistoryRequestType::ChartRange;
    request.pointId = m_currentPointId;
    request.startMs = m_lastStartMs;
    request.endMs = m_lastEndMs;
    request.limit = 1000;
    enqueueHistoryRequest(request);
}

void TrendPage::onHistoryPointsMessage(const QJsonObject &obj)
{
    if (!m_series || !m_statsLabel || !m_historyTable) {
        return;
    }

    const HistoryRequest request = m_activeHistoryRequest;
    const QString responsePointId = obj.value(QStringLiteral("pointId")).toString();
    if (!request.isValid() || (!responsePointId.isEmpty() && responsePointId != request.pointId)) {
        m_historyRequestInFlight = false;
        m_activeHistoryRequest = HistoryRequest();
        flushPendingHistoryRequest();
        return;
    }

    if (request.pointId != m_currentPointId) {
        m_historyRequestInFlight = false;
        m_activeHistoryRequest = HistoryRequest();
        flushPendingHistoryRequest();
        return;
    }

    const bool ok = obj.value(QStringLiteral("ok")).toBool(true);
    const QString reason = obj.value(QStringLiteral("reason")).toString();
    const QString message = obj.value(QStringLiteral("message")).toString();
    const int count = obj.value(QStringLiteral("count")).toInt(-1);
    const QJsonArray points = obj.value(QStringLiteral("points")).toArray();

    if (!ok) {
        const QString detail = message.isEmpty() ? reason : QStringLiteral("%1/%2").arg(reason, message);
        if (request.type == HistoryRequestType::ChartRange) {
            clearChartView(reason == QStringLiteral("invalid_point_id")
                               ? QStringLiteral("点位 ID 无效")
                               : QStringLiteral("历史查询失败：%1").arg(detail));
        } else if (request.type == HistoryRequestType::TableAllHistory) {
            clearTableView();
        }
    } else if (count == 0 || points.isEmpty()) {
        if (request.type == HistoryRequestType::ChartRange) {
            clearChartView(QStringLiteral("当前点位暂无历史数据"));
        } else if (request.type == HistoryRequestType::TableAllHistory) {
            clearTableView();
        }
    } else if (request.type == HistoryRequestType::ChartRange) {
        m_lastStartMs = request.startMs;
        m_lastEndMs = request.endMs;
        handleChartHistoryResponse(points);
    } else if (request.type == HistoryRequestType::TableAllHistory) {
        handleTableHistoryResponse(points);
    }

    m_historyRequestInFlight = false;
    m_activeHistoryRequest = HistoryRequest();
    flushPendingHistoryRequest();
}

void TrendPage::requestTableHistory()
{
    if (m_currentPointId.isEmpty()) {
        clearTableView();
        return;
    }

    HistoryRequest request;
    request.type = HistoryRequestType::TableAllHistory;
    request.pointId = m_currentPointId;
    request.startMs = 0;
    request.endMs = 0;
    request.limit = 5000;
    enqueueHistoryRequest(request);
}

void TrendPage::enqueueHistoryRequest(const HistoryRequest &request)
{
    if (!request.isValid()) {
        return;
    }

    if (m_historyRequestInFlight) {
        m_pendingHistoryRequest = request;
        return;
    }

    sendHistoryRequest(request);
}

void TrendPage::sendHistoryRequest(const HistoryRequest &request)
{
    if (!request.isValid()) {
        return;
    }

    m_activeHistoryRequest = request;
    m_historyRequestInFlight = true;
    emit historyQueryRequested(request.pointId, request.startMs, request.endMs, request.limit);
}

void TrendPage::flushPendingHistoryRequest()
{
    if (m_historyRequestInFlight || !m_pendingHistoryRequest.isValid()) {
        return;
    }

    const HistoryRequest request = m_pendingHistoryRequest;
    m_pendingHistoryRequest = HistoryRequest();
    sendHistoryRequest(request);
}

void TrendPage::handleChartHistoryResponse(const QJsonArray &points)
{
    m_series->clear();
    if (points.isEmpty()) {
        resetAxesToRequestRange(m_lastStartMs, m_lastEndMs);
        m_statsLabel->setText(QStringLiteral("没有查询到历史数据"));
        return;
    }

    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    double sumValue = 0.0;
    qint64 firstTimestampMs = 0;
    qint64 lastTimestampMs = 0;
    double lastValue = 0.0;
    int validCount = 0;

    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject point = value.toObject();
        const qint64 timestampMs = point.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
        const double numberValue = point.value(QStringLiteral("numberValue")).toDouble();
        const bool valid = point.value(QStringLiteral("valid")).toBool(true);

        if (!valid) {
            continue;
        }

        m_series->append(static_cast<qreal>(timestampMs), numberValue);
        minValue = qMin(minValue, numberValue);
        maxValue = qMax(maxValue, numberValue);
        sumValue += numberValue;
        lastValue = numberValue;
        if (firstTimestampMs <= 0) {
            firstTimestampMs = timestampMs;
        }
        lastTimestampMs = timestampMs;
        ++validCount;
    }

    if (validCount == 0 || m_series->points().isEmpty()) {
        m_series->clear();
        resetAxesToRequestRange(m_lastStartMs, m_lastEndMs);
        m_statsLabel->setText(QStringLiteral("历史数据均为无效点"));
        return;
    }

    updateAxes(firstTimestampMs, lastTimestampMs, minValue, maxValue);

    const QString unitText = m_currentPointUnit.isEmpty() ? QString() : QStringLiteral(" %1").arg(m_currentPointUnit);
    m_statsLabel->setText(QStringLiteral("当前值: %1%5    最大值: %2%5    最小值: %3%5    平均值: %4%5")
                              .arg(lastValue, 0, 'f', 2)
                              .arg(maxValue, 0, 'f', 2)
                              .arg(minValue, 0, 'f', 2)
                              .arg(sumValue / validCount, 0, 'f', 2)
                              .arg(unitText));
}

void TrendPage::handleTableHistoryResponse(const QJsonArray &points)
{
    m_historyTable->setRowCount(points.size());
    int row = 0;
    for (const QJsonValue &value : points) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject point = value.toObject();
        const qint64 timestampMs = point.value(QStringLiteral("timestampMs")).toVariant().toLongLong();
        const double numberValue = point.value(QStringLiteral("numberValue")).toDouble();
        const QString textValue = point.value(QStringLiteral("textValue")).toString();
        const bool valid = point.value(QStringLiteral("valid")).toBool(true);

        m_historyTable->setItem(row, 0, new QTableWidgetItem(displayTime(timestampMs)));
        m_historyTable->setItem(row, 1, new QTableWidgetItem(QString::number(numberValue, 'f', 2)));
        m_historyTable->setItem(row, 2, new QTableWidgetItem(textValue));
        m_historyTable->setItem(row, 3, new QTableWidgetItem(valid ? QStringLiteral("有效") : QStringLiteral("无效")));
        ++row;
    }
    m_historyTable->setRowCount(row);
}

void TrendPage::clearChartView(const QString &message)
{
    if (m_series) {
        m_series->clear();
    }
    resetAxesToRequestRange(m_lastStartMs, m_lastEndMs);
    if (m_statsLabel) {
        m_statsLabel->setText(message);
    }
}

void TrendPage::clearTableView()
{
    if (m_historyTable) {
        m_historyTable->setRowCount(0);
    }
}

void TrendPage::clearCurrentSelection(const QString &message)
{
    m_currentPointId.clear();
    m_currentDeviceKey.clear();
    m_currentPointName.clear();
    m_currentPointUnit.clear();
    m_activeHistoryRequest = HistoryRequest();
    m_pendingHistoryRequest = HistoryRequest();
    m_historyRequestInFlight = false;
    clearChartView(message);
    clearTableView();
}

void TrendPage::resetAxesToRequestRange(qint64 startMs, qint64 endMs)
{
    if (startMs <= 0 || endMs <= 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        startMs = now - 30LL * 60LL * 1000LL;
        endMs = now;
    }

    if (endMs <= startMs) {
        startMs -= 30LL * 1000LL;
        endMs += 30LL * 1000LL;
    }

    m_axisX->setTickCount(11);
    m_axisX->setFormat(axisTimeFormat(startMs, endMs));
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(startMs),
                      QDateTime::fromMSecsSinceEpoch(endMs));
    m_axisY->setRange(0.0, 1.0);
}

void TrendPage::updateAxes(qint64 firstTimestampMs, qint64 lastTimestampMs, double minValue, double maxValue)
{
    qint64 axisStartMs = m_lastStartMs;
    qint64 axisEndMs = m_lastEndMs;

    if (axisStartMs <= 0 || axisEndMs <= 0) {
        axisStartMs = firstTimestampMs;
        axisEndMs = lastTimestampMs;
    }

    if (axisStartMs <= 0 || axisEndMs <= 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        axisStartMs = now - 30LL * 60LL * 1000LL;
        axisEndMs = now;
    }

    if (axisEndMs <= axisStartMs) {
        axisStartMs -= 30LL * 1000LL;
        axisEndMs += 30LL * 1000LL;
    }

    const double span = maxValue - minValue;
    const double padding = span > 0.0 ? span * 0.1 : qMax(qAbs(maxValue) * 0.1, 1.0);
    m_axisX->setTickCount(11);
    m_axisX->setFormat(axisTimeFormat(axisStartMs, axisEndMs));
    m_axisX->setRange(QDateTime::fromMSecsSinceEpoch(axisStartMs),
                      QDateTime::fromMSecsSinceEpoch(axisEndMs));
    m_axisY->setRange(minValue - padding, maxValue + padding);
}
