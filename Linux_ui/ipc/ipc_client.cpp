#include "ipc_client.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::readyRead,
            this, &IpcClient::onReadyRead);

    connect(m_socket, &QLocalSocket::connected,
            this, &IpcClient::connected);

    connect(m_socket, &QLocalSocket::disconnected,
            this, &IpcClient::disconnected);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QLocalSocket::errorOccurred,
            this, [this](QLocalSocket::LocalSocketError) {
                emit errorOccured(m_socket->errorString());
            });
#else
    connect(m_socket,
            QOverload<QLocalSocket::LocalSocketError>::of(&QLocalSocket::error),
            this, [this](QLocalSocket::LocalSocketError) {
                emit errorOccured(m_socket->errorString());
            });
#endif
}

IpcClient::~IpcClient()
{
    m_socket->disconnectFromServer();
}

bool IpcClient::connectToServer(const QString &path)
{
    if (m_socket->state() == QLocalSocket::ConnectedState)
        return true;

    const QString serverName = serverNameFromPath(path);
    m_socket->abort();
    m_socket->connectToServer(serverName);

    if (!m_socket->waitForConnected(3000)) {
        emit errorOccured(m_socket->errorString());
        return false;
    }

    qDebug() << "QLocalSocket connect success:" << serverName;
    return true;
}

bool IpcClient::sendMessage(const QByteArray &msg)
{
    if (m_socket->state() != QLocalSocket::ConnectedState)
        return false;

    QByteArray frame = msg;
    if (!frame.endsWith('\n'))
        frame.append('\n');

    const qint64 written = m_socket->write(frame);
    if (written != frame.size()) {
        emit errorOccured(m_socket->errorString());
        return false;
    }

    m_socket->flush();
    qDebug() << "发送 sendMessage";
    return true;
}

void IpcClient::onReadyRead()
{
    m_recvBuf.append(m_socket->readAll());

    while (true) {
        int newline = m_recvBuf.indexOf('\n');
        if (newline < 0)
            break;

        QByteArray frame = m_recvBuf.left(newline).trimmed();
        m_recvBuf.remove(0, newline + 1);
        if (frame.isEmpty())
            continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(frame, &err);
        if (err.error != QJsonParseError::NoError || !doc.isObject()) {
            emit errorOccured(err.errorString());
            continue;
        }

        QJsonObject root = doc.object();
        const QString msgType = root.value("type").toString();

        if (msgType == "ports") {
            QStringList ports;
            for (const auto &v : root.value("ports").toArray())
                ports.append(v.toString());
            emit portsUpdated(ports);
            continue;
        }

        if (msgType == "port_status") {
            emit portStatusUpdated(root.value("slot").toInt(),
                                   root.value("port").toString(),
                                   root.value("device_type").toString(),
                                   root.value("baud").toInt(),
                                   root.value("connected").toBool(),
                                   root.value("message").toString());
            continue;
        }

        DataPack pack;
        if (DataParser::parseJson(frame, pack))
            emit deviceStatusUpdated(pack);
    }
}

QString IpcClient::serverNameFromPath(const QString &path) const
{
#ifdef Q_OS_WIN
    return QFileInfo(path).fileName();
#else
    return path;
#endif
}
