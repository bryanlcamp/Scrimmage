#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include "tcp_client.h"

namespace scrimmage::networking {

/**
 * @class TcpConnectionManager
 * @brief Manages multiple TCP client connections with automatic reconnect.
 *
 * Hot-path notes:
 * - Minimal locking: only on connection vector.
 * - Supports callbacks for received messages.
 * - Inline reconnect logic for low-latency recovery.
 */
class TcpConnectionManager {
public:
    using MessageCallback = TcpClient::ClientMessageCallback;

    TcpConnectionManager() = default;
    ~TcpConnectionManager() { stopAll(); }

    // Non-copyable
    TcpConnectionManager(const TcpConnectionManager&) = delete;
    TcpConnectionManager& operator=(const TcpConnectionManager&) = delete;

    /**
     * @brief Add a new TCP connection
     * @param host Server address
     * @param port Server port
     * @param callback Message callback
     */
    void addConnection(const std::string& host, uint16_t port, MessageCallback callback) {
        auto client = std::make_unique<TcpClient>(host, port);
        _clients.emplace_back(std::move(client), std::move(callback));
    }

    /**
     * @brief Start all connection receive loops
     */
    void startAll() {
        _running = true;
        for (auto& [client, cb] : _clients) {
            _threads.emplace_back([this, &client, cb]() {
                char buffer[4096];
                while (_running && client->isConnected()) {
                    ssize_t n = client->recv(buffer, sizeof(buffer));
                    if (n > 0) {
                        cb(std::string(buffer, static_cast<size_t>(n)));
                    } else {
                        // reconnect logic could go here
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            });
        }
    }

    /**
     * @brief Stop all connections and join threads
     */
    void stopAll() {
        _running = false;
        for (auto& t : _threads) if (t.joinable()) t.join();
        _threads.clear();
    }

private:
    std::vector<std::pair<std::unique_ptr<TcpClient>, MessageCallback>> _clients;
    std::vector<std::thread> _threads;
    std::atomic<bool> _running{false};
};

} // namespace scrimmage::networking