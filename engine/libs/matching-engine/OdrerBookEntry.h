//////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   An order, and node in the the order book's *intrusive*
//            doubly-linked list. Note optimizations below.
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////

#pragma once

namespace scrimmage::match {

  // A couple of notes:
  // (1) This struct is sized for cache-alignment: 32 bytes = 2 per 64-bit cache line.
  //     Essentially, this allows us a price-level to share a 64-bit cache line.
  //
  // (2) The links forward and backward are ints, not pointers. This allows us to:
  //     (a) Use a *cache-aligned array as an index pool*. By leaving the defaults, this
  //         will allowing enough space in L2 cache, and being in the hot path avoid LRU eviction.
  //         For *extremely* illiquid products, or are known to be extremely quiet, and
  //         have sudden bursts of activity, we could discuss a custom eviction policy.
  //     (b) Basic operations, add, modify, delete run in constant time. An Order, so it's
  //         simply a matter of updating ints, not moving any memory.
  //
  //  (3) OrderBookEntry structs are pre-allocated at compile time, based on what you provide,
  //      so the heap is not used. Instead, simply call allocate() on the OrderPool for a fresh
  //      instance, and deallocate to return the instance to the pool.
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