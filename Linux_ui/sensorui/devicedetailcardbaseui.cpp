// ============================
// pages/PageStatus.h
// ============================

#ifndef PAGESTATUS_H
#define PAGESTATUS_H

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QMap>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QString>
#include <QVector>
#include <QWidget>

#include "sensorui/relaycontroldialog.h"

class SlaveListDialog;
class SlaveDetailDialog;
class SensorThDetailCardUi;
class RelayDetailCardUi;

struct MasterStatusInfo
{
    int masterSlot = -1;
    QString masterName;
};

struct SlaveDeviceInfo
{
    int masterSlot = -1;
    int slaveAddr = -1;
    QString deviceType;
    QString displayName;
    bool online = false;
    int pollIntervalMs = 1000;
};

struct SlaveRuntimeInfo
{
    bool online = false;

    bool hasSensorTh = false;
    double temperature = 0.0;
    double humidity = 0.0;

    bool hasRelay = false;
    QVector<RelayChannelInfo> relayChannels;

    bool ledOn = false;
    bool fanOn = false;
    bool buzzerOn = false;

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

    void setMasterList(const QList<MasterStatusInfo> &masters);

    void setCurrentMaster(int masterSlot,
                          const QString &masterName,
                          int slaveCount);

    void setSlaveList(const QList<SlaveDeviceInfo> &slaveList);

    void removeSlave(int masterSlot,
                     int slaveAddr,
                     const QString &deviceType);

    int currentMasterSlotValue() const;

    void updateSlaveOnline(int masterSlot,
                           int slaveAddr,
                           const QString &deviceType,
                           bool online);

    void setAlarmText(const QString &text);

    void setSensorThData(int masterSlot,
                         int slaveAddr,
                         double temperature,
                         double humidity,
                         const QString &updateTime);

    void setRelayChannels(int masterSlot,
                          int slaveAddr,
                          const QVector<RelayChannelInfo> &channels,
                          const QString &updateTime);

    void setRelayStates(int masterSlot,
                        int slaveAddr,
                        bool ledOn,
                        bool fanOn,
                        bool buzzerOn,
                        const QString &updateTime);

signals:
    void addSlaveRequested(int masterSlot);
    void masterChanged(int masterSlot);
    void slaveSelected(int masterSlot,
                       int slaveAddr,
                       const QString &deviceType);

    void removeSlaveRequested(int masterSlot,
                              int slaveAddr,
                              const QString &deviceType);

    void relayChannelCommandRequested(int masterSlot,
                                      int slaveAddr,
                                      int channel,
                                      bool on);

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

private:
    void initUI();

    void selectSlave(int index);
    void refreshMasterLabels();
    void refreshSlaveCards();
    void rebuildSlaveCards();
    void clearCurrentDetail();
    void updateSlaveCardStyle(int index, bool selected);
    QFrame *createSlaveCard(int index);

    void updateOpenDialogs();
    void openSlaveListDialog();
    void openSlaveDetailDialog(int index);

    QString runtimeKey(const SlaveDeviceInfo &slave) const;

    QString runtimeKey(int masterSlot,
                       int slaveAddr,
                       const QString &deviceType) const;

    SlaveRuntimeInfo runtimeForSlave(const SlaveDeviceInfo &slave) const;

    bool isCurrentSlave(int masterSlot,
                        int slaveAddr,
                        const QString &deviceType) const;

    QString displayTypeName(const QString &deviceType) const;
    QString displayMasterName() const;

    QVector<RelayChannelInfo> defaultRelayChannelsFromOldState(bool ledOn,
                                                               bool fanOn,
                                                               bool buzzerOn) const;

private:
    int currentMasterSlot = -1;
    QString currentMasterName;
    int currentSlaveIndex = -1;

    QList<SlaveDeviceInfo> slaves;
    QMap<QString, SlaveRuntimeInfo> slaveRuntime;

    QLabel *summaryLabel = nullptr;
    QLabel *alarmLabel = nullptr;

    QComboBox *masterCombo = nullptr;
    QPushButton *addSlaveButton = nullptr;

    QFrame *slaveListPanel = nullptr;
    QLabel *currentPortLabel = nullptr;
    QLabel *listTitleLabel = nullptr;
    QLabel *emptyListLabel = nullptr;
    QScrollArea *slaveScrollArea = nullptr;
    QVBoxLayout *slaveListLayout = nullptr;
    QList<SlaveCard> slaveCards;

    QStackedWidget *detailStack = nullptr;
    SensorThDetailCardUi *sensorThDetailUi = nullptr;
    RelayDetailCardUi *relayDetailUi = nullptr;

    SlaveListDialog *slaveListDialog = nullptr;
    SlaveDetailDialog *slaveDetailDialog = nullptr;
};

#endif // PAGESTATUS_H