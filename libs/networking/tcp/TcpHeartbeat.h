//////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : Networking
// Component   : Heartbeat
// Description : Tier1 RAII-managed TCP heartbeat sender with nanosecond precision
// Author      : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include "../concurrency/cpu_pause.h"

namespace scrimmage::networking {

//////////////////////////////////////////////////////////////////////////////
// Class: TcpHeartbeat
// Purpose: Sends periodic heartbeat messages using a template callable.
// Hot-path optimizations:
// - Fully inlineable, no virtual calls
// - Separate thread for deterministic timing
// - Cache-aligned stop flag
// - Nanosecond precision using spin-wait + sleep
//////////////////////////////////////////////////////////////////////////////
template <typename HeartbeatCallable>
class TcpHeartbeat {
public:
    // Constructor: accepts heartbeat callable and interval duration
    inline TcpHeartbeat(HeartbeatCallable heartbeatFunction,
                        std::chrono::nanoseconds intervalDuration) noexcept
        : _heartbeatFunction(heartbeatFunction),
          _interval(intervalDuration),
          _running(false)
    {
    }

    // Non-copyable, non-movable
    TcpHeartbeat(const TcpHeartbeat&) = delete;
    TcpHeartbeat& operator=(const TcpHeartbeat&) = delete;
    TcpHeartbeat(TcpHeartbeat&&) = delete;
    TcpHeartbeat& operator=(TcpHeartbeat&&) = delete;

    // Destructor stops heartbeat if running
    inline ~TcpHeartbeat() {
        stop();
    }

    // Start heartbeat loop in dedicated thread
    inline void start() noexcept {
        if (_running.load(std::memory_order_relaxed)) {
            return;
        }

        _running.store(true, std::memory_order_release);

        _thread = std::thread([this] {
            while (_running.load(std::memory_order_acquire)) {
                const auto loopStart = std::chrono::steady_clock::now();

                // Hot-path heartbeat
                _heartbeatFunction();

                const auto loopEnd = std::chrono::steady_clock::now();
                const auto elapsed = loopEnd - loopStart;

                if (elapsed < _interval) {
                    auto remaining = _interval - elapsed;

                    // Spin-wait for sub-microsecond precision
                    if (remaining < std::chrono::microseconds(50)) {
                        const auto target = loopEnd + remaining;
                        while (std::chrono::steady_clock::now() < target) {
                            scrimmage::core::cpuPause();
                        }
                    } else {
                        // Sleep for bulk of remaining interval
                        std::this_thread::sleep_for(remaining - std::chrono::microseconds(50));

                        // Fine-grained spin
                        const auto target = std::chrono::steady_clock::now() + std::chrono::microseconds(50);
                        while (std::chrono::steady_clock::now() < target) {
                            scrimmage::core::cpuPause();
                        }
                    }
                }
            }
        });
    }

    // Stop heartbeat loop and join thread
    inline void stop() noexcept {
        _running.store(false, std::memory_order_release);

        if (_thread.joinable()) {
            _thread.join();
        }
    }

    [[nodiscard]] inline bool isRunning() const noexcept {
        return _running.load(std::memory_order_acquire);
    }

private:
    HeartbeatCallable _heartbeatFunction;           // Template heartbeat callable
    std::chrono::nanoseconds _interval;            // Heartbeat interval
    std::thread _thread;                            // Worker thread

    alignas(64) std::atomic<bool> _running;        // Stop flag (cache aligned)
};

} // namespace scrimmage::networking