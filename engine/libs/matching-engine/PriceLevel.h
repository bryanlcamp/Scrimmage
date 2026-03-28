#pragma once

#include "OrderBookEntry.h"
#include "OrderPool.h"
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>

namespace scrimmate::match {

// Per-level metadata: head/tail of intrusive FIFO queue + aggregate qty
struct alignas(8) LevelHead {
    uint16_t head;          // first entry index in pool (FIFO front)
    uint16_t tail;          // last entry index in pool (FIFO back, for append)
    uint32_t totalQty;      // aggregate quantity at this level
};

static_assert(sizeof(LevelHead) == 8, "LevelHead must be 8 bytes");

template<size_t MaxLevels = 4096, size_t PoolCapacity = 4096>
class PriceLevels {
    static_assert(std::has_single_bit(MaxLevels), "MaxLevels must be power of 2");

public:
    static constexpr size_t LEVEL_COUNT = MaxLevels;
    static constexpr size_t LEVEL_MASK  = MaxLevels - 1;
    static constexpr uint16_t NO_LEVEL  = UINT16_MAX;

    PriceLevels() noexcept { reset(); }

    void reset() noexcept {
        for (auto& lvl : _levels) {
            lvl.head = NONE;
            lvl.tail = NONE;
            lvl.totalQty = 0;
        }
        _bestLevel = NO_LEVEL;
        _levelCount = 0;
    }

    // Set tick size (minimum price increment in 1/10000 dollar units).
    // Must be called before first order. E.g., ES tick = 25 ($0.0025).
    void setTickSize(uint32_t tickSize) noexcept { _tickSize = tickSize; }

    // Convert an absolute price (1/10000 dollars) to a level index.
    // Divides by tick size so levels represent ticks, not raw price units.
    // Returns index in _levels array (bitmask wrapping for circular buffer).
    [[nodiscard]] uint16_t priceToLevel(uint32_t price) const noexcept {
        int32_t tickOffset = (static_cast<int32_t>(price) - static_cast<int32_t>(_refPrice))
                             / static_cast<int32_t>(_tickSize);
        return static_cast<uint16_t>((tickOffset + static_cast<int32_t>(MaxLevels / 2)) & LEVEL_MASK);
    }

    // Inverse of priceToLevel: given level index, compute absolute price.
    [[nodiscard]] uint32_t levelToPrice(uint16_t levelIdx) const noexcept {
        int32_t tickOffset = static_cast<int32_t>(levelIdx) - static_cast<int32_t>(MaxLevels / 2);
        return static_cast<uint32_t>(static_cast<int32_t>(_refPrice) + tickOffset * static_cast<int32_t>(_tickSize));
    }

    // Set reference price: center of the price level array
    // Used to map between absolute price and array indices with wrapping
    void setReferencePrice(uint32_t price) noexcept { _refPrice = price; }

    [[nodiscard]] uint32_t referencePrice() const noexcept { return _refPrice; }

    // Append an entry (by pool index) to the tail of a price level.
    // The entry's levelIdx is set here.
    void appendToLevel(uint16_t levelIdx, uint16_t entryIdx,
                       OrderPool<PoolCapacity>& pool) noexcept {
        auto& lvl = _levels[levelIdx];
        auto& entry = pool[entryIdx];

        entry.levelIndex = levelIdx;
        entry.prevIndex = lvl.tail;
        entry.nextIndex = NONE;

        if (lvl.tail != NONE) {
            pool[lvl.tail].nextIndex = entryIdx;
        } else {
            lvl.head = entryIdx;  // first entry at this level
        }
        lvl.tail = entryIdx;
        lvl.totalQty += entry.quantity;

        if (lvl.head == entryIdx) ++_levelCount;  // new level activated
    }

    // Unlink an entry from its level (O(1) with prev/next indices).
    // Returns true if the level became empty.
    bool unlinkFromLevel(uint16_t entryIdx, OrderPool<PoolCapacity>& pool) noexcept {
        auto& entry = pool[entryIdx];
        auto& lvl = _levels[entry.levelIndex];

        lvl.totalQty -= entry.quantity;

        if (entry.prevIndex != NONE) {
            pool[entry.prevIndex].nextIndex = entry.nextIndex;
        } else {
            lvl.head = entry.nextIndex;  // was the head
        }

        if (entry.nextIndex != NONE) {
            pool[entry.nextIndex].prevIndex = entry.prevIndex;
        } else {
            lvl.tail = entry.prevIndex;  // was the tail
        }

        bool levelEmpty = (lvl.head == NONE);
        if (levelEmpty) --_levelCount;
        return levelEmpty;
    }

    // Reduce quantity of an entry due to partial fill.
    // Updates both entry's quantity and level's aggregate quantity.
    void reduceQuantity(uint16_t entryIdx, uint32_t fillQty,
                        OrderPool<PoolCapacity>& pool) noexcept {
        pool[entryIdx].quantity -= fillQty;
        _levels[pool[entryIdx].levelIndex].totalQty -= fillQty;
    }

    // Direct access to level metadata (head, tail, totalQty)
    const LevelHead& level(uint16_t levelIdx) const noexcept { return _levels[levelIdx]; }
    LevelHead& level(uint16_t levelIdx) noexcept { return _levels[levelIdx]; }

    // Best level tracking (highest bid or lowest ask)
    // Maintained by OrderBook after add/cancel operations
    [[nodiscard]] uint16_t bestLevel() const noexcept { return _bestLevel; }
    void setBestLevel(uint16_t bestLevel) noexcept { _bestLevel = bestLevel; }

    // Query: does this side have any resting orders?
    [[nodiscard]] bool hasOrders() const noexcept { return _levelCount > 0; }

    // Query: how many price levels are currently occupied?
    [[nodiscard]] size_t activeLevelCount() const noexcept { return _levelCount; }

private:
    // _levels: per-price-level metadata (head, tail, total quantity)
    // Why alignas(64): keep level array cache-line aligned
    //   - Multiple threads may scan levels; reduce false sharing
    //   - Sequential scan performance (stride = 8 bytes per level)
    alignas(64) std::array<LevelHead, MaxLevels> _levels{};

    // _refPrice: center price for the circular buffer indexing
    // Allows wrapping: levelIdx = (tickOffset + MaxLevels/2) & bitmask
    // Updated on first order; provides symmetry around central price
    uint32_t _refPrice{0};

    // _tickSize: minimum price increment in 1/10000 dollar units
    // E.g., ES tick = 25 ($0.0025), AAPL tick = 100 ($0.01)
    // Used in priceToLevel() and levelToPrice() conversions
    uint32_t _tickSize{1};

    // _bestLevel: index of best price level (highest bid or lowest ask)
    // Updated by OrderBook when levels are added/removed
    uint16_t _bestLevel{NO_LEVEL};

    // _levelCount: count of non-empty price levels on this side
    // size_t used for consistency with std::size_t convention;
    // typically < MaxLevels (usually 50-500 live levels)
    size_t _levelCount{0};
};

} // namespace scrimmate::match