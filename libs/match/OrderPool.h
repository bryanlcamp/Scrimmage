#pragma once
#include <array>
#include <cstdint>

namespace match {

// Pre-allocated pool for orders
template<size_t PoolSize>
class OrderPool {
public:
    static constexpr uint16_t NONE = 0xFFFF;

    OrderPool() { reset(); }

    void reset() noexcept {
        _freeHead = 0;
        _activeCount = 0;
        for (uint16_t i = 0; i < PoolSize; ++i) {
            _entries[i]._nextIndex = i + 1;
        }
        _entries[PoolSize - 1]._nextIndex = NONE;
    }

    // Allocate a free slot
    uint16_t allocate() noexcept {
        if (_freeHead == NONE) return NONE;
        uint16_t idx = _freeHead;
        _freeHead = _entries[idx]._nextIndex;
        _entries[idx]._nextIndex = NONE;
        ++_activeCount;
        return idx;
    }

    // Deallocate slot
    void deallocate(uint16_t idx) noexcept {
        _entries[idx]._nextIndex = _freeHead;
        _freeHead = idx;
        --_activeCount;
    }

    [[nodiscard]] size_t activeCount() const noexcept { return _activeCount; }

    OrderBookEntry& operator[](size_t idx) noexcept { return _entries[idx]; }
    const OrderBookEntry& operator[](size_t idx) const noexcept { return _entries[idx]; }

private:
    std::array<OrderBookEntry, PoolSize> _entries{};
    uint16_t _freeHead{NONE};
    size_t _activeCount{0};
};

} // namespace match