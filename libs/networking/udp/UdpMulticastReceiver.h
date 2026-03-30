#pragma once

#include <cstring>
#include <string>
#include <stdexcept>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

namespace scrimmage::networking {

/**
 * @class UdpMulticastReceiver
 * @brief Tier 1 HFT multicast receiver for market data feeds.
 *
 * Features:
 * - RAII socket management
 * - Inline hot-path recv()
 * - Configurable receive buffer
 * - Zero-copy, minimal instructions between NIC and user buffer
 */
class UdpMulticastReceiver {
public:
    UdpMulticastReceiver(const std::string& multicastAddr,
                         uint16_t port,
                         size_t recvBufferSize = 2 * 1024 * 1024)
        : _multicastAddr(multicastAddr), _port(port)
    {
        _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (_socket < 0)
            throw std::runtime_error("UDP socket creation failed: " + std::string(strerror(errno)));

        int reuse = 1;
        if (setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
            throw std::runtime_error("Failed to set SO_REUSEADDR");

#ifdef SO_REUSEPORT
        setsockopt(_socket, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

        int bufSize = static_cast<int>(recvBufferSize);
        setsockopt(_socket, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);

        if (bind(_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
            throw std::runtime_error("Bind failed on port " + std::to_string(_port));

        ip_mreq mreq{};
        if (inet_pton(AF_INET, multicastAddr.c_str(), &mreq.imr_multiaddr) != 1)
            throw std::runtime_error("Invalid multicast address: " + multicastAddr);
        mreq.imr_interface.s_addr = INADDR_ANY;

        if (setsockopt(_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0)
            throw std::runtime_error("Failed to join multicast group: " + multicastAddr);
    }

    ~UdpMulticastReceiver() { if (_socket >= 0) close(_socket); }

    UdpMulticastReceiver(const UdpMulticastReceiver&) = delete;
    UdpMulticastReceiver& operator=(const UdpMulticastReceiver&) = delete;
    UdpMulticastReceiver(UdpMulticastReceiver&&) = delete;
    UdpMulticastReceiver& operator=(UdpMulticastReceiver&&) = delete;

    [[nodiscard]] inline ssize_t recv(void* buffer, size_t maxLen) noexcept {
        return ::recv(_socket, buffer, maxLen, 0);
    }

    [[nodiscard]] int fd() const noexcept { return _socket; }
    [[nodiscard]] const std::string& multicastAddress() const noexcept { return _multicastAddr; }
    [[nodiscard]] uint16_t port() const noexcept { return _port; }

private:
    int _socket = -1;
    std::string _multicastAddr;
    uint16_t _port;
};

} // namespace scrimmage::networking