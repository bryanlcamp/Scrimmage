#pragma once
#include <cstdint>
#include <cstring>

namespace scrimmage::exchange::ilink3 {

// Wire Add Order (64 bytes)
struct alignas(64) AddOrderWire {
    uint64_t clientOrderId;
    char symbol[8];       // Futures symbol, padded
    uint32_t quantity;
    uint32_t price;       // 1/10000 USD
    char side;            // 'B' or 'S'
    char orderType;       // 'L','M','S'
    char tif;             // '0'=Day, '3'=IOC, '4'=FOK, 'G'=GTC
    char reserved1;
    uint16_t reserved2;
    char _padding[34];    // 64 bytes total
};

// Wire Execution Ack / Reject
struct alignas(64) ExecutionAck {
    uint64_t orderId;
    uint64_t leavesQty;
    uint64_t filledQty;
    uint8_t  execType;   // 0=Ack, 1=Reject, 2=Partial
    uint8_t  reason;     // Reject reason code if applicable
    char     reserved[54];
};

inline char internalTifToWire(char tif) noexcept {
    switch(tif){ case 'D': return '0'; case 'I': return '3'; case 'F': return '4'; case 'G': return 'G'; default: return '0'; }
}
inline char wireTifToInternal(char tif) noexcept {
    switch(tif){ case '0': return 'D'; case '3': return 'I'; case '4': return 'F'; case 'G': return 'G'; default: return 'D'; }
}

} // namespace beacon::exchange::ilink3