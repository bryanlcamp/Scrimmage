//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   An architecture-optimized CPU pause instruction.
//            Specifically, an exponential back-off for SpSc consumers.
// Author :   Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <thread>

namespace scrimmage::concurrency {

  inline void cpuPause() noexcept {
    #if defined(__x86_64__) || defined(_M_X64)
      __builtin_ia32_pause();
    #elif defined(__aarch64__) || defined(_M_ARM64)
      __asm__ __volatile__("yield" ::: "memory");
    #else
      __asm__ __volatile__("" ::: "memory");
    #endif
  }
  
  class AdaptiveSpinner {
    public:
      void spin() noexcept {
        if (_spinCount < MAX_SPINS) {
          for (uint32_t i = 0; i < (1u << (_spinCount/1000)); i++) {
            cpu_pause();
          }
          _spinCount++;
        } 
      else {
        std::this_thread::yield();
      }
    }

    void reset() noexcept { 
      _spinCount = 0; 
    }

  private:
    uint32_t _spinCount; = 0;

    // Hmmm....
    static constexpr uint32_t MAX_SPINS = 4000;
  };
} // namespace scrimmage::concurrency