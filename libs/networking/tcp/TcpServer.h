//////////////////////////////////////////////////////////////////////////////
// Project  : Scrimmage
// Library  : Networking
// Purpose  : Generic low-latency TCP server with per-client threads
// Author   : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "../core/CpuPause.h"

namespace scrimmage::networking {

class TcpServer {
public:
    using ClientMessageCallback = std::function<void(const std::string&)>;

    TcpServer(uint16_t port, ClientMessageCallback callback, size_t backlog = 5) {
        _port = port;
        _callback = std::move(callback);
        _backlog = backlog;
    }

    ~TcpServer() { stop(); }

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    void start() {
        _running.store(true, std::memory_order_release);
        _acceptThread = std::thread(&TcpServer::listenLoop, this);
    }

    void stop() {
        _running.store(false, std::memory_order_release);

        if (_acceptThread.joinable())
            _acceptThread.join();

        for (size_t i = 0; i < _clientThreads.size(); i++) {
            if (_clientThreads[i].joinable())
                _clientThreads[i].join();
        }

        _clientThreads.clear();
    }

private:
    void listenLoop() noexcept {
        int serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd < 0) {
            std::cerr << "[TcpServer] Failed to create socket\n";
            return;
        }

        int enable = 1;
        ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);

        if (::bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::cerr << "[TcpServer] Bind failed\n";
            ::close(serverFd);
            return;
        }

        if (::listen(serverFd, static_cast<int>(_backlog)) < 0) {
            std::cerr << "[TcpServer] Listen failed\n";
            ::close(serverFd);
            return;
        }

        while (_running.load(std::memory_order_acquire)) {
            int clientFd = ::accept(serverFd, nullptr, nullptr);
            if (clientFd >= 0) {
                _clientThreads.emplace_back(&TcpServer::clientLoop, this, clientFd);
            } else {
                scrimmage::core::cpuPause();
            }
        }

        ::close(serverFd);
    }

    void clientLoop(int clientFd) noexcept {
        constexpr size_t BUFFER_SIZE = 1024;
        alignas(64) char _buffer[BUFFER_SIZE];

        while (_running.load(std::memory_order_acquire)) {
            ssize_t bytesRead = ::read(clientFd, _buffer, BUFFER_SIZE);
            if (bytesRead <= 0)
                break;

            std::string message(_buffer, static_cast<size_t>(bytesRead));
            _callback(message);
        }

        ::close(clientFd);
    }

private:
    uint16_t _port{0};
    size_t _backlog{0};
    ClientMessageCallback _callback;

    std::atomic<bool> _running{false};
    std::thread _acceptThread;
    std::vector<std::thread> _clientThreads;
};

} // namespace scrimmage::networking