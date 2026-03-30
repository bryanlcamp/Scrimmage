//////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : Networking
// Component   : Heartbeat
// Description : RAII-managed TCP heartbeat sender with nanosecond precision
// Author      : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

namespace scrimmage::networking {

//////////////////////////////////////////////////////////////////////////////
// Class: TcpHeartbeat
// Purpose: Sends periodic heartbeat messages using a user-supplied callable.
// Hot-path optimizations:
// - Minimal overhead per tick
// - Separate thread for deterministic timing
// - Stop flag is relaxed atomic
// - Nanosecond precision maintained for intervals
//////////////////////////////////////////////////////////////////////////////
class TcpHeartbeat {
public:
    using HeartbeatFn = std::function<void()>;

    // Constructor: accepts heartbeat callable and interval duration
    TcpHeartbeat(HeartbeatFn heartbeatFunction,
                 std::chrono::nanoseconds intervalDuration) 
        : _heartbeatFunction(heartbeatFunction),
          _interval(intervalDuration),
          _running(false)
    {
    }

    // Non-copyable
    TcpHeartbeat(const TcpHeartbeat&) = delete;
    TcpHeartbeat& operator=(const TcpHeartbeat&) = delete;

    // Non-movable
    TcpHeartbeat(TcpHeartbeat&&) = delete;
    TcpHeartbeat& operator=(TcpHeartbeat&&) = delete;

    // Destructor: stops heartbeat thread if running
    ~TcpHeartbeat {
        stop();
    }

    // Start heartbeat loop in dedicated thread
    void start {
        if (_running.load(std::memory_order_relaxed)) return;

        _running.store(true, std::memory_order_release);

        _thread = std::thread([this] {
            while (_running.load(std::memory_order_acquire)) {
                auto startTime = std::chrono::steady_clock::now();
                _heartbeatFunction();
                auto endTime = std::chrono::steady_clock::now();
                auto elapsed = endTime - startTime;

                if (elapsed < _interval) {
                    std::this_thread::sleep_for(_interval - elapsed);
                }
            }
        });
    }

    // Stop heartbeat loop and join thread
    void stop noexcept {
        _running.store(false, std::memory_order_release);

        if (_thread.joinable()) {
            _thread.join();
        }
    }

private:
    HeartbeatFn _heartbeatFunction;               // User-supplied heartbeat action
    std::chrono::nanoseconds _interval;           // Heartbeat interval
    std::thread _thread;                          // Worker thread
    std::atomic<bool> _running;                   // Stop flag
};

} // namespace scrimmage::networking