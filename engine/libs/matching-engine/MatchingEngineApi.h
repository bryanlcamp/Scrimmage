#pragma once

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "OrderBook.h"

namespace scrimmage::match {

// Router-level result: wraps OrderBook AddResult + identifies which book
struct RouteResult {
    AddResult result;
    uint8_t   bookIndex;    // which book handled the order
};

// Note: This template warranted a lot of comments.
template<
    // MaxSymbols: max distinct symbols per router
    // Default=64: typical exchange serves 50-100 symbols per day
    //   - Must fit in uint8_t (table of books needs 8-bit indices)
    //   - 64 * OrderBook overhead ≈ 2MB per router (acceptable)
    // Common overrides: 32 (quiet days), 128 (busy days)
    size_t MaxSymbols   = 64,

    // PoolSize: pre-allocated order objects per book
    // Default=4096: assumes max 4000 resting orders per symbol
    //   - Too small: rejection of valid orders
    //   - Too large: wasteful memory per router
    // Common overrides: 1024 (liquid), 8192 (volatile)
    size_t PoolSize     = 4096,

    // MapSize: hash table size for tracking resting orders within book
    // Default=8192: load factor ~0.5 with 4096 pool (common for open-addressing)
    //   - Bigger map = fewer collisions = faster cancel/modify
    //   - Must be power of 2 (bit-mask optimization)
    // Common overrides: 4096 (tight memory), 16384 (aggressive trading)
    size_t MapSize = 8192,

    // MaxLevels: The depth of the price book.
    //     Notes: (1) Products with wide spreads can fill many levels.
    //            (2) Most products use < 100 levels; excess is safety margin.
    //            (3) Might use 256 for narrow spreads; 8192 for wide.
    size_t MaxPriceLevels = 4096,

    // RouteMapSize: global hash table for orderId → bookIndex mapping
    // Default=65536: supports tracking up to 32k outstanding orders (load ~0.5)
    //   - One TCP connection order stream routed by symbol
    //   - 65536 is power of 2; efficient % via bitmask (RouteMapMask)
    //   - Each entry: 8 bytes (orderId) + 1 byte (bookIndex) in separate arrays
    // Common overrides: 32768 (slow trading), 131072 (high frequency)
    size_t RouteMapSize = 65536
>
class SymbolRouter {
    static_assert(MaxSymbols <= 256, "MaxSymbols must fit in uint8_t");
    static_assert(std::has_single_bit(RouteMapSize), "RouteMapSize must be power of 2");

public:
    static constexpr size_t MAX_SYMBOLS = MaxSymbols;
    static constexpr size_t ROUTE_MAP_SIZE = RouteMapSize;
    static constexpr size_t ROUTE_MAP_MASK = RouteMapSize - 1;
    // NO_BOOK = 0xFF: invalid book index sentinel value
    //   - Must fit in uint8_t (uint8_t range: 0-255)
    //   - 0xFF (255) chosen because bookIndex always < MaxSymbols (≤256)
    //   - Allows single byte for "no book found" throughout the code
    //   - Used as initial/uninitialized value in hash tables
    static constexpr uint8_t NO_BOOK       = 0xFF;

    SymbolRouter() noexcept { reset(); }

    void reset() noexcept {
        _symbolCount = 0;
        std::memset(_symbolKeys.data(), 0, sizeof(uint64_t) * MaxSymbols);
        std::memset(_routeKeys.data(), 0, sizeof(uint64_t) * RouteMapSize);
        std::memset(_routeValues.data(), NO_BOOK, sizeof(uint8_t) * RouteMapSize);
        for (auto& book : _books) book.reset();
    }

    // Note: This API is for tracking which symbols are valid on each exchange.
    //       This is client-facing, but should likely be refactored to a a separate
    //       SymbolRegistry class that the SymbolRouter references. That said, 
    //       a symbol must be 8 bytes, space-padded (same as NormalizedOrder.symbol).
    //       This is putting too much burden on the client; it's easily fixable and 
    //       will be involved in the SymbolRegistry refactor. Until then, it is 
    //       what it is.
    // 
    // Returns : uint8_t that fits symbol count in single byte (MaxSymbols ≤ 256).
    //           Fast array indexing: _books[bookIdx] uses uint8_t as direct table index.
    //           Minimal per-symbol overhead in routeMap entries.
    [[nodiscard]] uint8_t registerSymbol(const char symbol[8], uint32_t tickSize) noexcept {
        if (_symbolCount >= MaxSymbols) 
        {
          // Likely a disaster. Decision needs to be discussed with business.
          return NO_BOOK;
        }

        uint64_t key = symbolToKey(symbol);

        // Check for duplicate
        for (uint8_t i = 0; i < _symbolCount; ++i) {
            if (_symbolKeys[i] == key) return i;
        }

        uint8_t idx = static_cast<uint8_t>(_symbolCount);
        _symbolKeys[idx] = key;
        _books[idx].setTickSize(tickSize);
        ++_symbolCount;
        return idx;
    }

