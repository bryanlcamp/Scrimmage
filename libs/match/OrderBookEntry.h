#pragma once
#include <cstdint>

namespace match {

// Single order in the book
struct alignas(32) OrderBookEntry {
    uint64_t _orderId{0};
    uint32_t _quantity{0};
    uint32_t _price{0};
    uint8_t  _exchangeProtocol{0};
    char     _timeInForce{0};
    uint64_t _timestamp{0};
    uint16_t _levelIndex{0};     // Price level index
    uint16_t _nextIndex{0xFFFF}; // FIFO next pointer

    // Optional: pad to 32 bytes (2 per cache line)
    char _pad[4]{0};
};

static_assert(sizeof(OrderBookEntry) == 32, "OrderBookEntry must be 32 bytes");

} // namespace match