// Project:      NeuWillow
// Library:      neuro-matching-engine
// Purpose:   Order book entry — the fundamental unit of in the book.
//           32 bytes, alignas(32), 2 entries per cache line.
//           Intrusive doubly-linked via pool indices (no pointers).
// Author:   Bryan Camp

#pragma once

#include <cstdint>
#include <type_traits>

namespace neuwillow::matching {

// Sentinel value — "no link" for intrusive list prev/next
// Used throughout: NONE (0) = uninitialized or end-of-list
// Invariant: all pool indices are > 0; slot 0 reserved as sentinel
static constexpr uint16_t NONE = 0;

// OrderBookEntry: 32 bytes, cache-line aligned
// Stored in OrderPool array; linked into price level FIFO via prev/next indices
// Why no pointers: intrusive indices allow flat array → better cache/prefetch
// Why 32 bytes: exactly 2 entries per 64-byte cache line

// OrderBookEntry is an Order, and a node, in a doubly-linked list.
// A couple of notes:
// (1) This struct is sized for cache-alignment: 32 bytes = 2 per cache line.
// (2) We intentionally avoid pointers; dereferencing is cheap, but 2 steps.
// (3) These nodes are part of anxa "intrusive" data structure, meaning that the
//.    the link forward, link backward, and data are all contained in the
//.    OrderBookEntry itself. The
struct alignas(32) OrderBookEntry {
    uint64_t orderId;           // 8  — client order ID (cancel lookup, fill reports)
    uint32_t quantity;          // 4  — remaining qty (decremented on partial fills)
    uint32_t price;             // 4  — 1/10000 dollars (needed for fill encoding)
    uint16_t nextIndex;         // 2  — index of next entry at this price level (in pool)
    uint16_t prevIndex;         // 2  — index of prev entry (O(1) linkage for cancel/modify)
    uint8_t  exchangeProtocol;  // 1  — 1=OUCH, 2=Pillar, 3=CME (for response encoding)
    char     timeInForce;       // 1  — '0'=Day, '3'=IOC, '4'=FOK, 'G'=GTC
    uint16_t levelIndex;        // 2  — back-reference to price level index
    uint64_t timestamp;         // 8  — nanosecond or high performance timestamp
};                          // = 32 bytes total

// Compile-time layout verification
// These must be true for the pool's intrusive list assumptions to hold
static_assert(sizeof(OrderBookEntry) == 32, "OrderBookEntry must be 32 bytes");
static_assert(alignof(OrderBookEntry) == 32, "OrderBookEntry must be 32-byte aligned (2 per cache line)");

} // namespace neuwillow::matching
