#pragma once
#include <thread>
#include <atomic>
#include "spsc_ringbuffer.h"
#include "../cpu_pause.h"

namespace scrimmage::core::ringbuffer {

template<typename MsgType, typename Callback, size_t N=DEFAULT_RING_BUFFER_CAPACITY>
class SpScRingBufferConsumer {
public:
    SpScRingBufferConsumer(SpScRingBuffer<MsgType,N>& buffer, Callback callback)
        : _buffer(buffer), _callback(std::move(callback)), _stopFlag(false) {}

    void start(int core=-1) {
        _thread = std::thread([this]{ consumeLoop(); });
        if (core >= 0) pinThreadToCore(core);
    }

    void stop() noexcept {
        _stopFlag.store(true, std::memory_order_relaxed);
        if (_thread.joinable()) _thread.join();
    }

private:
    SpScRingBuffer<MsgType,N>& _buffer;
    Callback _callback;
    std::atomic<bool> _stopFlag;
    std::thread _thread;

    void consumeLoop() {
        MsgType msgs[32]; // Batch buffer
        size_t count = 0;
        MsgType msg{};
        while (!_stopFlag.load(std::memory_order_relaxed)) {
            if (_buffer.tryPop(msg)) {
                msgs[count++] = msg;
                if (count == 32) {
                    for (size_t i=0;i<count;++i) _callback(msgs[i]);
                    count = 0;
                }
            } else cpu_pause();
        }
        for (size_t i=0;i<count;++i) _callback(msgs[i]);
    }

    void pinThreadToCore(int core) {
        // platform-specific pinning
    }
};

} // namespace beacon::core::ringbuffer