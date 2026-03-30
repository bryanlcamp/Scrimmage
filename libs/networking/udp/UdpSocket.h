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
 * @class UdpSocket
 * @brief Tier 1 HFT general-purpose UDP socket.
 *
 * Features:
 * - Unicast or multicast
 * - Inline send/recv
 * - RAII socket
 */
class UdpSocket {
public:
    UdpSocket(const std::string& address, uint16_t port)
        : _address(address), _port(port)
    {
        _socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (_socket < 0)
            throw std::runtime_error("UDP socket creation failed: " + std::string(strerror(errno)));

        std::memset(&_destAddr, 0, sizeof(_destAddr));
        _destAddr.sin_family = AF_INET;
        _destAddr.sin_port = htons(_port);
        if (inet_pton(AF_INET, _address.c_str(), &_destAddr.sin_addr) <= 0)
            throw std::runtime_error("Invalid UDP address: " + _address);
    }

    ~UdpSocket() { if (_socket >= 0) close(_socket); }

    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;
    UdpSocket(UdpSocket&&) = delete;
    UdpSocket& operator=(UdpSocket&&) = delete;

    [[nodiscard]] inline ssize_t send(const void* data, size_t len) noexcept {
        return ::sendto(_socket, data, len, 0,
                        reinterpret_cast<const sockaddr*>(&_destAddr), sizeof(_destAddr));
    }

    [[nodiscard]] inline ssize_t recv(void* buffer, size_t len, sockaddr_in* srcAddr = nullptr) noexcept {
        socklen_t addrLen = sizeof(sockaddr_in);
        sockaddr_in tmpAddr{};
        sockaddr_in* addrPtr = srcAddr ? srcAddr : &tmpAddr;
        return ::recvfrom(_socket, buffer, len, 0, reinterpret_cast<sockaddr*>(addrPtr), &addrLen);
    }

    [[nodiscard]] int fd() const noexcept { return _socket; }
    [[nodiscard]] const std::string& address() const noexcept { return _address; }
    [[nodiscard]] uint16_t port() const noexcept { return _port; }

private:
    int _socket = -1;
    std::string _address;
    uint16_t _port;
    sockaddr_in _destAddr{};
};

} // namespace scrimmage::networking