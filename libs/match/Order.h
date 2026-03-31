#pragma once
#include <string>
#include "Constants.h"

namespace scrimmage::matching {

struct Order {
    char symbol[MAX_SYMBOL_LENGTH];   // Fixed-length symbol for cache friendliness
    uint64_t orderId;                 // Unique order ID
    int64_t price;                    // Price in integer ticks
    uint64_t quantity;                // Quantity
    bool isBuy;                       // Buy or sell

    Order() noexcept
        : orderId(ORDERID_EMPTY)
        , price(PRICE_EMPTY)
        , quantity(QUANTITY_EMPTY)
        , isBuy(true)
    {
        symbol[0] = '\0';
    }

    void setSymbol(const std::string& s) noexcept {
        size_t copyLen = (s.size() < MAX_SYMBOL_LENGTH - 1) ? s.size() : MAX_SYMBOL_LENGTH - 1;
        std::memcpy(symbol, s.data(), copyLen);
        symbol[copyLen] = '\0';
    }

    bool isEmpty() const noexcept {
        return orderId == ORDERID_EMPTY;
    }
};

} // namespace scrimmage::matching