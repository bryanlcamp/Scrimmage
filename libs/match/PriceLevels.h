#pragma once
#include <array>
#include <cstdint>
#include "OrderBookEntry.h"

namespace match {

/**
 * @brief Price-level management for one side (bid or ask)
 */
template<size_t MaxLevels, size_t PoolSize>
class PriceLevels {
public:
    static constexpr uint16_t NO_LEVEL = 0xFFFF;
    static constexpr uint16_t LEVEL_MASK = MaxLevels - 1;

    struct Level {
        uint16_t _head{0xFFFF};
        uint16_t _tail{0xFFFF};
        uint32_t _totalQty{0};
    };

    void reset() noexcept {
        _bestLevel = NO_LEVEL;
        _refPrice = 0;
        for (auto& lvl : _levels) lvl = Level{};
    }

    [[nodiscard]] uint16_t bestLevel() const noexcept { return _bestLevel; }
    void setBestLevel(uint16_t lvl) noexcept { _bestLevel = lvl; }

    void setReferencePrice(uint32_t price) noexcept { _refPrice = price; }
    void setTickSize(uint32_t tickSize) noexcept { _tickSize = tickSize; }

    uint16_t priceToLevel(uint32_t price) const noexcept {
        return static_cast<uint16_t>((price - _refPrice) / _tickSize) & LEVEL_MASK;
    }

    uint32_t levelToPrice(uint16_t lvl) const noexcept {
        return _refPrice + lvl * _tickSize;
    }

    Level& level(uint16_t idx) noexcept { return _levels[idx]; }
    const Level& level(uint16_t idx) const noexcept { return _levels[idx]; }

    bool hasOrders() const noexcept {
        for (auto& lvl : _levels) if (lvl._head != 0xFFFF) return true;
        return false;
    }

    void appendToLevel(uint16_t lvlIdx, uint16_t poolIdx, OrderPool<PoolSize>& pool) noexcept {
        Level& lvl = _levels[lvlIdx];
        OrderBookEntry& entry = pool[poolIdx];
        entry._levelIndex = lvlIdx;
        entry._nextIndex = 0xFFFF;

        if (lvl._tail != 0xFFFF) pool[lvl._tail]._nextIndex = poolIdx;
        else lvl._head = poolIdx;
        lvl._tail = poolIdx;
        lvl._totalQty += entry._quantity;
    }

    bool unlinkFromLevel(uint16_t poolIdx, OrderPool<PoolSize>& pool) noexcept {
        Level& lvl = _levels[pool[poolIdx]._levelIndex];
        uint16_t prev = 0xFFFF, idx = lvl._head;

        while (idx != 0xFFFF) {
            if (idx == poolIdx) {
                if (prev != 0xFFFF) pool[prev]._nextIndex = pool[idx]._nextIndex;
                else lvl._head = pool[idx]._nextIndex;
                if (lvl._tail == idx) lvl._tail = prev;
                lvl._totalQty -= pool[idx]._quantity;
                pool[idx]._nextIndex = 0xFFFF;
                return lvl._head == 0xFFFF;
            }
            prev = idx;
            idx = pool[idx]._nextIndex;
        }
        return false;
    }

    void reduceQuantity(uint16_t poolIdx, uint32_t qty, OrderPool<PoolSize>& pool) noexcept {
        Level& lvl = _levels[pool[poolIdx]._levelIndex];
        pool[poolIdx]._quantity -= qty;
        lvl._totalQty -= qty;
    }

    [[nodiscard]] size_t activeLevelCount() const noexcept {
        size_t count = 0;
        for (auto& lvl : _levels) if (lvl._head != 0xFFFF) ++count;
        return count;
    }

private:
    std::array<Level, MaxLevels> _levels{};
    uint16_t _bestLevel{NO_LEVEL};
    uint32_t _tickSize{1};
    uint32_t _refPrice{0};
};

} // namespace match