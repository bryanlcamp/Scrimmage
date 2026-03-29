#pragma once
#include <array>
#include "order_book_entry.h"

namespace scrimmage::exchange::engine {

/**
 * @brief Intrusive fixed-capacity allocator.
 *
 * Guarantees:
 * - O(1) allocate / deallocate
 * - No heap allocations
 * - Deterministic reuse (LIFO-ish)
 */
template<size_t Capacity>
class OrderPool {
public:
    static constexpr uint16_t INVALID = OrderBookEntry::INVALID;

    void reset() noexcept {
        for (uint16_t i = 0; i < Capacity - 1; ++i)
            _entries[i].next = i + 1;

        _entries[Capacity - 1].next = INVALID;
        _freeHead = 0;
    }

    inline uint16_t allocate() noexcept {
        if (_freeHead == INVALID) return INVALID;

        uint16_t idx = _freeHead;
        _freeHead = _entries[idx].next;

        _entries[idx].next = INVALID;
        _entries[idx].prev = INVALID;

        return idx;
    }

    inline void deallocate(uint16_t idx) noexcept {
        _entries[idx].next = _freeHead;
        _entries[idx].orderId = 0;
        _freeHead = idx;
    }

    inline OrderBookEntry& operator[](uint16_t i) noexcept { return _entries[i]; }
    inline auto& entries() noexcept { return _entries; }

private:
    std::array<OrderBookEntry, Capacity> _entries{};
    uint16_t _freeHead{0};
};

} // namespace