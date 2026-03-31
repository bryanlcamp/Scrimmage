#pragma once
#include <cstdint>

namespace scrimmage::match {

// Sides
constexpr char SIDE_BUY  = 'B';
constexpr char SIDE_SELL = 'S';

// Time-in-Force
constexpr char TIF_GTC = '0';
constexpr char TIF_DAY = '1';
constexpr char TIF_OPG = '2';
constexpr char TIF_IOC = '3';
constexpr char TIF_FOK = '4';
constexpr char TIF_GTX = '5';

// Sentinel values
inline constexpr uint32_t MARKET_ORDER_PRICE   = 0;
inline constexpr int64_t  PRICE_EMPTY          = -1;       // Unset price
inline constexpr uint64_t ORDERID_EMPTY        = 0;        // Unset order ID
inline constexpr uint64_t QUANTITY_EMPTY       = 0;        // Unset quantity
inline constexpr size_t   MAX_SYMBOL_LENGTH    = 16;       // Symbol string length cap
inline constexpr size_t   MAX_ORDERS_PER_BOOK = 1024 * 64; // Adjust for memory/cache
} // namespace scrimmage::matching
} // namespace scrimmage::match