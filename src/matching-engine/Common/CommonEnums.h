//////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   A collection of commonly used enums throughout the codebase.
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////

#pragma once

static constexpr uint16_t END_OF_POOL = 0;

namespace scrimmage::match {
  enum class TimeInForce : char {
    GoodTillCancel     = 0, // Rests indefinitely.
    GoodForDay         = 1, // Rests untill the end of the trading day.
    ImmediateOrCancel  = 2, // Fill as much as possible, cancel the remainder.
    FillOrKill         = 3, // Reject entire order if not 100% filled.
    AtTheOpen          = 4, // Typically used in simulation, not live trading.
    GoodTillCancelPlus = 5  // GTC - and do not cross existing orders.
  };

  enum class OrderSide : char { 
    Buy  = 'B', 
    Sell = 'S'
  };

  enum class OrderAddResult : uint8_t {
    Added     = 0,
    Filled    = 1,
    Partial   = 2,
    Rejected  = 3,
    Cancelled = 4
  };

  enum class ExchangeProtocol : uint8_t {
    Ouch   = 1,
    Pillar = 2,
    CME    = 3
  };

  enum class OrderPoolPosition : uint16_t {
    End = 0
   };

} // namespace scrimmage::match