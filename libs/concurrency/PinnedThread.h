//////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : core::concurrency
// Purpose     : Ultra-low-latency thread utilities: PinnedThread, StopToken,
//               CPU pause, and adaptive spin for SpSc consumers.
// Author      : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <thread>
#include <cassert>
#include <cstddef>
#include <type_traits>
#include <chrono>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#elif defined(__APPLE__)
#include <pthread.h>
#include <mach/thread_policy.h>
#include <mach/mach.h>
#endif

namespace scrimmage::core::concurrency {

// Custom stop token for hot-path deterministic signaling
class StopToken {
public:
    explicit StopToken(const std::atomic<bool>* stopFlag) noexcept {
        _stopFlag = stopFlag;
    }

    [[gnu::always_inline]] inline bool stopRequested() const noexcept {
#if defined(__GNUC__) || defined(__clang__)
        return __builtin_expect(_stopFlag->load(std::memory_order_relaxed), 0);
#else
        return _stopFlag->load(std::memory_order_relaxed);
#endif
    }

private:
    const std::atomic<bool>* _stopFlag;
};

// Architecture-specific CPU pause
[[gnu::always_inline]] inline void cpuPause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ __volatile__("yield" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

// Adaptive spin for single-producer single-consumer consumers
class AdaptiveSpinner {
public:
    AdaptiveSpinner() noexcept {
        _spinIteration = 0;
    }

    [[gnu::always_inline]] inline void spin() noexcept {
        if (_spinIteration < MAX_SPINS) {
            for (uint32_t innerSpin = 0; innerSpin < (1u << (_spinIteration / 1000)); innerSpin++) {
                cpuPause();
            }
            _spinIteration++;
        } else {
            std::this_thread::yield();
        }
    }

    [[gnu::always_inline]] inline void reset() noexcept {
        _spinIteration = 0;
    }

private:
    uint32_t _spinIteration;
    static constexpr uint32_t MAX_SPINS = 4000;
};

// RAII-managed pinned thread
template <typename Worker>
class ThreadBootstrap {
public:
    ThreadBootstrap(Worker&& worker, std::atomic<bool>& stopFlag, int targetCpuCore) noexcept
        : _worker(std::forward<Worker>(worker))
        , _stopFlag(stopFlag)
        , _targetCpuCore(targetCpuCore)
    {}

    ThreadBootstrap(const ThreadBootstrap&) = delete;
    ThreadBootstrap& operator=(const ThreadBootstrap&) = delete;
    ThreadBootstrap(ThreadBootstrap&&) = delete;
    ThreadBootstrap& operator=(ThreadBootstrap&&) = delete;

    [[gnu::always_inline]] inline void operator()() noexcept {
        pinThreadIfRequested();
        warmupStackPages();
        _worker(StopToken(&_stopFlag));
    }

private:
    [[gnu::always_inline]] inline void pinThreadIfRequested() noexcept {
        if (_targetCpuCore == -1) {
            return;
        }
#if defined(__linux__)
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        CPU_SET(static_cast<unsigned>(_targetCpuCore), &cpuSet);
        int result = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
        assert(result == 0 && "CPU affinity failed (Linux)");
#elif defined(__APPLE__)
        thread_port_t machThread = pthread_mach_thread_np(pthread_self());
        thread_affinity_policy_data_t policy = {_targetCpuCore};
        thread_policy_set(machThread, THREAD_AFFINITY_POLICY,
                          reinterpret_cast<thread_policy_t>(&policy),
                          THREAD_AFFINITY_POLICY_COUNT);
#endif
    }

    [[gnu::always_inline]] inline void warmupStackPages() noexcept {
        volatile char _stackBuffer[4096];
        for (size_t _offset = 0; _offset < sizeof(_stackBuffer); _offset += 64) {
            _stackBuffer[_offset] = 0;
        }
    }

private:
    Worker _worker;
    std::atomic<bool>& _stopFlag;
    int _targetCpuCore;
};

// RAII wrapper for PinnedThread
class PinnedThread {
public:
    static constexpr int NO_CPU_PINNING = -1;

    template <typename WorkerFunction>
    explicit PinnedThread(WorkerFunction&& worker, int cpuCore = NO_CPU_PINNING)
        : _stopFlag(false)
    {
        using Bootstrap = ThreadBootstrap<std::decay_t<WorkerFunction>>;
        _thread = std::thread(Bootstrap(std::forward<WorkerFunction>(worker),
                                        _stopFlag,
                                        cpuCore));
    }

    PinnedThread(const PinnedThread&) = delete;
    PinnedThread& operator=(const PinnedThread&) = delete;
    PinnedThread(PinnedThread&&) = delete;
    PinnedThread& operator=(PinnedThread&&) = delete;

    ~PinnedThread() {
        if (!_stopFlag.load(std::memory_order_relaxed)) {
            requestStop();
        }
        if (_thread.joinable()) {
            _thread.join();
        }
    }

    [[gnu::always_inline]] inline void requestStop() noexcept {
        _stopFlag.store(true, std::memory_order_relaxed);
    }

private:
    alignas(64) std::atomic<bool> _stopFlag;
    std::thread _thread;
};

} // namespace scrimmage::core::concurrency