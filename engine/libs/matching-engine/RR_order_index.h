#pragma once
#include <array>
#include <cstdint>

namespace scrimmage::exchange::engine {

/**
 * @brief O(1) orderId → pool index lookup.
 *
 * NOTE:
 * - Assumes low collision environment (power-of-2 masking)
 * - Can be upgraded to robin_hood if needed
 */
template<size_t Size>
class OrderIndex {
public:
    static constexpr uint16_t INVALID = 0xFFFF;

    void reset() noexcept { _map.fill(INVALID); }

    inline void insert(uint64_t id, uint16_t idx) noexcept {
        _map[id & MASK] = idx;
    }

    inline uint16_t find(uint64_t id) const noexcept {
        return _map[id & MASK];
    }

    inline void erase(uint64_t id) noexcept {
        _map[id & MASK] = INVALID;
    }

private:
    static constexpr size_t MASK = Size - 1;
    std::array<uint16_t, Size> _map{};
};

} // namespace