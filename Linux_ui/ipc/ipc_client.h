#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include <QObject>
#include <QLocalSocket>
#include <QDebug>
#include <QStringList>
#include "data/data_protocol.h"
#include "data/data_parser.h"

class IpcClient : public QObject
{
    Q_OBJECT
public:
    explicit IpcClient(QObject *parent = nullptr);
    ~IpcClient();

    bool connectToServer(const QString &path);
    bool isConnected() const;
    bool sendMessage(const QByteArray &msg);

signals:
    void connected();
    void disconnected();
    void deviceStatusUpdated(const DataPack &pack);//pageinfo
    void devicetrend(const DataPack &pack);//pagetrend
    void deviceinfo(const DataPack &pack);//linux info
    void devicesetting(const DataPack &pack);//setting
    void errorOccured(const QString &err);
    void portsUpdated(const QStringList &ports);
    void portStatusUpdated(int slot,
                           const QString &port,
                           const QString &deviceType,
                           int baud,
                           bool connected,
                           const QString &message);
    void deviceRegistered(quint32 seq,
                          int slot,
                          int deviceId,
                          const QString &deviceName,
                          const QString &deviceType,
                          int pollIntervalMs);
    void commandAckReceived(quint32 seq,
                            const QString &cmd,
                            const QString &status,
                            const QString &reason,
                            const QString &message);
    void alarmConfigReceived(double tempHigh, double humiHigh);
    void emergencyReceived(int level,
                           const QString &reason,
                           int deviceId,
                           const QString &deviceType,
                           const QString &pointKey,
                           double value,
                           double threshold,
                           double temp,
                           double humi);

private slots:
    void onReadyRead();

private:
    QString serverNameFromPath(const QString &path) const;

    QLocalSocket *m_socket;
    QByteArray m_recvBuf;//用于切包的
};


#endif // IPC_CLIENT_H
