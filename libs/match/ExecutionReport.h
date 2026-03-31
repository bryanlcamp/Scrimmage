#pragma once

#include <string>
#include <cstdint>

namespace scrimmage::matching {

// Execution report sent to client
struct ExecutionReport {
    uint64_t _orderId;
    std::string _symbol;
    double _price;
    uint64_t _filledQuantity;
    uint64_t _remainingQuantity;
    bool _isBuy;
    uint64_t _timestampNs; // high precision nanosecond timestamp
};

} // namespace scrimmage::matching