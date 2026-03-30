#pragma once

#include <string>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

namespace scrimmage::networking {

/**
 * @class TcpClient
 * @brief RAII TCP client for low-latency HFT applications.
 *
 * Design goals:
 * - Zero-overhead inline send/recv wrappers
 * - RAII socket management
 * - TCP_NODELAY enabled by default
 * - Minimal branching in hot-path
 *
 * Usage:
 *   TcpClient client("127.0.0.1", 8080);
 *   client.send(order, sizeof(order));
 *   ssize_t n = client.recv(buffer, sizeof(buffer));
 */
class TcpClient {
public:
    /**
     * @brief Construct and connect to a TCP server
     * @param host Server IP or hostname
     * @param port Server port
     * @param enableNoDelay Disable Nagle's algorithm (default true)
     * @param sendBufferSize Optional send buffer size (0 = default)
     * @param recvBufferSize Optional receive buffer size (0 = default)
     * @throws std::runtime_error if connection fails
     */
    TcpClient(const std::string& host,
              uint16_t port,
              bool enableNoDelay = true,
              size_t sendBufferSize = 0,
              size_t recvBufferSize = 0)
        : _host(host), _port(port)
    {
        // --- create socket ---
        _socket = ::socket(AF_INET, SOCK_STREAM, 0);
        if (_socket < 0) {
            throw std::runtime_error("TCP socket creation failed: " + std::string(strerror(errno)));
        }

        // --- setup server address ---
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(_port);

        if (inet_pton(AF_INET, _host.c_str(), &addr.sin_addr) != 1) {
            ::close(_socket);
            throw std::runtime_error("Invalid host address: " + _host);
        }

        // --- connect ---
        if (::connect(_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            ::close(_socket);
            throw std::runtime_error("Failed to connect to " + _host + ":" + std::to_string(_port) +
                                     " : " + std::string(strerror(errno)));
        }

        // --- disable Nagle (TCP_NODELAY) ---
        if (enableNoDelay) {
            int flag = 1;
            ::setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
        }

        // --- optional socket buffer sizes ---
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

    // Non-copyable, non-movable
    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;
    TcpClient(TcpClient&&) = delete;
    TcpClient& operator=(TcpClient&&) = delete;

    /**
     * @brief Inline send (hot-path)
     * @param data Pointer to bytes
     * @param len Length of bytes
     * @return Bytes sent or -1 on error
     */
    [[nodiscard]] inline ssize_t send(const void* data, size_t len) noexcept {
        return ::send(_socket, data, len, 0);
    }

    /**
     * @brief Send all bytes (blocking)
     * @param data Pointer to bytes
     * @param len Length
     * @return true if all sent, false on error
     */
    [[nodiscard]] bool sendAll(const void* data, size_t len) noexcept {
        const char* ptr = static_cast<const char*>(data);
        size_t remaining = len;

        while (remaining > 0) {
            ssize_t n = ::send(_socket, ptr, remaining, 0);
            if (n <= 0) return false;
            ptr += n;
            remaining -= n;
        }
        return true;
    }

    /**
     * @brief Inline receive (blocking)
     * @param buffer Pointer to user buffer
     * @param maxLen Max bytes
     * @return Bytes received, 0 if closed, -1 on error
     */
    [[nodiscard]] inline ssize_t recv(void* buffer, size_t maxLen) noexcept {
        return ::recv(_socket, buffer, maxLen, 0);
    }

    /**
     * @brief Check if socket is valid (connected)
     */
    [[nodiscard]] bool isConnected() const noexcept { return _socket >= 0; }

    /**
     * @brief Underlying socket FD (advanced use)
     */
    [[nodiscard]] int fd() const noexcept { return _socket; }

    /**
     * @brief Server host
     */
    [[nodiscard]] const std::string& host() const noexcept { return _host; }

    /**
     * @brief Server port
     */
    [[nodiscard]] uint16_t port() const noexcept { return _port; }

private:
    int _socket{-1};
    std::string _host;
    uint16_t _port;
};

} // namespace scrimmage::networking