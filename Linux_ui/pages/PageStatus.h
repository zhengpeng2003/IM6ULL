#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QGridLayout;
class QHBoxLayout;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class SlaveDetailDialog;
class SlaveListDialog;

struct SlaveDeviceInfo {
    int masterSlot = 0;
    int slaveAddr = 0;
    QString deviceName;
    QString displayName;
    QString deviceType;
    bool online = false;
};

struct MasterStatusInfo {
    int masterSlot = 0;
    QString masterName;
};

struct SlaveRuntimeInfo {
    bool online = false;
    bool hasSensorTh = false;
    double temperature = 0.0;
    double humidity = 0.0;
    bool hasRelay = false;
    bool ledOn = false;
    bool fanOn = false;
    bool buzzerOn = false;
    bool hasMeter = false;
    QString voltage;
    QString current;
    QString power;
    QString energy;
    QString updateTime;
};

class PageStatus : public QWidget
{
    Q_OBJECT
public:
    explicit PageStatus(QWidget *parent = nullptr);

    void setMasterSummary(int masterCount,
                          int onlineSlaveCount,
                          int alarmCount,
                          const QString &mqttState);
    void setMasterList(const QList<MasterStatusInfo> &masters,
                       int preferredMasterSlot = -1);
    void setCurrentMaster(const QString &masterName, int slaveCount);
    void setSlaveList(const QList<SlaveDeviceInfo> &slaveList);
    int currentMasterSlotValue() const;
    void updateSlaveOnline(int masterSlot,
                           int slaveAddr,
                           const QString &deviceType,
                           bool online);
    void selectSlave(int index);
    void setAlarmText(const QString &text);
    void setSensorThData(int masterSlot,
                         int slaveAddr,
                         double temperature,
                         double humidity,
                         const QString &updateTime);
    void setRelayStates(int masterSlot,
                        int slaveAddr,
                        bool ledOn,
                        bool fanOn,
                        bool buzzerOn,
                        const QString &updateTime);
    void setMeterValues(int masterSlot,
                        int slaveAddr,
                        const QString &voltage,
                        const QString &current,
                        const QString &power,
                        const QString &energy,
                        const QString &updateTime);

signals:
    void addSlaveRequested(int masterSlot);
    void masterChanged(int masterSlot);
    void slaveSelected(int masterSlot, int slaveAddr, const QString &deviceType);
    void relayCommandRequested(int masterSlot,
                               int slaveAddr,
                               const QString &channel,
                               bool on);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct SlaveCard {
        QFrame *frame = nullptr;
        QLabel *title = nullptr;
        QLabel *dot = nullptr;
        QLabel *state = nullptr;
    };

    struct MetricCard {
        QFrame *frame = nullptr;
        QLabel *icon = nullptr;
        QLabel *name = nullptr;
        QLabel *value = nullptr;
        QLabel *unit = nullptr;
    };

    void initUI();
    void refreshMasterLabels();
    void refreshSlaveCards();
    void rebuildSlaveCards();
    void clearCurrentDetail();
    void updateSlaveCardStyle(int index, bool selected);
    QFrame *createSlaveCard(int index);
    MetricCard createMetricCard(const QString &iconText, const QString &name);
    void setMetricCard(MetricCard &card,
                       const QString &iconText,
                       const QString &name,
                       const QString &value,
                       const QString &unit);
    void setDetailMeta(const SlaveDeviceInfo &slave);
    void showSensorDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime);
    void showRelayDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime);
    void showMeterDetail(const SlaveDeviceInfo &slave, const SlaveRuntimeInfo &runtime);
    QString displayTypeName(const QString &deviceType) const;
    QString displayMasterName() const;
    void openSlaveListDialog();
    void openSlaveDetailDialog(int index);
    void updateOpenDialogs();
    QString runtimeKey(const SlaveDeviceInfo &slave) const;
    QString runtimeKey(int masterSlot, int slaveAddr, const QString &deviceType) const;
    SlaveRuntimeInfo runtimeForSlave(const SlaveDeviceInfo &slave) const;
    bool isCurrentSlave(int masterSlot, int slaveAddr, const QString &deviceType) const;

    QLabel *summaryLabel = nullptr;
    QLabel *listTitleLabel = nullptr;
    QLabel *currentPortLabel = nullptr;
    QComboBox *masterCombo = nullptr;
    QLabel *slaveCountLabel = nullptr;
    QPushButton *addSlaveButton = nullptr;
    QLabel *alarmLabel = nullptr;
    QFrame *slaveListPanel = nullptr;
    QScrollArea *slaveScrollArea = nullptr;
    QLabel *emptyListLabel = nullptr;
    QVBoxLayout *slaveListLayout = nullptr;

    QList<SlaveDeviceInfo> slaves;
    QMap<QString, SlaveRuntimeInfo> slaveRuntime;
    QVector<SlaveCard> slaveCards;
    SlaveListDialog *slaveListDialog = nullptr;
    SlaveDetailDialog *slaveDetailDialog = nullptr;
    int currentSlaveIndex = -1;
    int currentMasterSlot = -1;
    QString currentMasterName;

    QFrame *detailPanel = nullptr;
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
    QWidget *relayControlPanel = nullptr;
    QPushButton *ledOnButton = nullptr;
    QPushButton *ledOffButton = nullptr;
    QPushButton *fanOnButton = nullptr;
    QPushButton *fanOffButton = nullptr;
    QPushButton *buzzerOnButton = nullptr;
    QPushButton *buzzerOffButton = nullptr;
    QLabel *pollIntervalLabel = nullptr;
    QLabel *lastUpdateLabel = nullptr;
};
