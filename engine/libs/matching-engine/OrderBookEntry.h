//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   Intrusive, cache-friendly, OrderBook Entry representation.
//            No pointer chasing, entries are connected via array indices.
//            O(1) insert and remove.
// Author :   Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <limits>

namespace scrimmage::match 
{
  // Hot-path OrderBook entry.
  // Struct size and layout are intentionally chosen for cache-line efficiency:
  //  (a) 32 bytes total, fits 2 entries per 64-byte cache line.
  //  (b) The member order is largest to smallest to minimize padding.

  struct alignas(32) OrderBookEntry {
    uint64_t orderId;       // 8 bytes
    uint64_t price;         // 8 bytes
    uint64_t quantity;      // 8 bytes
    uint8_t  side;          // 1 byte
    uint16_t previousIndex; // 2 bytes - array index allows for O(1) modify/delete.
    uint16_t nextIndex;     // 2 bytes - array index allows for O(1) modify/delete.

    static constexpr uint16_t INVALID =
        std::numeric_limits<uint16_t>::max(); 
  };

static_assert(sizeof(OrderBookEntry) == 32,
              "OrderBookEntry must 32 bytes for cache-line efficiency.");

} // namespace scrimmage::match