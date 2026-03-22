//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   The OrderBook's client-facing API return vaue has organically 
//            spread throughout the codebase. It's clean and self-descriptive.
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "CommonEnums.h"

namespace scrimmage::match {
  // Force 32-byte cache alignment.
  struct alignas(32) MatchResult { 
    uint64_t aggressorOrderId;           // Incoming order id.
    uint64_t filledOrderId;              // Resting order id.
    uint32_t fillPrice;                  // Price of the fill (1/10000 dollars).
    uint32_t fillQty;                    // Full / partial.
    ExchangeProtocol restingProtocol;    // Outgoing exchange protocol for decoding.
    ExchangeProtocol aggressorProtocol;  // Incoming exchange protocol for encoding.
    OrderSide aggressorSide;             // Whether the incoming order wantes to buy or sell.
    char _pad;                           // Forcing 32-byte allignment.
  };
} // namespace scrimmage::match