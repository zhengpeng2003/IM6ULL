#ifndef PAGETREND_H
#define PAGETREND_H

#include <QDateTime>
#include <QList>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "data/data_protocol.h"
#include "pages/PageStatus.h"

class QLabel;
class QPushButton;
class QComboBox;
class QFrame;
class TrendChartWidget;

class PageTrend : public QWidget
{
    Q_OBJECT
public:
    explicit PageTrend(QWidget *parent = nullptr);

    void setMasterList(const QList<MasterStatusInfo> &masters);
    void setSlaveList(const QList<SlaveDeviceInfo> &slaves);

signals:
    void masterChanged(int masterSlot);
    void slaveChanged(int masterSlot, int slaveAddr, const QString &deviceType);
    void timeRangeChanged(const QString &range);

public slots:
    void addData(const DataPack &pack);

private:
    struct CurveOption {
        QString key;
        QString text;
        QString unit;
    };

    struct TrendPoint {
        QDateTime time;
        int masterSlot = 0;
        int slaveAddr = 0;
        QString deviceType;
        QString curveKey;
        double value = 0.0;
    };

    struct StatCard {
        QFrame *frame = nullptr;
        QLabel *title = nullptr;
        QLabel *value = nullptr;
    };

    void initUI();
    void refreshSlaveCombo();
    void refreshCurveCombo();
    void refreshControlState();
    void queryCurrentTrend();
    void updateStats(const QVector<double> &values, const QString &unit);
    void setStatCard(StatCard &card, const QString &value, const QString &unit);
    void appendPoint(const QDateTime &time,
                     int masterSlot,
                     const DeviceData &device,
                     const QString &curveKey,
                     double value);
    int selectedMasterSlot() const;
    SlaveDeviceInfo selectedSlave() const;
    CurveOption selectedCurve() const;
    QList<CurveOption> curveOptionsForDevice(const QString &deviceType) const;
    int rangeSeconds() const;
    QStringList makeAxisLabels(const QDateTime &endTime, int seconds) const;
    int masterSlotForDevice(int slaveAddr, DeviceType type) const;
    QString deviceTypeString(DeviceType type) const;
    void trimCache();

    QComboBox *masterCombo = nullptr;
    QComboBox *slaveCombo = nullptr;
    QComboBox *curveCombo = nullptr;
    QComboBox *timeRangeCombo = nullptr;
    QPushButton *queryButton = nullptr;
    TrendChartWidget *chartWidget = nullptr;
    StatCard latestCard;
    StatCard maxCard;
    StatCard minCard;
    StatCard avgCard;

    QList<MasterStatusInfo> masterInfos;
    QList<SlaveDeviceInfo> slaveInfos;
    QVector<TrendPoint> trendPoints;
    int maxCachePoints = 720;
};

#endif // PAGETREND_H
