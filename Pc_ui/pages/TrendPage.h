#pragma once
#include <QString>
#include <QWidget>
class DataManager;
class QComboBox;
class QDateTimeAxis;
class QLabel;
class QLineSeries;
class QJsonArray;
class QJsonObject;
class QTableWidget;
class QValueAxis;
class DeviceTreeWidget;

class TrendPage : public QWidget
{
    Q_OBJECT
public:
    explicit TrendPage(DataManager *data, QWidget *parent = nullptr);

public slots:
    void onHistoryPointsMessage(const QJsonObject &obj);

signals:
    void historyQueryRequested(const QString &pointId, qint64 startMs, qint64 endMs, int limit);

private slots:
    void refreshPointTree();
    void onPointSelected(const QString &pointId, const QString &deviceKey, const QString &pointName, const QString &unit);
    void queryHistory();

private:
    enum class HistoryRequestType {
        None,
        ChartRange,
        TableAllHistory
    };

    struct HistoryRequest {
        HistoryRequestType type = HistoryRequestType::None;
        QString pointId;
        qint64 startMs = 0;
        qint64 endMs = 0;
        int limit = 1000;

        bool isValid() const { return type != HistoryRequestType::None && !pointId.isEmpty(); }
    };

    void requestTableHistory();
    void enqueueHistoryRequest(const HistoryRequest &request);
    void sendHistoryRequest(const HistoryRequest &request);
    void flushPendingHistoryRequest();
    void handleChartHistoryResponse(const QJsonArray &points);
    void handleTableHistoryResponse(const QJsonArray &points);
    void clearChartView(const QString &message);
    void clearTableView();
    void clearCurrentSelection(const QString &message);
    void resetAxesToRequestRange(qint64 startMs, qint64 endMs);
    void updateAxes(qint64 firstTimestampMs, qint64 lastTimestampMs, double minValue, double maxValue);

    DataManager *m_data = nullptr;
    DeviceTreeWidget *m_tree = nullptr;
    QComboBox *m_rangeCombo = nullptr;
    QLineSeries *m_series = nullptr;
    QDateTimeAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    QTableWidget *m_historyTable = nullptr;
    QLabel *m_statsLabel = nullptr;
    QString m_currentPointId;
    QString m_currentDeviceKey;
    QString m_currentPointName;
    QString m_currentPointUnit;
    qint64 m_lastStartMs = 0;
    qint64 m_lastEndMs = 0;
    HistoryRequest m_activeHistoryRequest;
    HistoryRequest m_pendingHistoryRequest;
    bool m_historyRequestInFlight = false;
};
