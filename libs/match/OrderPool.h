#pragma once
#include "OrderBookEntry.h"
#include <array>
#include <atomic>
#include <cstddef>

namespace scrimmage::matching {

template <size_t MaxOrders = MAX_ORDERS_PER_BOOK>
class OrderBookPool {
public:
    OrderBookPool() noexcept {
        for (size_t i = 0; i < MaxOrders - 1; i++) {
            _entries[i].nextFreeIndex.store(i + 1, std::memory_order_relaxed);
        }
        _entries[MaxOrders - 1].nextFreeIndex.store(NO_FREE_INDEX, std::memory_order_relaxed);
        _freeIndex.store(0, std::memory_order_relaxed);
    }

    static constexpr size_t NO_FREE_INDEX = SIZE_MAX;

    // Allocate an entry, return index or NO_FREE_INDEX
    size_t allocate() noexcept {
        size_t current = _freeIndex.load(std::memory_order_acquire);
        if (current == NO_FREE_INDEX) {
            return NO_FREE_INDEX;
        }
        size_t next = _entries[current].nextFreeIndex.load(std::memory_order_relaxed);
        _freeIndex.store(next, std::memory_order_release);
        return current;
    }

    void free(size_t idx) noexcept {
        _entries[idx].nextFreeIndex.store(_freeIndex.load(std::memory_order_relaxed), std::memory_order_relaxed);
        _freeIndex.store(idx, std::memory_order_release);
        _entries[idx].reset();
    }

    OrderBookEntry& entry(size_t idx) noexcept {
        return _entries[idx];
    }

    const OrderBookEntry& entry(size_t idx) const noexcept {
        return _entries[idx];
    }

private:
    alignas(64) std::array<OrderBookEntry, MaxOrders> _entries;
    std::atomic<size_t> _freeIndex;
};

} // namespace scrimmage::matching