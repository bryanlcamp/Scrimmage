//////////////////////////////////////////////////////////////////////////////
// Project  : Scrimmage
// Library  : Networking
// Purpose  : RAII TCP client for low-latency HFT applications
// Author   : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "../core/CpuPause.h"

namespace scrimmage::networking {

class TcpClient {
public:
    TcpClient(const std::string& host,
              uint16_t port,
              bool enableNoDelay = true,
              size_t sendBufferSize = 0,
              size_t recvBufferSize = 0) {
        _host = host;
        _port = port;

        // create socket
        _socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_socket < 0) {
            throw std::runtime_error("TCP socket creation failed: " + std::string(strerror(errno)));
        }

        // setup server address
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);

        if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) != 1) {
            ::close(_socket);
            throw std::runtime_error("Invalid host address: " + _host);
        }

        // connect
        if (::connect(_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(_socket);
            throw std::runtime_error("Failed to connect to " + _host + ":" + std::to_string(_port) +
                                     " : " + std::string(strerror(errno)));
        }

        // disable Nagle
        if (enableNoDelay) {
            int flag = 1;
            ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        }

        // optional socket buffers
        if (sendBufferSize > 0) {
            int sz = static_cast<int>(sendBufferSize);
            ::setsockopt(_socket, SOL_SOCKET, SO_SNDBUF, &sz, sizeof(sz));
        }
        if (recvBufferSize > 0) {
            int sz = static_cast<int>(recvBufferSize);
            ::setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, &sz, sizeof(sz));
        }
    }

    ~TcpClient() {
        if (_socket >= 0) {
            ::close(_socket);
        }
    }

    // no copy/move
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = delete;
    TcpClient& operator=(TcpClient&&) = delete;

    // hot-path send
    [[nodiscard]] inline ssize_t send(const void* data, size_t len) noexcept {
        return ::send(_socket, data, len, 0);
    }

    [[nodiscard]] bool sendAll(const void* data, size_t len) noexcept {
        const char* ptr = static_cast<const char*>(data);
        size_t remaining = len;

        while (remaining > 0) {
            ssize_t n = ::send(_socket, ptr, remaining, 0);
            if (n <= 0) return false;
            ptr += n;
            remaining--;
        }

        return true;
    }

    [[nodiscard]] inline ssize_t recv(void* buffer, size_t maxLen) noexcept {
        return ::recv(_socket, buffer, maxLen, 0);
    }

    [[nodiscard]] bool isConnected() const noexcept { return _socket >= 0; }
    [[nodiscard]] int fd() const noexcept { return _socket; }
    [[nodiscard]] const std::string& host() const noexcept { return _host; }
    [[nodiscard]] uint16_t port() const noexcept { return _port; }

private:
    int _socket{-1};
    std::string _host;
    uint16_t _port{0};
};

} // namespace scrimmage::networking