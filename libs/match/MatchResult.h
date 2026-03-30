#pragma once
#include <cstdint>

namespace scrimmage::match {

// Result of a single fill
struct alignas(32) MatchResult {
    uint64_t _orderId;           // Aggressor order
    uint64_t _restingOrderId;    // Passive resting order
    uint32_t _fillPrice;         // Fill price
    uint32_t _fillQty;           // Fill quantity
    uint8_t  _restingProtocol;   // Resting order protocol
    uint8_t  _aggressorProtocol; // Aggressor protocol
    char     _aggressorSide;     // SIDE_BUY or SIDE_SELL
    char     _pad;               // padding for 32-byte alignment
};

static_assert(sizeof(MatchResult) == 32, "MatchResult must be 32 bytes");
static_assert(alignof(MatchResult) == 32, "MatchResult must be 32-byte aligned");

// Result of addOrder()
enum class AddResult : uint8_t {
    ADDED = 0,      // Rests in book
    FILLED = 1,     // Fully matched
    PARTIAL = 2,    // Partially matched, remainder rests
    REJECTED = 3,   // FOK could not fill, or pool exhausted
    CANCELLED = 4   // IOC remainder cancelled
};

// Fill callback type
using FillCallback = void(*)(const MatchResult& fill, void* userData);

} // namespace scrimmage::match