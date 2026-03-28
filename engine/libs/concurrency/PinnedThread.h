///////////////////////////////////////////////////////////////////////
// Project     : Scrimmage
// Component   : concurrency
// Description : RAII-managed, CPU-pinned worker thread with StopToken.
//               Optimized for ultra-low-latency (HFT-style workloads).
//
// Design Philosophy:
// - Zero-cost abstractions only
// - Deterministic behavior > "modern" abstractions
// - Explicit control over threading, affinity, and memory behavior
//
// Notes for reviewers:
// - Every "non-modern" choice (volatile, #ifdefs, etc.) is intentional
// - std::jthread / std::stop_token avoided due to added abstraction cost
///////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>
#include <atomic>
#include <cassert>
#include <type_traits>

#if defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
#elif defined(__APPLE__)
  #include <pthread.h>
  #include <mach/thread_policy.h>
  #include <mach/mach.h>
#endif

namespace scrimmage::concurrency {

///////////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   A customized, lightweight (readonly) check for thread shutdown.
// Author :   Bryan Camp
//
// Why not std::stop_token? Truth be told, I would have used it. But started
// reading about it, using engineering instincts, and, yes, collaborated with AI and
// came to the conclusion to use a custom StopToken instead. It's not just speed 
// we care about: it's also having deterministic execution: and this compiles 
// to a single load in the hot line, giving us predictability.
//
// StopToken       |  std::stop_token
// ------------------------------------------
// direct ptr      |  indirect (shared state)
// relaxed         |  usually acquire
// no extra logic  |  may include checks
// minimal codegen |  implementation specific
//
// So, if not for being in the HFT hot path, I would have used the std::stop_token.  
///////////////////////////////////////////////////////////////////////////////////
class StopToken {
  public:
    explicit StopToken(const std::atomic<bool>* stopFlag) noexcept
      : _stopFlag(stopFlag) {}

    // Inlining provides a strong hint in tight critical path polling loops,
    // and eliminates any need function call overhead. Note that no synchronization 
    // or ordering of other data is required. Also, note that the expected  
    // ** Expected codegen (x86-64): - single load + branch.
    // Note: we can safely use memory_order_relaxed: we're not using the flag
    // to synchronize data, only to signal.     
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

  ///////////////////////////////////////////////////////////////////////////////////
  // Project:   Scrimmage
  // Library:   matching-engine
  // Purpose:   Predominantly to replace a large lambda for code readability. Also,
  //            pins the thread to a CPU before doing any *real* work, pre-touches
  //            the stack to eliminate page faults, and lastly, launches the
  //            user's thread (which we have ownership) with a StopFlag. Note that
  //            this makes debugging far more simpler than a large lambda statement.
  // Author :   Bryan Camp
  ///////////////////////////////////////////////////////////////////////////////////
  template <typename Worker>
  class ThreadBootstrap {
    public:
      // Capture user worker, stop flag, target CPU
      ThreadBootstrap(PinnedThread&& asyncPinnedWorker,
                      std::atomic<bool>& stopSignal,
                      int targetCpuCore) noexcept
          : _userWorker(std::forward<PinnedThread>(asyncPinnedWorker)),
            _stopSignal(stopSignal),
            _targetCpuCore(targetCpuCore)
      {}

      ThreadBootstrap(const ThreadBootstrap&) = delete;
      ThreadBootstrap& operator=(const ThreadBootstrap&) = delete;
      ThreadBootstrap(ThreadBootstrap&&) = delete;
      ThreadBootstrap& operator=(ThreadBootstrap&&) = delete;

