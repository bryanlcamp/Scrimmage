#pragma once

#include <string>
#include <stdexcept>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

namespace scrimmage::networking {

/**
 * @class UdpMulticastSender
 * @brief Tier 1 HFT multicast sender for synthetic ticks or market events.
 *
 * Features:
 * - RAII socket
 * - Inline hot-path send()
 * - Configurable TTL and loopback
 * - Zero-copy design
 */
class UdpMulticastSender {
public:
    UdpMulticastSender(const std::string& multicastAddr,
                       uint16_t port,
                       uint8_t ttl = 1)
        : _multicastAddr(multicastAddr), _port(port)
    {
        _socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (_socket < 0)
            throw std::runtime_error("UDP socket creation failed: " + std::string(strerror(errno)));

        // TTL
        if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0)
            throw std::runtime_error("Failed to set multicast TTL");

        // Loopback
        unsigned char loopback = 1;
        if (setsockopt(_socket, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(loopback)) < 0)
            throw std::runtime_error("Failed to enable multicast loopback");

        std::memset(&_destAddr, 0, sizeof(_destAddr));
        _destAddr.sin_family = AF_INET;
        _destAddr.sin_port = htons(_port);
        if (inet_pton(AF_INET, multicastAddr.c_str(), &_destAddr.sin_addr) <= 0)
            throw std::runtime_error("Invalid multicast address: " + multicastAddr);
    }

    ~UdpMulticastSender() { if (_socket >= 0) close(_socket); }

    UdpMulticastSender(const UdpMulticastSender&) = delete;
    UdpMulticastSender& operator=(const UdpMulticastSender&) = delete;
    UdpMulticastSender(UdpMulticastSender&&) = delete;
    UdpMulticastSender& operator=(UdpMulticastSender&&) = delete;

    [[nodiscard]] inline ssize_t send(const void* data, size_t len) noexcept {
        return ::sendto(_socket, data, len, 0,
                        reinterpret_cast<const sockaddr*>(&_destAddr), sizeof(_destAddr));
    }

    [[nodiscard]] int fd() const noexcept { return _socket; }
    [[nodiscard]] const std::string& multicastAddress() const noexcept { return _multicastAddr; }
    [[nodiscard]] uint16_t port() const noexcept { return _port; }

private:
    int _socket = -1;
    std::string _multicastAddr;
    uint16_t _port;
    sockaddr_in _destAddr{};
};

} // namespace scrimmage::networking