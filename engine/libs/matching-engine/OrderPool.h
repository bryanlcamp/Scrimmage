//////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/CommonEnums.h"
#include "OrderBookEntry.h"
#include <array>
#include <bit>
#include <cstring>

namespace scrimmage::match {

template<size_t Capacity = DEFAULT_ORDER_POOL_SIZE>
class OrderPool {
    static_assert(std::has_single_bit(Capacity), "Capacity must be power of 2");

public:
  static constexpr size_t POOL_SIZE = Capacity;
  static constexpr size_t POOL_MASK = Capacity - 1;

  OrderPool() noexcept { 
    reset(); 
  }

  // Called at startup and when the OrderBook needs to reset.
  void reset() noexcept {
    std::memset(_entries.data(), 0, sizeof(OrderBookEntry) * Capacity);
    _freeHead = 1;
    _activeCount = 0;
    for (uint16_t i = 1; i < Capacity - 1; ++i) {
      _entries[i].nextIndex = i + 1;
    }
    _entries[Capacity - 1].nextIndex = OrderPoolPosition.End;
  }

  // Returns an empty OrderEntry from the pool.
  // The return value can't be ignored as it will be the order's id.
  [[nodiscard]] uint16_t allocate() noexcept {
    if (_freeHead == OrderPoolEntries.None) [[unlikely]] 
      return OrderPoolPosition.End;
    uint16_t availableIndex = _freeHead;
    _freeHead = _entries[availableIndex].nextIndex;
    _entries[availableIndex].nextIndex = OrderPoolPosition.End;
    _entries[availableIndex].prevIndex = OrderPoolPosition.End;
    ++_activeCount;
    return availableIndex;
  }

    // Removes an OrderEntry from the pool by index.
    // Marks the removed slot as the next available slot.
    // Note: The caller must ensure the poolIndex is valid and allocated.
    void deallocate(uint16_t poolIndex) noexcept {
        _entries[poolIndex].nextIndex = _freeHead;
        _entries[poolIndex].orderId = 0;
        _freeHead = poolIndex;
        --_activeCount;
    }

    OrderBookEntry& operator[](uint16_t poolEntryIndex) noexcept { 
      return _entries[poolEntryIndex]; 
    }

    const OrderBookEntry& operator[](uint16_t poolEntryIndex) const noexcept { 
      return _entries[poolEntryIndex]; 
    }

    // If calling the accessor the return value must be used.
    [[nodiscard]] size_t getOrderCount() const noexcept { 
      return _activeCount; 
    }

    // If calling the accessor the return value must be used.
    [[nodiscard]] bool isPoolFull() const noexcept { 
      return _freeHead == OrderPoolPosition.End; 
    }

private:

    // Some import notes:
    // We can access OrderBook entries without dereferencing a series of pointers.
    // Dereferering a pointer is fast, but two operations instead of one.
    // But imrportantly, this can help with branch prediction and keep data in the same cache line.
    alignas(64) std::array<OrderBookEntry, Capacity> _entries{};

    // Index of first free slot in the pool.
    uint16_t _freeHead{1};

    // The umber of orders in the pool.
    size_t _activeCount{0};
};

} // namespace scrimmage::match