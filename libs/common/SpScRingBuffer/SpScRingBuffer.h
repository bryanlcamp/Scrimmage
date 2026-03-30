//////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : common
// Purpose     : Cache-line optimized single-producer single-consumer ring buffer
// Author      : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <cstddef>
#include <array>
#include <algorithm>
#include <chrono>
#include "../concurrency/cpu_pause.h"

namespace scrimmage::common {

// Default compile-time capacity
inline constexpr int DEFAULT_RING_BUFFER_CAPACITY = 1024;

// Lock-free SPSC ring buffer
template <typename T, size_t Capacity = DEFAULT_RING_BUFFER_CAPACITY>
class SpScRingBuffer {
public:
    SpScRingBuffer() noexcept
        : _head(0)
        , _tail(0)
        , _dropped(0)
        , _highWaterMark(0)
    {}

    SpScRingBuffer(const SpScRingBuffer&) = delete;
    SpScRingBuffer& operator=(const SpScRingBuffer&) = delete;
    SpScRingBuffer(SpScRingBuffer&&) = delete;
    SpScRingBuffer& operator=(SpScRingBuffer&&) = delete;

    // Non-blocking push, returns false if full
    bool tryPush(const T& item) noexcept {
        size_t currentHead = _head.load(std::memory_order_relaxed);
        size_t nextHead = increment(currentHead);

        if (nextHead == _tail.load(std::memory_order_acquire)) {
            _dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        _buffer[currentHead] = item;
        _head.store(nextHead, std::memory_order_release);

        updateHighWaterMark(nextHead);
        return true;
    }

    // Blocking push with timeout in milliseconds
    bool push(const T& item, uint32_t timeoutMs = 5000) noexcept {
        auto startTime = std::chrono::steady_clock::now();
        auto timeoutDuration = std::chrono::milliseconds(timeoutMs);
        uint64_t spinCount = 0;

        while (true) {
            size_t currentHead = _head.load(std::memory_order_relaxed);
            size_t nextHead = increment(currentHead);

            if (nextHead != _tail.load(std::memory_order_acquire)) {
                _buffer[currentHead] = item;
                _head.store(nextHead, std::memory_order_release);
                updateHighWaterMark(nextHead);
                return true;
            }

            if (spinCount++ % 1000 == 0) {
                auto elapsed = std::chrono::steady_clock::now() - startTime;
                if (elapsed >= timeoutDuration) {
                    return false;
                }
            }

            scrimmage::core::concurrency::cpuPause();
        }
    }

    bool pushWithTimeout(const T& item, uint32_t timeoutMs) noexcept {
        return push(item, timeoutMs);
    }

    bool tryPop(T& item) noexcept {
        size_t currentTail = _tail.load(std::memory_order_relaxed);
        if (currentTail == _head.load(std::memory_order_acquire)) {
            return false;
        }

        item = _buffer[currentTail];
        _tail.store(increment(currentTail), std::memory_order_release);
        return true;
    }

    size_t dropped() const noexcept {
        return _dropped.load(std::memory_order_relaxed);
    }

    size_t highWaterMark() const noexcept {
        return _highWaterMark.load(std::memory_order_relaxed);
    }

private:
    std::array<T, Capacity> _buffer;
    alignas(64) std::atomic<size_t> _head;
    alignas(64) std::atomic<size_t> _tail;
    std::atomic<size_t> _dropped;
    std::atomic<size_t> _highWaterMark;

    size_t increment(size_t currentIndex) const noexcept {
        return (currentIndex + 1 == Capacity) ? 0 : currentIndex + 1;
    }

    void updateHighWaterMark(size_t currentHead) noexcept {
        size_t currentHigh = _highWaterMark.load(std::memory_order_relaxed);
        size_t used = (currentHead + Capacity - _tail.load(std::memory_order_relaxed)) % Capacity;

        while (used > currentHigh && !_highWaterMark.compare_exchange_weak(currentHigh, used, std::memory_order_relaxed)) {
        }
    }
};

} // namespace scrimmage::common