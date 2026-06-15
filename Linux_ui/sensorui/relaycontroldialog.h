// ============================
// sensor_ui/relaycontroldialog.h
// ============================

#ifndef RELAYCONTROLDIALOG_H
#define RELAYCONTROLDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QVBoxLayout>
#include <QVector>

struct RelayChannelInfo
{
    int channel = 0;
    QString key;
    QString name;
    bool enabled = true;
    bool on = false;
};

class RelayControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RelayControlDialog(QWidget *parent = nullptr);

    void setRelayInfo(int masterSlot,
                      int slaveAddr,
                      const QString &portName,
                      const QVector<RelayChannelInfo> &channels);

signals:
    void relayCommandRequested(int masterSlot,
                               int slaveAddr,
                               int channel,
                               bool on);

private:
    void rebuildChannelRows();

private:
    int currentMasterSlot = -1;
    int currentSlaveAddr = -1;
    QString currentPortName;

    QVector<RelayChannelInfo> currentChannels;

    QLabel *titleLabel = nullptr;
    QLabel *metaLabel = nullptr;
    QWidget *contentWidget = nullptr;
    QVBoxLayout *contentLayout = nullptr;
};

#endif // RELAYCONTROLDIALOG_H