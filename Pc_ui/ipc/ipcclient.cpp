#include "ipcclient.h"

#include <QDebug>

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QLocalSocket(this);

    connect(m_socket, &QLocalSocket::connected,
            this, &IpcClient::onConnected);

    connect(m_socket, &QLocalSocket::disconnected,
            this, &IpcClient::onDisconnected);

    connect(m_socket, &QLocalSocket::readyRead,
            this, &IpcClient::onReadyRead);

    connect(m_socket, &QLocalSocket::errorOccurred,
            this, &IpcClient::onErrorOccurred);
}

IpcClient::~IpcClient()
{
    disconnectFromServer();
}

bool IpcClient::connectToServer(const QString &serverName)
{
    if (isConnected()) {
        return true;
    }

    if (m_socket->state() == QLocalSocket::ConnectingState) {
        return true;
    }

    m_recvBuffer.clear();

    qDebug() << "IpcClient connecting to:" << serverName;

    /*
     * 注意：
     * QLocalSocket 不需要写 "\\\\.\\pipe\\PcDataIpcPipe"
     * 直接写服务名 "PcDataIpcPipe" 即可。
     */
    m_socket->connectToServer(serverName);

    return true;
}

void IpcClient::disconnectFromServer()
{
    if (!m_socket) {
        return;
    }

    if (m_socket->state() == QLocalSocket::UnconnectedState) {
        return;
    }

    m_socket->disconnectFromServer();

    if (m_socket->state() != QLocalSocket::UnconnectedState) {
        m_socket->abort();
    }

    m_recvBuffer.clear();
}

bool IpcClient::isConnected() const
{
    if (!m_socket) {
        return false;
    }

    return m_socket->state() == QLocalSocket::ConnectedState;
}

bool IpcClient::sendMessage(const QByteArray &msg)
{
    if (!isConnected()) {
        emit errorOccured("QLocalSocket is not connected.");
        return false;
    }

    QByteArray frame = msg;

    if (!frame.endsWith('\n')) {
        frame.append('\n');
    }

    qint64 written = m_socket->write(frame);

    if (written != frame.size()) {
        emit errorOccured("QLocalSocket write failed.");
        return false;
    }

    m_socket->flush();

    qDebug() << "IpcClient send:" << frame;

    return true;
}

bool IpcClient::sendMessage(const QString &msg)
{
    return sendMessage(msg.toUtf8());
}

void IpcClient::onConnected()
{
    qDebug() << "IpcClient connected.";

    emit connected();
}

void IpcClient::onDisconnected()
{
    qDebug() << "IpcClient disconnected.";

    m_recvBuffer.clear();

    emit disconnected();
}

void IpcClient::onReadyRead()
{
    QByteArray data = m_socket->readAll();

    if (data.isEmpty()) {
        return;
    }

    qDebug() << "IpcClient recv raw:" << data;

    processReceivedData(data);
}

void IpcClient::onErrorOccurred(QLocalSocket::LocalSocketError socketError)
{
    Q_UNUSED(socketError);

    QString err = m_socket->errorString();

    qDebug() << "IpcClient error:" << err;

    emit errorOccured(err);
}

void IpcClient::processReceivedData(const QByteArray &data)
{
    m_recvBuffer.append(data);

    const int maxBufferSize = 1024 * 1024;

    if (m_recvBuffer.size() > maxBufferSize) {
        m_recvBuffer.clear();
        emit errorOccured("IPC recv buffer overflow.");
        return;
    }

    while (true) {
        int pos = m_recvBuffer.indexOf('\n');

        if (pos < 0) {
            break;
        }

        QByteArray frame = m_recvBuffer.left(pos);
        m_recvBuffer.remove(0, pos + 1);

        if (frame.endsWith('\r')) {
            frame.chop(1);
        }

        if (frame.isEmpty()) {
            continue;
        }

        qDebug() << "IpcClient recv frame:" << frame;

        emit messageReceived(frame);
    }
}