    [[nodiscard]] AddResult addOrder(const char symbol[8],
      uint32_t price, uint32_t quantity, char side, 
      char timeInForce, uint64_t timestamp,
      FillCallback onFill, void* userData) noexcept {
        
        // Find the book for this symbol.
        uint8_t bookIndex = findBook(symbol);
        if (bookIndex == NO_BOOK) [[unlikely]] {
            // This symbol is not registered. Reject the order.
            return {AddResult::REJECTED, NO_BOOK};
        }

        // TODO: (1) Provide an immediate ACK to the client?
        //       (2) Wait to see if the ordrer was accepted/rejected?
        //       (3) Wait to send an ACK until we know how much quantity was filled immediately?

        // Proceed to add the order to the appropriate book.
        // When we're adding an order via the symbol router, we don't have an orderId yet.
        // Gemerate an order id now, so the order book can pass it via callbacks.
        // We need to return a more rich result from addOrder, including the orderId.
        uint64_t orderId = generateOrderId();
        _books[bookIndex].addOrder(
            orderId, 
            price, quantity, side, 
            timeInForce, 
            timestamp, onFill, userData);
        return orderId;
    }

    // Add order: route by symbol
    [[nodiscard]] RouteResult addOrder(const char symbol[8],
                         uint64_t orderId, uint32_t price, uint32_t quantity,
                         char side, char timeInForce, uint8_t protocol,
                         uint64_t timestamp,
                         FillCallback onFill, void* userData) noexcept {

        uint8_t bookIdx = findBook(symbol);
        if (bookIdx == NO_BOOK) [[unlikely]] {
            return {AddResult::REJECTED, NO_BOOK};
        }

        // Record orderId → bookIndex for cancel/modify routing
        routeInsert(orderId, bookIdx);

        AddResult result = _books[bookIdx].addOrder(
            orderId, price, quantity, side, timeInForce, protocol,
            timestamp, onFill, userData);

        // If order didn't rest in book, remove from route map
        if (result == AddResult::FILLED || result == AddResult::REJECTED ||
            result == AddResult::CANCELLED) {
            routeErase(orderId);
        }

        return {result, bookIdx};
    }

    [[nodiscard]] bool cancelOrder(uint64_t orderId) noexcept {
        uint8_t bookIdx = routeFind(orderId);
        if (bookIdx == NO_BOOK) 
        {
          // TODO: return in native exchange protocol.
          return false;
        }

        bool cancelled = _books[bookIdx].cancelOrder(orderId);
        if (cancelled)
        {
          routeErase(orderId);
        }
        return cancelled;
    }

    // Modify order: route by orderId
    [[nodiscard]] bool modifyOrder(uint64_t orderId, 
      uint32_t newPrice, uint32_t newQty,
      uint64_t timestamp,
      FillCallback onFill, void* userData) noexcept {
        uint8_t bookIdx = routeFind(orderId);
        if (bookIdx == NO_BOOK) {
          // TODO: native exchange protocol.
          return false;
        }

        return _books[bookIdx].modifyOrder(
            orderId, newPrice, newQty, 
            timestamp, 
            onFill, userData);
    }

    [[nodiscard]] OrderBook<PoolSize, MapSize, MaxPriceLevels>&
    book(uint8_t bookIndex) noexcept { return _books[bookIndex]; }

    [[nodiscard]] const OrderBook<PoolSize, MapSize, MaxPriceLevels>&
    book(uint8_t bookIndex) const noexcept { return _books[bookIndex]; }

    [[nodiscard]] uint8_t findBook(const char symbol[8]) const noexcept {
        uint64_t key = symbolToKey(symbol);
        for (uint8_t i = 0; i < _symbolCount; ++i) {
            if (_symbolKeys[i] == key) return i;
        }
        return NO_BOOK;
    }

    [[nodiscard]] size_t symbolCount() const noexcept { return _symbolCount; }

    [[nodiscard]] size_t totalOrders() const noexcept {
        size_t total = 0;
        for (size_t i = 0; i < _symbolCount; ++i) {
            total += _books[i].orderCount();
        }
        return total;
    }

private:
    // Treat symbol[8] as uint64_t for fast comparison (no strcmp)
    // Uses memcpy for safe, unaligned-friendly translation (zero overhead at -O3)
    static uint64_t symbolToKey(const char symbol[8]) noexcept {
        // Why uint64_t: 8-byte symbols can be loaded as single 64-bit integer
        //   - One CPU load instead of 8 char comparisons
        //   - Fast equality: one == instead of strcmp() loop
        //   - sortable: if ever needed for binary search
        uint64_t key{};
        std::memcpy(&key, symbol, 8);
        return key;
    }

