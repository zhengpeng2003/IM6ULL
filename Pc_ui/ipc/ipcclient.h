#ifndef IPCCLIENT_H
#define IPCCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QLocalSocket>
#include <QString>

class IpcClient : public QObject
{
    Q_OBJECT

public:
    explicit IpcClient(QObject *parent = nullptr);
    ~IpcClient();

    bool connectToServer(const QString &serverName = "PcDataIpcPipe");
    void disconnectFromServer();

    bool isConnected() const;

    bool sendMessage(const QByteArray &msg);
    bool sendMessage(const QString &msg);

signals:
    void connected();
    void disconnected();
    void errorOccured(const QString &error);
    void messageReceived(const QByteArray &frame);

private:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onErrorOccurred(QLocalSocket::LocalSocketError socketError);
    void processReceivedData(const QByteArray &data);

private:
    QLocalSocket *m_socket = nullptr;
    QByteArray m_recvBuffer;
};

#endif // IPCCLIENT_H
