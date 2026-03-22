//////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   An order, node in the the order book's *intrusive*
//            doubly-linked list. Note optimizations below.
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////

#pragma once

namespace scrimmage::match {

  // A couple of notes:
  // (1) This struct is sized for cache-alignment: 32 bytes = 2 per cache line.
  // (2) The link forward and backward are contained in the nocde itself, i.e. "intrusive".
  // (3) Ints give us array access to an index pool vs "pointer chasing".
  //     Speccifically, no need for *pNext and *pPrev pointers which can cause cache misses.
  struct alignas(32) OrderBookEntry {
    uint64_t orderId;
    uint32_t remainingQuantity;
    uint32_t price;             // 1/10000 dollars (needed for fill encoding)
    uint16_t nextIndex;         // index of next entry at this price level
    uint16_t prevIndex;         // index of next entry at this price level
    ExchangeProtocol protocol;
    TimeInForce timeInForce;
    uint16_t levelIndex;        // back-reference to price level index
    uint64_t timestamp;         // nanosecond or high performance timestamp
  };

  static_assert(sizeof(OrderBookEntry) == 32, "Index pool requires 32-byte structs");
} // namespace scrimmage::match