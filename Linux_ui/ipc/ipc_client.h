#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include <QObject>
#include <QSocketNotifier>
#include <QDebug>
#include "data/data_protocol.h"
#include "data/data_parser.h"
class IpcClient : public QObject
{
    Q_OBJECT
public:
    explicit IpcClient(QObject *parent = nullptr);
    ~IpcClient();

    bool connectToServer(const QString &path);
    bool sendMessage(const QByteArray &msg);

signals:
    void connected();
    void disconnected();
    void deviceStatusUpdated(const DataPack &pack);//pageinfo
    void devicetrend(const DataPack &pack);//pagetrend
    void deviceinfo(const DataPack &pack);//linux info
    void devicesetting(const DataPack &pack);//setting
    void errorOccured(const QString &err);

private slots:
    void onReadyRead();

private:
    int m_fd;
    QSocketNotifier *m_notifier;
    QByteArray m_recvBuf;//用于切包的
};

#endif // IPC_CLIENT_H
