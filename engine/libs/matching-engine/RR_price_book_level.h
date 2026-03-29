#pragma once
#include <cstdint>

/**
 * @brief Single price level in order book
 */
struct PriceLevel {
    uint64_t price;          // Fixed-point 1/10000 dollars
    uint64_t quantity;       // Total size at this price
    uint16_t headOrderIdx;   // Index to first order in array-based linked list
    uint16_t tailOrderIdx;   // Index to last order
    uint16_t numOrders;      // Number of resting orders
};

/**
 * @brief Resting order
 */
struct OrderBookEntry {
    uint64_t orderId;        // Exchange-assigned order id
    uint64_t clientOrderId;  // Echoed to client
    uint64_t quantity;       // Remaining quantity
    uint64_t price;          // Price (1/10000 dollars)
    uint8_t side;            // 0=Buy, 1=Sell
    uint16_t prevIndex;      // Linked list index
    uint16_t nextIndex;
};