///////////////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Library     : concurrency
// Purpose     : Ultra-low-latency thread utilities: PinnedThread, StopToken,
//               CPU pause, and thread affinity control for SPSC consumers.
// Author      : Bryan Camp
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <thread>
#include <cassert>
#include <cstddef>
#include <type_traits>

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
#endif

#if defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
#elif defined(__APPLE__)
  #include <pthread.h>
  #include <mach/thread_policy.h>
  #include <mach/mach.h>
#endif

#include "CpuPause.h"

namespace scrimmage::concurrency {

  // StopToken: Deterministic, low-latency signal to stop a thread.
  class StopToken {
    public:
      explicit StopToken(const std::atomic<bool>& stopFlag) noexcept
      : _stopFlag(stopFlag)
      {}

        [[gnu::always_inline]] inline bool stopRequested() const noexcept {
        #if defined(__GNUC__) || defined(__clang__)
          return __builtin_expect(_stopFlag.load(std::memory_order_relaxed), 0);
        #else
          return _stopFlag.load(std::memory_order_relaxed);
        #endif
      }

    private:
      const std::atomic<bool>& _stopFlag;
  };

  // Starts a PinnedThread while making it debuggable.
  // Sets thread properties, avoids a large lambda, simplifies debugging.
  template <typename Worker>
  class ThreadBootstrap {
    public:
      ThreadBootstrap(Worker&& worker,
                      std::atomic<bool>& stopFlag,
                      int targetCpu) noexcept
          : _worker(std::forward<Worker>(worker))
          , _stopFlag(stopFlag)
          , _targetCpu(targetCpu)
      {}

      ThreadBootstrap(const ThreadBootstrap&) = delete;
      ThreadBootstrap& operator=(const ThreadBootstrap&) = delete;
      ThreadBootstrap(ThreadBootstrap&&) = delete;
      ThreadBootstrap& operator=(ThreadBootstrap&&) = delete;

      [[gnu::always_inline]] inline void operator()() noexcept {
          applyThreadAffinity();
          warmupStack();
          _worker(StopToken(_stopFlag));
      }

    private:
      [[gnu::always_inline]] inline void applyThreadAffinity() noexcept {
          if (_targetCpu == -1) {
              return;
          }

      #if defined(__linux__)
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        CPU_SET(static_cast<unsigned>(_targetCpu), &cpuSet);
        int returnCode = pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &cpuSet
        );
        assert(returnCode == 0 && "pthread_setaffinity_np failed");

#elif defined(__APPLE__)
        // NOTE:
        // macOS does NOT support strict CPU pinning.
        // This sets a thread affinity *tag* (grouping hint), not a core binding.
        thread_port_t thread = pthread_mach_thread_np(pthread_self());

        thread_affinity_policy_data_t policy = { _targetCpu };

        kern_return_t kr = thread_policy_set(
            thread,
            THREAD_AFFINITY_POLICY,
            reinterpret_cast<thread_policy_t>(&policy),
            THREAD_AFFINITY_POLICY_COUNT
        );

        assert(kr == KERN_SUCCESS && "thread_policy_set failed (macOS)");

#endif
    }

    [[gnu::always_inline]] inline void warmupStack() noexcept {
        constexpr std::size_t WARMUP_SIZE = 64 * 1024; // 64KB

        volatile char buffer[WARMUP_SIZE];
        for (std::size_t i = 0; i < WARMUP_SIZE; i += 64) {
            buffer[i] = 0;
        }
    }

private:
    Worker _worker;
    std::atomic<bool>& _stopFlag;
    int _targetCpu;
};

///////////////////////////////////////////////////////////////////////////////
// PinnedThread: RAII-managed worker thread with optional affinity
///////////////////////////////////////////////////////////////////////////////
class PinnedThread {
public:
    static constexpr int NO_AFFINITY = -1;

    template <typename WorkerFunction>
    explicit PinnedThread(WorkerFunction&& worker,
                          int targetCpu = NO_AFFINITY)
        : _stopFlag(false)
    {
        using Bootstrap = ThreadBootstrap<std::decay_t<WorkerFunction>>;

        _thread = std::thread{
            Bootstrap(std::forward<WorkerFunction>(worker),
                      _stopFlag,
                      targetCpu)
        };
    }

    PinnedThread(const PinnedThread&) = delete;
    PinnedThread& operator=(const PinnedThread&) = delete;
    PinnedThread(PinnedThread&&) = delete;
    PinnedThread& operator=(PinnedThread&&) = delete;

    ~PinnedThread() {
        requestStop();
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

} // namespace scrimmage::concurrency