      [[gnu::always_inline]] inline void operator()() noexcept {
          pinThreadIfRequested();
          warmupStackPages();
          runWorkerLoop();
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

        const int result = pthread_setaffinity_np(
          pthread_self(),
          sizeof(cpu_set_t),
          &cpuSet
        );

        // Fail-fast: incorrect pinning breaks latency guarantees
        assert(result == 0 && "CPU affinity failed (Linux)");

      #elif defined(__APPLE__)
        const thread_port_t machThread = pthread_mach_thread_np(pthread_self());
        thread_affinity_policy_data_t policy = { _targetCpuCore };
        thread_policy_set(
          machThread,
          THREAD_AFFINITY_POLICY,
          reinterpret_cast<thread_policy_t>(&policy),
          THREAD_AFFINITY_POLICY_COUNT
        );
      #endif
    } // pinThreadIfRequested

    ///////////////////////////////////////////////////////////////////////////
    // Stack Warmup
    //
    // Purpose:
    // - Eliminate first-touch page faults during runtime
    //
    // Implementation:
    // - volatile: prevents compiler from removing memory writes
    // - 4096 bytes: typical OS page size
    // - 64-byte stride: typical cache line size
    //
    // Effect:
    // - Touches each cache line once → minimal cache pollution
    // - Ensures pages are committed before hot path begins
    ///////////////////////////////////////////////////////////////////////////


    // Note:          See signature usages below:
    // volatile:      (1) Ensures that the compiler does *not* optimize this away
    //                (2) Memory goes unused otherwise.
    // always_inline: (1) Makes this become a string-line code in entry path.
    [[gnu::always_inline]] inline void warmupStackPages() noexcept {
      volatile char buffer[4096];

      // Touch one cache line per 64 bytes.
      for (size_t i = 0; i < sizeof(buffer); i += 64) {
        buffer[i] = 0;
      }
    }

    [[gnu::always_inline]] inline void runWorkerLoop() noexcept {
        // Run the user-provided function, allowing them to stop at any point.
        StopToken token{&_stopFlag};
        _worker(token);
    }

    private:
      // User-provided function.
      PinnedThread _worker;
      
      // Shared stop signal.
      std::atomic<bool>& _stopFlag;

      // Target CPU core.
      int _cpuCore;
  };
};

    
  
  ///////////////////////////////////////////////////////////////////////////////////
  // Project:   Scrimmage
  // Library:   matching-engine
  // Purpose:   RAII-managed worker thread with optional CPU pinning.
  // Author :   Bryan Camp
  //
  // Notes:
  // (1) Cache line isolated stop flag: avoid false sharing.
  // (2) Deterministic shutdown: (requestStop + join)
  /////////////////////////////////////////////////////////////////////////////////
  class PinnedThread {
    public:
      static constexpr int NO_CPU_PINNING = -1;

      PinnedThread() = delete;
      PinnedThread(const PinnedThread&) = delete;
      PinnedThread& operator=(const PinnedThread&) = delete;
      PinnedThread(PinnedThread&&) = delete;
      PinnedThread& operator=(PinnedThread&&) = delete;

      template <typename WorkerFunction>
        explicit PinnedThread(WorkerFunction&& worker,
                            int cpuCore = NO_CPU_PINNING)
          : _stopFlag(false)
      {
        using Bootstrap = ThreadBootstrap<std::decay_t<WorkerFunction>>;

        _thread = std::thread(
            Bootstrap(std::forward<WorkerFunction>(worker),
                      _stopFlag,
                      cpuCore)
        );
      }

      ~PinnedThread() {
        // Avoid an unnecessary store is already stopped.
        if (!_stopFlag.load(std::memory_order_relaxed)) {
          requestStop();
        }

        if (_thread.joinable()) {
          _thread.join();
        }
      }

      // always_inline usage: provides minimal overhead in shutdown paths.
      [[gnu::always_inline]] inline void requestStop() noexcept {
        _stopFlag.store(true, std::memory_order_relaxed);
      }

    private:
      // alignas(64) usage:
      // (1) Prevents false sharing with adjacent memory.
      // (2) Ensures that the stop flag resides on its own cache line.
      alignas(64) std::atomic<bool> _stopFlag;

      std::thread _thread;
  };
} // namespace scrimmage::concurrency