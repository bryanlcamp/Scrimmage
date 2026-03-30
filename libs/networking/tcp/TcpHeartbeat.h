#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace beacon::networking {

/**
 * @class TcpHeartbeat
 * @brief Sends periodic heartbeat messages on a client connection
 *
 * Hot-path:
 * - Minimal overhead, separate thread
 * - Can be templated to support multiple client types
 */
class TcpHeartbeat {
public:
    using HeartbeatFn = std::function<void()>;

    TcpHeartbeat(HeartbeatFn fn, std::chrono::milliseconds interval)
        : _fn(fn), _interval(interval) {}

    ~TcpHeartbeat() { stop(); }

    TcpHeartbeat(const TcpHeartbeat&) = delete;
    TcpHeartbeat& operator=(const TcpHeartbeat&) = delete;

    void start() {
        _running = true;
        _thread = std::thread([this]() {
            while (_running) {
                std::this_thread::sleep_for(_interval);
                _fn();
            }
        });
    }

    void stop() {
        _running = false;
        if (_thread.joinable()) _thread.join();
    }

private:
    HeartbeatFn _fn;
    std::chrono::milliseconds _interval;
    std::thread _thread;
    std::atomic<bool> _running{false};
};

} // namespace beacon::networking