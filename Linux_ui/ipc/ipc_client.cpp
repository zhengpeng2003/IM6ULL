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

bool IpcClient::connectToServer(const QString &path, int timeoutMs)
{
    if (m_socket->state() == QLocalSocket::ConnectedState)
        return true;

    const QString serverName = serverNameFromPath(path);
    m_socket->abort();
    m_socket->connectToServer(serverName);

    if (!m_socket->waitForConnected(timeoutMs)) {
        emit errorOccured(m_socket->errorString());
        return false;
    }

    qDebug() << "QLocalSocket connect success:" << serverName;
    return true;
}

bool IpcClient::isConnected() const
{
    return m_socket->state() == QLocalSocket::ConnectedState;
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

        if (msgType == "ack") {
            const QString cmd = root.value("cmd").toString();
            const QString status = root.value("status").toString();
            const QString reason = root.value("reason").toString();
            const QString message = root.value("message").toString();

            if ((cmd == "connect_port" || cmd == "disconnect_port") && root.contains("slot")) {
                emit portStatusUpdated(root.value("slot").toInt(),
                                       root.value("port").toString(),
                                       root.value("device_type").toString(),
                                       root.value("baud").toInt(),
                                       root.value("connected").toBool(),
                                       status == "ok" ? message : reason);
            }

            emit commandAckReceived(
                static_cast<quint32>(root.value("seq").toVariant().toULongLong()),
                cmd,
                status,
                reason,
                message);

            if (cmd == "scan_ports") {
                QStringList ports;
                for (const auto &v : root.value("ports").toArray())
                    ports.append(v.toString());
                emit portsUpdated(ports);
            } else if ((cmd == "get_alarm_config" || cmd == "set_alarm_config") &&
                       root.contains("temp_high") &&
                       root.contains("humi_high")) {
                emit alarmConfigReceived(root.value("temp_high").toDouble(),
                                         root.value("humi_high").toDouble());
            }
            continue;
        }

        if (msgType == "ports") {
            QStringList ports;
            for (const auto &v : root.value("ports").toArray())
                ports.append(v.toString());
            emit portsUpdated(ports);
            continue;
        }

        if (msgType == "runtime_state") {
            QStringList ports;
            const QJsonArray portArray = root.value("ports").toArray();
            for (const auto &portValue : portArray) {
                const QJsonObject portObj = portValue.toObject();
                const int slot = portObj.value("slot").toInt();
                const QString port = portObj.value("port").toString();
                const int baud = portObj.value("baud").toInt();
                const bool connected = portObj.value("connected").toBool();
                if (port.isEmpty() && !connected)
                    continue;
                if (!port.isEmpty())
                    ports.append(port);

                emit portStatusUpdated(slot,
                                       port,
                                       QStringLiteral("unknown"),
                                       baud,
                                       connected,
                                       connected ? QStringLiteral("restored") : QStringLiteral("disconnected"));

                const QJsonArray devices = portObj.value("devices").toArray();
                for (const auto &deviceValue : devices) {
                    const QJsonObject deviceObj = deviceValue.toObject();
                    emit deviceRegistered(
                        static_cast<quint32>(root.value("seq").toVariant().toULongLong()),
                        slot,
                        deviceObj.value("deviceId").toInt(),
                        deviceObj.value("deviceName").toString(),
                        deviceObj.value("deviceType").toString(),
                        deviceObj.value("pollIntervalMs").toInt(1000));
                }
            }

            emit portsUpdated(ports);
            continue;
        }

        if ((msgType == "command" && root.value("cmd").toString() == "port_status") ||
            msgType == "port_status") {
            emit portStatusUpdated(root.value("slot").toInt(),
                                   root.value("port").toString(),
                                   root.value("device_type").toString(),
                                   root.value("baud").toInt(),
                                   root.value("connected").toBool(),
                                   root.value("message").toString());
            continue;
        }

        if (msgType == "alarm_config") {
            emit alarmConfigReceived(root.value("temp_high").toDouble(),
                                     root.value("humi_high").toDouble());
            continue;
        }

        if (msgType == "device_register") {
            emit deviceRegistered(
                static_cast<quint32>(root.value("seq").toVariant().toULongLong()),
                root.value("slot").toInt(),
                root.value("deviceId").toInt(root.value("slave_id").toInt()),
                root.value("deviceName").toString(),
                root.value("deviceType").toString(),
                root.value("pollIntervalMs").toInt(1000));
            continue;
        }

        if (msgType == "command" && root.value("cmd").toString() == "emergency") {
            emit emergencyReceived(root.value("level").toInt(),
                                   root.value("reason").toString(),
                                   root.value("deviceId").toInt(),
                                   root.value("deviceType").toString(),
                                   root.value("pointKey").toString(),
                                   root.value("value").toDouble(),
                                   root.value("threshold").toDouble(),
                                   root.value("temp").toDouble(),
                                   root.value("humi").toDouble());
            continue;
        }

        DataPack pack;
        if (DataParser::parseJson(frame, pack)) {
            emit deviceStatusUpdated(pack);
            emit devicetrend(pack);
            emit deviceinfo(pack);
        }
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
