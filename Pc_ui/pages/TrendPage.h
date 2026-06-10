#pragma once
#include <QString>
#include <QWidget>
class DataManager;
class QChartView;
class QComboBox;
class QLabel;
class QLineSeries;
class QJsonObject;

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
    void refreshPointList();
    void queryHistory();

private:
    DataManager *m_data = nullptr;
    QComboBox *m_pointCombo = nullptr;
    QComboBox *m_rangeCombo = nullptr;
    QLineSeries *m_series = nullptr;
    QLabel *m_statsLabel = nullptr;
};
