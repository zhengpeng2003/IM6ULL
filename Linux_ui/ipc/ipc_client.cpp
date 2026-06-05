#include "ipc_client.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#include <QDebug>

#define IPC_SOCKET_PATH "/tmp/device_ipc.sock"

IpcClient::IpcClient(QObject *parent)
    : QObject(parent)
    , m_fd(-1)
    , m_notifier(nullptr)
{

}

IpcClient::~IpcClient()
{
    if (m_fd != -1) close(m_fd);
}

bool IpcClient::connectToServer(const QString &path)
{
    if (m_fd != -1) return true;

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        emit errorOccured("socket() failed");
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path.toUtf8().data(), sizeof(addr.sun_path) - 1);

    // ❗ 关键：加 :: 让它调用系统 connect()，而不是 QObject::connect()
    if (::connect(m_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        emit errorOccured("connect() failed");
        close(m_fd);
        m_fd = -1;
        return false;
    }
    qDebug()<<"connect() success";
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &IpcClient::onReadyRead);
    emit connected();
    return true;
}

bool IpcClient::sendMessage(const QByteArray &msg)
{
    if (m_fd < 0) return false;
    ::send(m_fd, msg.constData(), msg.size(), 0);
    qDebug() << "发送 sendMessage ";
    return true;
}


void IpcClient::onReadyRead()
{
    char buf[1024];
    int n = ::recv(m_fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        emit disconnected();
        close(m_fd);
        m_fd = -1;
        return;
    }

    // 累积数据到缓存
    m_recvBuf.append(buf, n);

    // 尝试用 DataParser 解析
    DataPack pack;
    DataParser parser;
    if (parser.parseJson(m_recvBuf, pack)) {
        // ✅ 成功解析，发信号给外部使用
        //qDebug()<<"✅ 成功解析，发信号给外部使用";
        emit deviceStatusUpdated(pack);
        // 清空缓存
        m_recvBuf.clear();
    }
    else {
        // 解析失败：可能数据还没收完整，等待下一次 recv
        // 不清空缓存
    }
}
