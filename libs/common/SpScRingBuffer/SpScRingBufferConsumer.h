//////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : common
// Purpose     : Threaded consumer for single-producer single-consumer ring buffer
// Author      : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <thread>
#include <utility>
#include "../concurrency/cpu_pause.h"
#include "../concurrency/pinned_thread.h"
#include "spsc_ringbuffer.h"

namespace scrimmage::core::ringbuffer {

// Threaded consumer for SpScRingBuffer
template <typename MessageType, typename Callback, size_t Capacity = DEFAULT_RING_BUFFER_CAPACITY>
class SpScRingBufferConsumer {
public:
    SpScRingBufferConsumer(SpScRingBuffer<MessageType, Capacity>& buffer, Callback callback) noexcept
        : _buffer(buffer)
        , _callback(std::move(callback))
        , _stopFlag(false)
    {}

    SpScRingBufferConsumer(const SpScRingBufferConsumer&) = delete;
    SpScRingBufferConsumer& operator=(const SpScRingBufferConsumer&) = delete;
    SpScRingBufferConsumer(SpScRingBufferConsumer&&) = delete;
    SpScRingBufferConsumer& operator=(SpScRingBufferConsumer&&) = delete;

    // Start consumer thread with optional CPU pinning
    void start(int cpuCore = PinnedThread::NO_CPU_PINNING) noexcept {
        _pinnedThread = std::make_unique<PinnedThread>([this](StopToken token) { consumeLoop(token); }, cpuCore);
    }

    // Stop consumer thread
    void stop() noexcept {
        if (_pinnedThread) {
            _pinnedThread->requestStop();
        }
    }

private:
    SpScRingBuffer<MessageType, Capacity>& _buffer;  // Reference to SPSC buffer
    Callback _callback;                              // Callback for popped messages
    std::atomic<bool> _stopFlag;                     // Stop signal for thread
    std::unique_ptr<PinnedThread> _pinnedThread;    // Thread wrapper

    void consumeLoop(StopToken token) noexcept {
        AdaptiveSpinner _spinner;
        MessageType _message{};
        while (!token.stopRequested()) {
            if (_buffer.tryPop(_message)) {
                _callback(_message);
                _spinner.reset();
            } else {
                _spinner.spin();
            }
        }
    }
};

// Factory function to simplify consumer creation
template <typename MessageType, size_t Capacity, typename Callback>
auto makeSpScRingBufferConsumer(SpScRingBuffer<MessageType, Capacity>& buffer, Callback&& callback) noexcept {
    return SpScRingBufferConsumer<MessageType, std::decay_t<Callback>, Capacity>(buffer, std::forward<Callback>(callback));
}

} // namespace scrimmage::common