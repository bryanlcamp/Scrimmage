#pragma once
#include <thread>

namespace scrimmage::core {

/**
 * @brief Architecture-optimized CPU pause instruction.
 */
inline void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(_M_X64)
    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ __volatile__("yield" ::: "memory");
#else
    __asm__ __volatile__("" ::: "memory");
#endif
}

/**
 * @brief Adaptive spin with exponential backoff for SPSC consumers.
 */
class AdaptiveSpinner {
    uint32_t spin_count_ = 0;
    static constexpr uint32_t MAX_SPINS = 4000;

public:
    void spin() noexcept {
        if (spin_count_ < MAX_SPINS) {
            for (uint32_t i = 0; i < (1u << (spin_count_/1000)); ++i) cpu_pause();
            ++spin_count_;
        } else {
            std::this_thread::yield();
        }
    }
    void reset() noexcept { spin_count_ = 0; }
};

} // namespace beacon::core