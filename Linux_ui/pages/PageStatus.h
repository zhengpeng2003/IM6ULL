#pragma once

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QEvent>
#include <QList>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QComboBox;
class QStackedWidget;
class QVBoxLayout;
class SensorThUi;
class RelayUi;
class MeterUi;

struct SlaveDeviceInfo {
    int masterSlot = 0;
    int slaveAddr = 0;
    QString deviceName;
    QString displayName;
    QString deviceType;
    bool online = false;
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
    void setMasterList(const QStringList &masters);
    void setCurrentMaster(const QString &masterName, int slaveCount);
    void setSlaveList(const QList<SlaveDeviceInfo> &slaveList);
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
        QLabel *state = nullptr;
    };

    void initUI();
    void refreshMasterLabels();
    void refreshSlaveCards();
    void rebuildSlaveCards();
    void updateSlaveCardStyle(int index, bool selected);
    QFrame *createSlaveCard(int index);
    bool isCurrentSlave(int masterSlot, int slaveAddr, const QString &deviceType) const;

    QLabel *summaryLabel = nullptr;
    QComboBox *masterCombo = nullptr;
    QLabel *slaveCountLabel = nullptr;
    QPushButton *addSlaveButton = nullptr;
    QLabel *alarmLabel = nullptr;
    QLabel *emptyListLabel = nullptr;
    QVBoxLayout *slaveListLayout = nullptr;

    QList<SlaveDeviceInfo> slaves;
    QVector<SlaveCard> slaveCards;
    int currentSlaveIndex = -1;
    int currentMasterSlot = 0;
    QString currentMasterName;

    QStackedWidget *detailStack = nullptr;
    SensorThUi *sensorThUi = nullptr;
    RelayUi *relayUi = nullptr;
    MeterUi *meterUi = nullptr;
};
