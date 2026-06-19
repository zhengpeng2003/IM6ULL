#ifndef IPC_SERVER_HPP
#define IPC_SERVER_HPP

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

class IpcServer
{
public:
    using MessageCallback = std::function<void(const std::string &msg)>;
    using ClientCallback = std::function<void()>;

public:
    explicit IpcServer(const std::string &pipeName = R"(\\.\pipe\PcDataIpcPipe)");
    ~IpcServer();

    bool start();
    void stop();

    bool sendMessage(const std::string &msg);

    void setMessageCallback(MessageCallback cb);
    void setClientConnectedCallback(ClientCallback cb);
    void setClientDisconnectedCallback(ClientCallback cb);

    bool hasClient() const;

private:
    void loop();
#ifdef _WIN32
    HANDLE createPipeInstance();
#endif
    void closeClient();
    void processReceivedData(const char *buf, int len);

private:
    std::string m_pipeName;

#ifdef _WIN32
    HANDLE m_pipeHandle = INVALID_HANDLE_VALUE;
#endif

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_clientConnected{false};

    std::thread m_thread;
    std::mutex m_sendMutex;

    std::string m_recvBuffer;

    MessageCallback m_messageCallback;
    ClientCallback m_clientConnectedCallback;
    ClientCallback m_clientDisconnectedCallback;
};

#endif