    // Fibonacci hash for orderId
    // Why Fibonacci (0x9E3779B97F4A7C15): golden ratio * 2^64
    //   - Uniform distribution across hash buckets
    //   - Minimizes clustering in open-addressing table
    //   - Produces high-quality low bits (used with & RouteMapMask)
    // Why size_t: hash table indices must be platform's address-size integer
    //   - Allows future 64-bit sized tables (on 64-bit systems)
    //   - Consistent with std::size_t convention for container indices
    static size_t hash(uint64_t key) noexcept {
        return static_cast<size_t>(key * UINT64_C(0x9E3779B97F4A7C15));
    }

    // =========================================================================
    // ROUTE MAP: orderId → bookIndex (open-addressing, bitmask)
    // Two parallel arrays (keys/values) for tight memory layout:
    //   - _routeKeys:   [orderId, orderId, 0, orderId, ...]  (uint64_t*)
    //   - _routeValues: [bookIdx, bookIdx, X, bookIdx, ...]  (uint8_t*)
    // Why two arrays: memory efficiency
    //   - Contiguous keys cache-friendly for probing
    //   - Small sentinel values (uint8_t) don't waste space
    //   - vs. std::unordered_map adds 40+ bytes per entry (pointers, hash state)
    // Why 0 as empty sentinel: all orderIds > 0 (assigned by exchange)
    // Why RouteMapSize power of 2: enables (hash & mask) instead of % (division)
    // =========================================================================

    void routeInsert(uint64_t orderId, uint8_t bookIdx) noexcept {
        size_t slot = hash(orderId) & ROUTE_MAP_MASK;
        for (size_t i = 0; i < RouteMapSize; ++i) {
            if (_routeKeys[slot] == 0) {
                _routeKeys[slot] = orderId;
                _routeValues[slot] = bookIdx;
                return;
            }
            slot = (slot + 1) & ROUTE_MAP_MASK;
        }
    }

    [[nodiscard]] uint8_t routeFind(uint64_t orderId) const noexcept {
        size_t slot = hash(orderId) & ROUTE_MAP_MASK;
        for (size_t i = 0; i < RouteMapSize; ++i) {
            if (_routeKeys[slot] == orderId) return _routeValues[slot];
            if (_routeKeys[slot] == 0) return NO_BOOK;
            slot = (slot + 1) & ROUTE_MAP_MASK;
        }
        return NO_BOOK;
    }

    void routeErase(uint64_t orderId) noexcept {
        size_t slot = hash(orderId) & ROUTE_MAP_MASK;
        for (size_t i = 0; i < RouteMapSize; ++i) {
            if (_routeKeys[slot] == orderId) {
                _routeKeys[slot] = 0;
                _routeValues[slot] = NO_BOOK;
                routeRehashFrom(slot);
                return;
            }
            if (_routeKeys[slot] == 0) return;
            slot = (slot + 1) & ROUTE_MAP_MASK;
        }
    }

    void routeRehashFrom(size_t emptySlot) noexcept {
        size_t slot = (emptySlot + 1) & ROUTE_MAP_MASK;
        while (_routeKeys[slot] != 0) {
            size_t idealSlot = hash(_routeKeys[slot]) & ROUTE_MAP_MASK;
            if (shouldMove(emptySlot, slot, idealSlot)) {
                _routeKeys[emptySlot] = _routeKeys[slot];
                _routeValues[emptySlot] = _routeValues[slot];
                _routeKeys[slot] = 0;
                _routeValues[slot] = NO_BOOK;
                emptySlot = slot;
            }
            slot = (slot + 1) & ROUTE_MAP_MASK;
        }
    }

    static bool shouldMove(size_t empty, size_t current, size_t ideal) noexcept {
        if (current >= ideal) {
            return (empty < current) && (empty >= ideal);
        } 
        else {
            return (empty >= ideal) || (empty < current);
        }
    }

    // Symbol registry: small linear scan (< 64 symbols, fits in a cache line or two)
    // Why linear scan instead of hash table: 64 symbols * 8 bytes = 512 bytes
    //   - Fits in L1 cache; sequential search is O(64) with perfect cache behavior
    // Per-symbol order books array, indexed by bookIndex (0-based).
    // Initialized via registerSymbol() which also populates symbol name → index mapping.
    std::array<uint64_t, MaxSymbols> _symbolKeys{};

    // Per-symbol order books
    // Why size_t for loop iteration: matches array .size() return type consistently
    std::array<OrderBook<PoolSize, MapSize, MaxPriceLevels>, MaxSymbols> _books{};

    // Global orderId → bookIndex route map
    // Why alignas(64): keep key/value arrays aligned to cache line
    //   - Reduce false sharing when multiple threads probe hash table
    //   - Improves cache locality during lookups
    alignas(64) std::array<uint64_t, RouteMapSize> _routeKeys{};
    alignas(64) std::array<uint8_t, RouteMapSize>  _routeValues{};

    size_t _symbolCount{0};  // acts as uint8_t but size_t allows loop counter transitions
    size_t _routeCount{0};   // used in diagnostics, size_t for consistency
};

} // namespace scrimmage::match