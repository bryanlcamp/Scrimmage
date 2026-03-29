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
  struct OrderBookEntry {
    uint64_t orderId;
    uint64_t price;
    uint64_t quantity;
    uint8_t  side;
    uint16_t prev;
    uint16_t next;

    static constexpr uint16_t INVALID =
        std::numeric_limits<uint16_t>::max();
};

} // namespace scrimmage::match