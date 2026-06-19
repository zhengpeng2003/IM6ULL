#include "IpcServer.hpp"

#include <iostream>

IpcServer::IpcServer(const std::string &pipeName)
    : m_pipeName(pipeName)
{
}

IpcServer::~IpcServer()
{
    stop();
}

bool IpcServer::start()
{
#ifndef _WIN32
    std::cerr << "This IpcServer only supports Windows Named Pipe." << std::endl;
    return false;
#else
    if (m_running) {
        return true;
    }

    m_pipeHandle = createPipeInstance();
    if (m_pipeHandle == INVALID_HANDLE_VALUE) {
        return false;
    }

    m_running = true;
    m_thread = std::thread(&IpcServer::loop, this);

    std::cout << "IPC server listening: " << m_pipeName << std::endl;
    return true;
#endif
}

void IpcServer::stop()
{
#ifdef _WIN32
    if (!m_running) {
        return;
    }

    m_running = false;

    closeClient();

    if (m_thread.joinable()) {
        m_thread.join();
    }

    std::cout << "IPC server stopped" << std::endl;
#endif
}

void IpcServer::loop()
{
#ifdef _WIN32
    while (m_running) {
        if (m_pipeHandle == INVALID_HANDLE_VALUE) {
            m_pipeHandle = createPipeInstance();
            if (m_pipeHandle == INVALID_HANDLE_VALUE) {
                Sleep(1000);
                continue;
            }
        }

        std::cout << "IPC waiting for client..." << std::endl;

        BOOL connected = ConnectNamedPipe(m_pipeHandle, nullptr)
                             ? TRUE
                             : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            std::cerr << "ConnectNamedPipe failed, error=" << GetLastError() << std::endl;
            CloseHandle(m_pipeHandle);
            m_pipeHandle = INVALID_HANDLE_VALUE;
            Sleep(1000);
            continue;
        }

        m_clientConnected = true;
        m_recvBuffer.clear();

        std::cout << "IPC client connected" << std::endl;

        if (m_clientConnectedCallback) {
            m_clientConnectedCallback();
        }

        char buf[1024];

        while (m_running && m_clientConnected) {
            DWORD bytesRead = 0;

            BOOL ok = ReadFile(
                m_pipeHandle,
                buf,
                sizeof(buf),
                &bytesRead,
                nullptr
                );

            if (!ok || bytesRead == 0) {
                DWORD err = GetLastError();

                if (err == ERROR_BROKEN_PIPE ||
                    err == ERROR_PIPE_NOT_CONNECTED ||
                    err == ERROR_NO_DATA) {
                    std::cout << "IPC client disconnected" << std::endl;
                } else {
                    std::cerr << "ReadFile failed, error=" << err << std::endl;
                }

                closeClient();

                if (m_clientDisconnectedCallback) {
                    m_clientDisconnectedCallback();
                }

                break;
            }

            processReceivedData(buf, static_cast<int>(bytesRead));
        }

        closeClient();

        if (m_running) {
            m_pipeHandle = createPipeInstance();
            if (m_pipeHandle == INVALID_HANDLE_VALUE) {
                Sleep(1000);
            }
        }
    }
#endif
}

#ifdef _WIN32
HANDLE IpcServer::createPipeInstance()
{
    HANDLE pipeHandle = CreateNamedPipeA(
        m_pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        4096,
        4096,
        0,
        nullptr
        );

    if (pipeHandle == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_PIPE_BUSY) {
            std::cerr << "CreateNamedPipe failed: pipe is busy. Another Pc_data may already be running. error="
                      << err << std::endl;
        } else {
            std::cerr << "CreateNamedPipe failed, error=" << err << std::endl;
        }
    }

    return pipeHandle;
}
#endif

void IpcServer::processReceivedData(const char *buf, int len)
{
    if (len <= 0) {
        return;
    }

    m_recvBuffer.append(buf, len);

    const size_t maxBufferSize = 1024 * 1024;
    if (m_recvBuffer.size() > maxBufferSize) {
        std::cerr << "IPC recv buffer overflow, clear buffer" << std::endl;
        m_recvBuffer.clear();
        return;
    }

    while (true) {
        size_t pos = m_recvBuffer.find('\n');

        if (pos == std::string::npos) {
            break;
        }

        std::string msg = m_recvBuffer.substr(0, pos);
        m_recvBuffer.erase(0, pos + 1);

        if (!msg.empty() && msg.back() == '\r') {
            msg.pop_back();
        }

        if (msg.empty()) {
            continue;
        }

        std::cout << "IPC recv: " << msg << std::endl;

        if (m_messageCallback) {
            m_messageCallback(msg);
        }
    }
}

bool IpcServer::sendMessage(const std::string &msg)
{
#ifdef _WIN32
    if (!m_clientConnected || m_pipeHandle == INVALID_HANDLE_VALUE) {
        std::cout << "[DBG_IPC_SEND] skip send clientConnected="
                  << (m_clientConnected ? "true" : "false")
                  << " pipeInvalid="
                  << (m_pipeHandle == INVALID_HANDLE_VALUE ? "true" : "false")
                  << " payloadBytes=" << msg.size() << std::endl;
        return false;
    }

    std::lock_guard<std::mutex> lock(m_sendMutex);

    std::string data = msg;
    data += '\n';

    DWORD bytesWritten = 0;

    BOOL ok = WriteFile(
        m_pipeHandle,
        data.c_str(),
        static_cast<DWORD>(data.size()),
        &bytesWritten,
        nullptr
        );

    if (!ok) {
        const DWORD error = GetLastError();
        std::cerr << "[DBG_IPC_SEND] WriteFile failed error=" << error
                  << " requestedBytes=" << data.size()
                  << " clientConnected=" << (m_clientConnected ? "true" : "false")
                  << std::endl;
        m_clientConnected = false;
        return false;
    }

    const bool complete = bytesWritten == data.size();
    std::cout << "[DBG_IPC_SEND] WriteFile ok="
              << (complete ? "true" : "false")
              << " requestedBytes=" << data.size()
              << " writtenBytes=" << bytesWritten
              << " clientConnected=" << (m_clientConnected ? "true" : "false")
              << std::endl;
    return complete;

#else
    (void)msg;
    std::cout << "[DBG_IPC_SEND] send skipped non_windows" << std::endl;
    return false;
#endif
}

void IpcServer::closeClient()
{
#ifdef _WIN32
    m_clientConnected = false;

    if (m_pipeHandle != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_pipeHandle);
        CloseHandle(m_pipeHandle);
        m_pipeHandle = INVALID_HANDLE_VALUE;
    }

    m_recvBuffer.clear();
#endif
}

bool IpcServer::hasClient() const
{
    return m_clientConnected;
}

void IpcServer::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void IpcServer::setClientConnectedCallback(ClientCallback cb)
{
    m_clientConnectedCallback = std::move(cb);
}

void IpcServer::setClientDisconnectedCallback(ClientCallback cb)
{
    m_clientDisconnectedCallback = std::move(cb);
}
