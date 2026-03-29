#pragma once
#include <cstdint>

namespace scrimmage::exchange::messages {

// Add Order (64 bytes, cache-line aligned)
struct alignas(64) AddOrder {
    uint64_t clientOrderId;
    uint64_t symbol;
    uint64_t price;    // 1/10000 USD
    uint64_t quantity;
    uint8_t  side;     // 0=Buy, 1=Sell
    uint8_t  tif;      // 0=Day, 1=IOC, 2=FOK, 3=GTC
};

// Execution Report (64 bytes, cache-line aligned)
struct alignas(64) ExecutionReport {
    uint64_t orderId;
    uint64_t matchedOrderId;
    uint64_t price;
    uint64_t quantity;
    uint64_t leavesQty;
    uint8_t  execType; // 0=New, 1=Partial, 2=Fill, 3=Cancel, 4=Reject
};

} // namespace beacon::exchange::messages