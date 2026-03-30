#pragma once
#include <cstdint>
#include "MatchingEngineConstants.h"
#include "OrderBookEntry.h"
#include "OrderPool.h"
#include "OrderMap.h"
#include "PriceLevels.h"
#include "MatchResult.h"

namespace scrimmage::match {

template<size_t PoolSize = 4096,
         size_t MapSize = 8192,
         size_t MaxLevels = 4096>
class OrderBook {
public:
    OrderBook() noexcept = default;

    void reset() noexcept {
        _pool.reset();
        _map.reset();
        _bids.reset();
        _asks.reset();
        _refPriceSet = false;
    }

    void setTickSize(uint32_t tickSize) noexcept {
        _bids.setTickSize(tickSize);
        _asks.setTickSize(tickSize);
        _tickSize = tickSize;
    }

    AddResult addOrder(uint64_t orderId, uint32_t price, uint32_t quantity,
                       char side, char timeInForce, uint8_t protocol,
                       uint64_t timestamp,
                       FillCallback onFill, void* userData) noexcept;

    [[nodiscard]] bool cancelOrder(uint64_t orderId) noexcept;
    [[nodiscard]] bool modifyOrder(uint64_t orderId, uint32_t newPrice, uint32_t newQty,
                                   uint64_t timestamp,
                                   FillCallback onFill, void* userData) noexcept;

    [[nodiscard]] uint32_t bestBidPrice() const noexcept;
    [[nodiscard]] uint32_t bestAskPrice() const noexcept;
    [[nodiscard]] uint32_t bestBidQty() const noexcept;
    [[nodiscard]] uint32_t bestAskQty() const noexcept;

    [[nodiscard]] size_t orderCount() const noexcept { return _pool.activeCount(); }
    [[nodiscard]] size_t bidLevelCount() const noexcept { return _bids.activeLevelCount(); }
    [[nodiscard]] size_t askLevelCount() const noexcept { return _asks.activeLevelCount(); }

private:
    bool tryMatch(uint64_t aggressorId, uint32_t price, uint32_t& remainingQty,
                  char aggressorSide, uint8_t aggressorProtocol,
                  FillCallback onFill, void* userData) noexcept;

    uint32_t checkAvailableLiquidity(uint32_t price, char side) const noexcept;

    void updateBestAfterAdd(char side, uint16_t newLevel) noexcept;
    void scanForNewBest(PriceLevels<MaxLevels, PoolSize>& levels,
                        uint16_t emptyLevel, bool isBidSide) noexcept;
    [[nodiscard]] uint16_t scanNextLevel(
            const PriceLevels<MaxLevels, PoolSize>& levels,
            uint16_t from, bool isBidSide) const noexcept;

    OrderPool<PoolSize> _pool;
    OrderMap<MapSize> _map;
    PriceLevels<MaxLevels, PoolSize> _bids;
    PriceLevels<MaxLevels, PoolSize> _asks;
    uint32_t _tickSize{1};
    bool _refPriceSet{false};
};

} // namespace scrimmage::match