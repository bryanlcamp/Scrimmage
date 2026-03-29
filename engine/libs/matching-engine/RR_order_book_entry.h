#pragma once
#include <cstdint>
#include <limits>

namespace scrimmage::exchange::engine {

/**
 * @brief Intrusive order representation.
 *
 * Design:
 * - No pointers → index-based linking
 * - Cache-friendly
 * - O(1) insert/remove
 */
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

} // namespace