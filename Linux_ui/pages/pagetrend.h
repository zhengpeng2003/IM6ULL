#ifndef PAGETREND_H
#define PAGETREND_H

#include <QVector>
#include <QWidget>
#include <QStringList>

#include "data/data_protocol.h"

class QLabel;
class QPushButton;
class QComboBox;
class TrendChartWidget;

class PageTrend : public QWidget
{
    Q_OBJECT
public:
    explicit PageTrend(QWidget *parent = nullptr);

    void setMasterList(const QStringList &masters);
    void setSlaveList(const QStringList &slaves);
    void appendTemperature(double value);
    void appendHumidity(double value);
    void setTemperatureData(const QVector<double> &values);
    void setHumidityData(const QVector<double> &values);

signals:
    void masterChanged(int index);
    void slaveChanged(int index);
    void timeRangeChanged(const QString &range);

public slots:
    void addData(const DataPack &pack);

private:
    enum TrendMode {
        TemperatureMode,
        HumidityMode
    };

    void initUI();
    void switchTrendMode(TrendMode mode);
    void updateModeButtons();
    void updateChart();
    void updateStats();
    void trimData(QVector<double> &values);

    QComboBox *masterCombo = nullptr;
    QComboBox *slaveCombo = nullptr;
    QComboBox *timeRangeCombo = nullptr;
    QPushButton *temperatureButton = nullptr;
    QPushButton *humidityButton = nullptr;
    TrendChartWidget *chartWidget = nullptr;
    QLabel *statsLabel = nullptr;

    QVector<double> temperatureValues;
    QVector<double> humidityValues;
    TrendMode currentMode = TemperatureMode;
    int maxPoints = 80;
};

#endif // PAGETREND_H
