#pragma once
#include <cstdint>

namespace match {

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

// Market order price sentinel
constexpr uint32_t MARKET_ORDER_PRICE = 0;

} // namespace match