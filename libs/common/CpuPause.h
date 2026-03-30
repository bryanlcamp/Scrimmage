//////////////////////////////////////////////////////////////////////////////
// Project  : Scrimmage
// Library  : Core
// Purpose  : Architecture-optimized CPU pause / yield instruction
// Author   : Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

namespace scrimmage::core {

// Inline CPU pause for spin-wait loops
inline void cpuPause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ __volatile__("yield" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

} // namespace scrimmage::core