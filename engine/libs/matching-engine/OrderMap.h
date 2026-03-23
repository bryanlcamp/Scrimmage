//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   matching-engine
// Purpose:   This is an order book for a single product on any exchange.
//            At this point the native exchange packet has already been
//            decoded and normalized into the order book's internal format.
//            A few notes:
//            (1) This class, by itself, is not to be intended to be used by
//                itself. Only in conjuntion with the SymbolRouter class.
//                That is why APIs operate on orderIds, and not products.
//            (2) Each instance of OrderBook maintains a pool
// The OrderBook's client-facing API. It maintains an intrusive 
//            list of Orders, meaning that the array holds Orders, and is
//
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include "OrderBookEntry.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace scrimmage::matching {

// Stored per entry in the hash map
struct MapValue {
    uint16_t poolIndex;
    char side;       // 'B' or 'S' — avoids list walk on cancel
    uint8_t _pad;
};

// 
template<size_t Capacity = 8192>
class OrderMap {
    static_assert(std::has_single_bit(Capacity), "Capacity must be power of 2");

public:
    static constexpr size_t MAP_SIZE = Capacity;
    static constexpr size_t MAP_MASK = Capacity - 1;
    static constexpr uint64_t EMPTY_KEY = 0;

    OrderMap() noexcept { reset(); }

    void reset() noexcept {
        std::memset(_keys.data(), 0, sizeof(uint64_t) * Capacity);
        std::memset(_values.data(), 0, sizeof(MapValue) * Capacity);
        _count = 0;
    }

    // Insert orderId → (poolIndex, side).
    // Returns true on success, false if full (should not happen if sized > pool).
    // Why two returns: success vs. full is critical distinction:
    //   - full means the entire hash table is saturated
    //   - typical usage: OrderMap sized 2x OrderPool to keep load factor ~0.5
    bool insert(uint64_t orderId, uint16_t poolIndex, char side) noexcept {
        size_t slot = hash(orderId) & MAP_MASK;
        for (size_t i = 0; i < Capacity; ++i) {
            if (_keys[slot] == EMPTY_KEY) {
                _keys[slot] = orderId;
                _values[slot] = {poolIndex, side, 0};
                ++_count;
                return true;
            }
            slot = (slot + 1) & MAP_MASK;
        }
        return false; // full — should never happen if sized > pool
    }

    // Lookup orderId → MapValue.
    // Returns {NONE, 0, 0} if not found.
    // Caller can check poolIndex == NONE to detect miss.
    [[nodiscard]] MapValue find(uint64_t orderId) const noexcept {
        size_t slot = hash(orderId) & MAP_MASK;
        for (size_t i = 0; i < Capacity; ++i) {
            if (_keys[slot] == orderId) return _values[slot];
            if (_keys[slot] == EMPTY_KEY) return {NONE, 0, 0};
            slot = (slot + 1) & MAP_MASK;
        }
        return {NONE, 0, 0};
    }

    // Remove orderId from the map.
    // Returns true if found and removed, false if not found.
    // Uses Robin Hood backward-shift deletion (no tombstones)
    //   - Probing sequences remain short even with many deletes
    //   - Decrement count to track live entries
    bool erase(uint64_t orderId) noexcept {
        size_t slot = hash(orderId) & MAP_MASK;
        for (size_t i = 0; i < Capacity; ++i) {
            if (_keys[slot] == orderId) {
                // Robin Hood / backward-shift deletion to avoid tombstones
                _keys[slot] = EMPTY_KEY;
                _values[slot] = {NONE, 0, 0};
                --_count;
                rehashFrom(slot);
                return true;
            }
            if (_keys[slot] == EMPTY_KEY) return false;
            slot = (slot + 1) & MAP_MASK;
        }
        return false;
    }

    // Query: how many (orderId) entries are currently stored?
    [[nodiscard]] size_t count() const noexcept { return _count; }

private:
    // Fast hash for uint64_t — fibonacci hashing (multiply + shift)
    // Why Fibonacci (0x9E3779B97F4A7C15): golden ratio * 2^64
    //   - Uniform distribution across hash buckets
    //   - Minimizes clustering in open-addressing table
    //   - Produces high-quality low bits (used with & MAP_MASK)
    static size_t hash(uint64_t key) noexcept {
        // Golden ratio constant for fibonacci hashing
        return static_cast<size_t>(key * UINT64_C(0x9E3779B97F4A7C15));
    }

    // Backward-shift deletion: slide subsequent entries back to fill gap
    // Maintains invariant that probing sequence has no unoccupied slot before entry's ideal slot
    void rehashFrom(size_t emptySlot) noexcept {
        size_t slot = (emptySlot + 1) & MAP_MASK;
        while (_keys[slot] != EMPTY_KEY) {
            size_t idealSlot = hash(_keys[slot]) & MAP_MASK;
            // Check if this entry would benefit from being moved back
            if (shouldMove(emptySlot, slot, idealSlot)) {
                _keys[emptySlot] = _keys[slot];
                _values[emptySlot] = _values[slot];
                _keys[slot] = EMPTY_KEY;
                _values[slot] = {NONE, 0, 0};
                emptySlot = slot;
            }
            slot = (slot + 1) & MAP_MASK;
        }
    }

    // Determine if moving entry from 'current' to 'empty' is correct
    // Circular distance comparison for open addressing wraparound cases
    static bool shouldMove(size_t empty, size_t current, size_t ideal) noexcept {
        // Circular distance comparison for open addressing
        if (current >= ideal) {
            return (empty < current) && (empty >= ideal);
        } else {
            // Wrapped around
            return (empty >= ideal) || (empty < current);
        }
    }

    // _keys: hash table keys (orderId values)
    // Why alignas(64): keep keys array cache-line aligned
    //   - Reduce false sharing during lookups (multiple threads may probe simultaneously)
    //   - Improves cache locality on probing sequences
    alignas(64) std::array<uint64_t, Capacity> _keys{};

    // _values: hash table values (poolIndex, side, padding)
    // Separate from keys for memory efficiency and cache behavior
    alignas(64) std::array<MapValue, Capacity> _values{};

    // _count: number of (orderId) entries currently stored
    // size_t for consistency with std::size_t convention
    size_t _count{0};
};

} // namespace scrimmage::match