#pragma once

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class QGridLayout;

class DetailBaseUi : public QWidget
{
    Q_OBJECT
public:
    explicit DetailBaseUi(const QString &title, QWidget *parent = nullptr);

protected:
    QLabel *addRow(QGridLayout *grid, int row, const QString &key, const QString &value = "--");
    void setStateLabel(QLabel *label, bool online);
};

class SensorThUi : public DetailBaseUi
{
    Q_OBJECT
public:
    explicit SensorThUi(QWidget *parent = nullptr);

    void setDeviceInfo(const QString &deviceName, const QString &masterName, int slaveAddr);
    void setOnline(bool online);
    void setTemperatureHumidity(double temperature, double humidity, const QString &updateTime);
    void clearData();

private:
    QLabel *deviceNameLabel = nullptr;
    QLabel *masterNameLabel = nullptr;
    QLabel *slaveAddrLabel = nullptr;
    QLabel *stateLabel = nullptr;
    QLabel *temperatureLabel = nullptr;
    QLabel *humidityLabel = nullptr;
    QLabel *updateTimeLabel = nullptr;
};

class RelayUi : public DetailBaseUi
{
    Q_OBJECT
public:
    explicit RelayUi(QWidget *parent = nullptr);

    void setDeviceInfo(const QString &deviceName,
                       const QString &masterName,
                       int masterSlot,
                       int slaveAddr);
    void setOnline(bool online);
    void setRelayStates(bool ledOn, bool fanOn, bool buzzerOn, const QString &updateTime);
    void clearData();

signals:
    void relayCommandRequested(int masterSlot,
                               int slaveAddr,
                               const QString &channel,
                               bool on);

private:
    QWidget *createButtonRow(const QString &channelText,
                             const QString &channel,
                             QPushButton *&onButton,
                             QPushButton *&offButton);
    void setControlEnabled(bool enabled);

    QLabel *deviceNameLabel = nullptr;
    QLabel *masterNameLabel = nullptr;
    QLabel *slaveAddrLabel = nullptr;
    QLabel *stateLabel = nullptr;
    QLabel *ledLabel = nullptr;
    QLabel *fanLabel = nullptr;
    QLabel *buzzerLabel = nullptr;
    QLabel *updateTimeLabel = nullptr;
    QPushButton *ledOnButton = nullptr;
    QPushButton *ledOffButton = nullptr;
    QPushButton *fanOnButton = nullptr;
    QPushButton *fanOffButton = nullptr;
    QPushButton *buzzerOnButton = nullptr;
    QPushButton *buzzerOffButton = nullptr;
    int currentMasterSlot = 0;
    int currentSlaveAddr = 0;
};

class MeterUi : public DetailBaseUi
{
    Q_OBJECT
public:
    explicit MeterUi(QWidget *parent = nullptr);

    void setDeviceInfo(const QString &deviceName, const QString &masterName, int slaveAddr);
    void setOnline(bool online);
    void setMeterValues(const QString &voltage,
                        const QString &current,
                        const QString &power,
                        const QString &energy,
                        const QString &updateTime);
    void clearData();

private:
    QLabel *deviceNameLabel = nullptr;
    QLabel *masterNameLabel = nullptr;
    QLabel *slaveAddrLabel = nullptr;
    QLabel *stateLabel = nullptr;
    QLabel *voltageLabel = nullptr;
    QLabel *currentLabel = nullptr;
    QLabel *powerLabel = nullptr;
    QLabel *energyLabel = nullptr;
    QLabel *updateTimeLabel = nullptr;
};
