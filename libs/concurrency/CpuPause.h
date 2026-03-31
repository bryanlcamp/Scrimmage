//////////////////////////////////////////////////////////////////////////////
// Project  : Scrimmage
// Library  : concurrency
// Purpose  : Architecture-optimized CPU pause / yield instruction
// Author   : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

namespace scrimmage::concurrency {
  // architecture-specific spin-loop hint to pause a tight-loop.
  [[gnu::always_inline]] inline void cpuPause() noexcept {
    #if defined(__x86_64__) || defined(_M_X64)
      _mm_pause();
    #elif defined(__aarch64__) || defined(_M_ARM64)
      __asm__ __volatile__("yield" ::: "memory");
    #else
      __asm__ __volatile__("" ::: "memory");
    #endif
  }
} // namespace scrimmage::concurrency