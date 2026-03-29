#pragma once
#include <cstdint>

namespace scrimmage::exchange::messages {

/**
 * @brief Execution results emitted by the matching engine.
 *
 * Designed for:
 * - SPSC transport
 * - Cache-line alignment
 * - Minimal branching on consumer side
 */
struct alignas(64) ExecutionReport {
    enum Type : uint8_t {
        ACK = 0,
        FILL = 1,
        REJECT = 2
    };

    uint64_t orderId;
    uint64_t matchedOrderId; // Only valid for FILL
    uint64_t price;
    uint64_t quantity;

    uint64_t clientOrderId;
    uint64_t timestamp;

    uint8_t  type;
    uint8_t  side;
    uint8_t  reserved[6]; // pad to 64 bytes
};

static_assert(sizeof(ExecutionReport) == 64, "ExecutionReport must be 64 bytes");

} // namespace