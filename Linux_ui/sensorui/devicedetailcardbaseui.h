// ============================
// sensor_ui/devicedetailcardbaseui.h
// ============================

#ifndef DEVICEDETAILCARDBASEUI_H
#define DEVICEDETAILCARDBASEUI_H

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

class DeviceDetailCardBaseUi : public QFrame
{
    Q_OBJECT

public:
    struct MetricCard {
        QFrame *frame = nullptr;
        QLabel *icon = nullptr;
        QLabel *name = nullptr;
        QLabel *value = nullptr;
        QLabel *unit = nullptr;
    };

public:
    explicit DeviceDetailCardBaseUi(QWidget *parent = nullptr);
    virtual ~DeviceDetailCardBaseUi() = default;

    void setBaseInfo(const QString &portName,
                     int masterSlot,
                     int slaveAddr,
                     const QString &typeName,
                     const QString &deviceType,
                     bool online);

    void setPollInterval(int pollIntervalMs);
    void setLastUpdateTime(const QString &updateTime);
    void setOnline(bool online);

    virtual void clearData() = 0;

signals:
    void removeSlaveRequested(int masterSlot,
                              int slaveAddr,
                              const QString &deviceType);

protected:
    MetricCard createMetricCard(const QString &iconText,
                                const QString &name);

    void setMetricCard(MetricCard &card,
                       const QString &iconText,
                       const QString &name,
                       const QString &value,
                       const QString &unit);

    void setMetricVisible(bool a, bool b, bool c, bool d);

    QVBoxLayout *extraLayout();

protected:
    int currentMasterSlot = -1;
    int currentSlaveAddr = -1;
    QString currentDeviceType;

    QLabel *detailTitleLabel = nullptr;
    QLabel *detailStateLabel = nullptr;

    QLabel *detailPortLabel = nullptr;
    QLabel *detailAddrLabel = nullptr;
    QLabel *detailTypeLabel = nullptr;

    QFrame *metricPanel = nullptr;
    QGridLayout *metricGrid = nullptr;

    MetricCard metricA;
    MetricCard metricB;
    MetricCard metricC;
    MetricCard metricD;

    QLabel *pollIntervalLabel = nullptr;
    QLabel *lastUpdateLabel = nullptr;
    QPushButton *removeSlaveButton = nullptr;

private:
    QVBoxLayout *extraAreaLayout = nullptr;
};

#endif // DEVICEDETAILCARDBASEUI_H