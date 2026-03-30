#pragma once
#include <array>
#include <cstdint>

namespace match {

/**
 * @brief O(1) orderId -> pool index + side lookup
 */
template<size_t Size>
struct MapValue {
    uint16_t _poolIndex{0xFFFF};
    char _side{0};
};

template<size_t MapSize>
class OrderMap {
public:
    static constexpr uint16_t NONE = 0xFFFF;

    void reset() noexcept {
        for (auto& v : _map) { v._poolIndex = NONE; v._side = 0; }
    }

    bool insert(uint64_t orderId, uint16_t poolIndex, char side) noexcept {
        uint16_t idx = orderId & MASK;
        if (_map[idx]._poolIndex != NONE) return false; // collision
        _map[idx]._poolIndex = poolIndex;
        _map[idx]._side = side;
        return true;
    }

    void erase(uint64_t orderId) noexcept {
        uint16_t idx = orderId & MASK;
        _map[idx]._poolIndex = NONE;
        _map[idx]._side = 0;
    }

    [[nodiscard]] MapValue<MapSize> find(uint64_t orderId) const noexcept {
        return _map[orderId & MASK];
    }

private:
    static constexpr size_t MASK = MapSize - 1;
    std::array<MapValue<MapSize>, MapSize> _map{};
};

} // namespace match