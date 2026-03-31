#pragma once
#include "Constants.h"
#include "Order.h"
#include <atomic>
#include <cstddef>

namespace scrimmage::matching {

struct alignas(64) OrderBookEntry {
    Order order;              // Actual order
    std::atomic<uint64_t> nextFreeIndex; // For pool management

    OrderBookEntry() noexcept
        : order()
        , nextFreeIndex(0)
    {}
    
    bool isEmpty() const noexcept {
        return order.isEmpty();
    }

    void reset() noexcept {
        order = Order();
        nextFreeIndex.store(0, std::memory_order_relaxed);
    }
};

} // namespace scrimmage::matching