//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
//
// Library:   matching-engine
//
// Purpose:   This is an order book for a single product on any exchange.
//            At this point the native exchange packet has already been
//            decoded and provided to us as bid/ask/qty/side...
//            A few notes:
//            (1) This class, is NOT intended to be used by itself.
//                The SymbolRouter class has an instance of every symbol's order book.
//                The Client-facing API routes the order to the appropriate symbol's order book.
//            (2) Each instance of OrderBook maintains a pool
//
// The OrderBook's client-facing API. It maintains an intrusive 
//            list of Orders, meaning that the array holds Orders, and is
//
// Author:    Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <bit>
#include <cstdint>
#include <cstring>

#include "Common/CommonEnums.h"
#include "OrderBookEntry.h"
#include "OrderPool.h"
#include "OrderMap.h"
#include "PriceLevel.h"

namespace scrimmage::match {

// Note: In the next iteration use native binary protocols.
using FillCallback = void(*)(const MatchResult& fill, void* userData);

template<
    size_t PoolSize  = 4096, // Pre-allocated order objects per book.
    size_t MapSize   = 8192, // Hash table size for order id -> pool index mapping.
    size_t MaxLevels = 4096  // Maximum number of price levels per side.
> 
class OrderBook {
    static_assert(MapSize >= PoolSize * 2,        "Reduce MapSize collisions rate.");
    static_assert(std::has_single_bit(MapSize),   "MapSize must be power of 2.");
    static_assert(std::has_single_bit(MaxLevels), "MaxLevels must be power of 2.");

public:
    OrderBook() noexcept = default;

    // Set tick size (minimum price increment in 1/10000 dollar units).
    // Must be called before first order. E.g., ES = 25, AAPL = 100.
    void setTickSize(uint32_t tickSize) noexcept {
        _bids.setTickSize(tickSize);
        _asks.setTickSize(tickSize);
        _tickSize = tickSize;
    }

    ////////////////////////////////////////////////////////////////////////
    // addOrder
    //
    // Notes  : (1) All TimeInForce values are honored: IOC/FOK/GFD...
    //              Specify a price of 0 for Maret Orders.
    //          (2) All partial fill behavior is honored.
    //          (3) The orderId is decided by the symbo
    // Returns: An enum indicating a possibility of outcomes. 
    ////////////////////////////////////////////////////////////////////////
    AddResult addOrder(uint64_t orderId,
        uint32_t price, uint32_t quantity, char side,
        TimeInForce timeInForce, ExchangeProtocol exchangeProtocl,
        uint64_t timestamp,
        FillCallback onFill, void* userData) noexcept {

        // Set reference price on first order (centers the level array)
        if (!_refPriceSet) [[unlikely]] {
            _bids.setReferencePrice(price);
            _asks.setReferencePrice(price);
            _refPriceSet = true;
        }

        // Fill or Kill: fill the entire order or rejct it.
        if (timeInForce == TimeInForce.FillOrKill) {
            uint32_t available = checkAvailableLiquidity(price, side);
            if (available < quantity) {
                return OrderAddResult.Rejected;
            }
        }

        // See how much quantity we can fill immediately.
        uint32_t remainingQty = quantity;

        // A lot of complexity behind the scenes here.
        bool matched = tryMatch(orderId, 
          price, remainingQty, side, 
          protocol, onFill, userData);

        if (remainingQty == 0) {
            // No more complexity needed. 
            // Simply notify the client they got what they wanted.
            return OrderAddResult.Filled;
        }

        // A couple of notes:
        // (1) We could have been partiall filled, not not at all.
        // (2) Regardless, both Market and IOC orders are no longer valid.

        // Immediate or Cancel: fill as much as possible, cancel the remainder.
        if (timeInForce == TimeInForce.ImmediateOrCancel) {
            // If the order was partiallly fliled, the remainder is cancelled.
            // If we were not filled at all, the order is rejected.          
            return matched ? OrderAddResult.Partial : OrderAddResult.Rejected;
        }

        // Market orders do not rest on the book.
        if (price == MARKET_ORDER_PRICE) {
            // If the order was partiallly fliled, the remainder is cancelled.
            // If we were not filled at all, the order is rejected.
            return matched ? OrderAddResult.Cancelled : OrderAddResult.Rejected;
        }

        // Note: This order could be partially or completely unmatched.
        //       Regardless, since it's an add, it receives a new order id. 
        uint16_t poolIndex = _pool.allocate();

        if (poolIndex == NONE) [[unlikely]] {
          // POTENTIAL DISASTER: The pool is full, and simply rejects the order.
          // We have several options here, each require business decisions: 
          // (1) Expand the pool, which seems *interently* dengerous. 
          //     User configuration/available system memory, etc.
          // (2) Fire an an alert / manual intervention.
          // (3) Throttle or halt trading, which seems like a disaster.
          // (4) This is clearly a business decision.
          // (5) Simply punt, and simply reject the order for now.
          //     Which offerss the client no visibility or availabley to hedge.
          return OrderAddResult.Rejected;;
        }

        // A couple of points re: housekeeping:
        // (1) This order will be resting at front of a new price level.
        // (2) The current order was partially filled, so we have to clean up this price level.

        // A couple of things to remember:
        // (1) The OrderBookEntry is itself a note in an "intrusive" doubly-linked list.
        //     (a) The _pool has an index to the order.
        //     (b) The OrderBookEntry has a back-reference to the pool, which allows O(1) cancel/modify.
        //     (c) Each OrderBookEntry has the array index of both the previous and next entry at each level.

        OrderBookEntry& entry  = _pool[poolIndex];
        entry.orderId          = orderId;
        entry.price            = price;
        entry.quantity         = remainingQty;
        entry.exchangeProtocol = protocol;
        entry.timeInForce      = timeInForce;
        entry.timestamp        = timestamp;

        // Add our link to the new order. 
        if (!_map.insert(orderId, poolIndex, side)) [[unlikely]] {
            _pool.deallocate(poolIndex);
            return AddResult::REJECTED;
        }

        // Add this order to the price level.
        auto& levels = (side == SIDE_BUY) ? _bids : _asks;
        uint16_t levelIndex = levels.priceToLevel(price);
        levels.appendToLevel(levelIndex, poolIndex, _pool);

        // Update the BBO if this order improved the market.
        updateBestAfterAdd(side, levelIndex);

        return matched ? AddResult::PARTIAL : AddResult::ADDED;
    }

    ///////////////////////////////////////////////////////////////////////////
    // modifyOrder
    //
    // Notes  : (1) This is implemented as a cancel/replace.
    //          (2) You lose queue position.
    //          (3) You're not guaranteed to get the price/quantity you want.
    //
    // Returns: True if the cancel succeeded, and the new order was added.
    //          False otherwise.
    ///////////////////////////////////////////////////////////////////////////
    [[nodiscard]]
    bool modifyOrder(uint64_t orderId, uint32_t newPrice, uint32_t newQty,
                     uint64_t timestamp,
                     FillCallback onFill, void* userData) noexcept {

        MapValue mapValue = _map.find(orderId);
        if (mapValue.poolIndex == NONE) 
        {
          // This order was not found. We can't do anything.
          return false;
        }

        OrderBookEntry& entry = _pool[mapValue.poolIndex];
        char side       = mapValue.side;
        uint8_t protocol = entry.exchangeProtocol;
        char timeInForce = entry.timeInForce;

        if (!cancelOrder(orderId)) 
        {
          // TODO: This warrants a special return value.
          return false;
        }

        // Allow quantity increases, until we use an exchange that supports modify-in-place.
        return addOrder(
          orderId,
          newPrice, newQty, side,
          timeInForce, protocol, timestamp,
          onFill, userData);
    }    

    [[nodiscard]] bool cancelOrder(uint64_t orderId) noexcept {

        // This order was not found. We can't do anything.
        MapValue mapValue = _map.find(orderId);
        if (mapValue.poolIndex == OrderPoolPosition.End) 
          return false;

        uint16_t poolIndex = mapValue.poolIndex;
        uint16_t levelIndex = _pool[poolIndex].levelIndex;

        // Check whether removing this order causes us to update the BBO.
        auto& levels = (mapValue.side == SIDE_BUY) ? _bids : _asks;
        bool levelEmpty = levels.unlinkFromLevel(poolIndex, _pool);
        if (levelEmpty && levels.bestLevel() == levelIndex) {
            scanForNewBest(levels, levelIndex, mapValue.side == OrderSide.Buy);
        }

        // Return the slot back to the index pool.
        _map.erase(orderId);
        _pool.deallocate(poolIndex);

        return true;
    }

    [[nodiscard]] uint32_t getBestBidPrice() const noexcept {
        if (_bids.bestLevel() == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL) 
          return 0;
        return _bids.levelToPrice(_bids.bestLevel());
    }

    [[nodiscard]] uint32_t getBestAskPrice() const noexcept {
        if (_asks.bestLevel() == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL) 
          return 0;
        return _asks.levelToPrice(_asks.bestLevel());
    }

    [[nodiscard]] uint32_t getBestBidQty() const noexcept {
        if (_bids.bestLevel() == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL) 
          return 0;
        return _bids.level(_bids.bestLevel()).totalQty;
    }

    [[nodiscard]] uint32_t getBestAskQty() const noexcept {
        if (_asks.bestLevel() == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL) 
          return 0;
        return _asks.level(_asks.bestLevel()).totalQty;
    }

    [[nodiscard]] size_t getOrderCount() const noexcept { 
      return _pool.activeCount(); 
    }

    [[nodiscard]] size_t getBidLevelCount() const noexcept { 
      return _bids.activeLevelCount(); 
    }

    [[nodiscard]] size_t getAskLevelCount() const noexcept { 
      return _asks.activeLevelCount(); 
    }

     void reset() noexcept {
        _pool.reset();
        _map.reset();
        _bids.reset();
        _asks.reset();
        _refPriceSet = false;
    }
    

private:
    // A new notes:
    //  (1) Find best price level on the oppposite side.
    //  (2) If this is a limit order: then check price is favorable; buyer pays >= ask, seller accepts <= bid.
    //  (3) If this is a market order (price=0): cross all available liquidity.
    //  (4) For each level: walk the FIFO queue, match the quantity, generate generate fills, update the order book.
    //  (5) Repeat the above until remainingQty is exhausted or there are no more resting orders.
    bool tryMatch(
      uint64_t aggressorId, uint32_t price, uint32_t& remainingQty,
      char aggressorSide, uint8_t aggressorProtocol,
      FillCallback onFill, void* userData) noexcept 
      {

        auto& oppLevels = (aggressorSide == SIDE_BUY) ? _asks : _bids;

        bool isBuyer = (aggressorSide == SIDE_BUY);
        bool filled = false;

        // While there is still enough liquidity to fill this order.
        while (remainingQty > 0 && oppLevels.hasOrders()) {

            uint16_t besttLevel = oppLevels.bestLevel();
            if (besttLevel == PriceLevels < MaxLevels, PoolSize>::NO_LEVEL) {
              break;
            }

            uint32_t levelPrice = oppLevels.levelToPrice(bestLvl);

            // Price check: buyer must pay >= ask, seller must accept <= bid
            if (isBuyer && price != MARKET_ORDER_PRICE && price < levelPrice) {
              break;
            }
d
            if (!isBuyer && price != MARKET_ORDER_PRICE && price > levelPrice) {
              break;
            }

            // price == MARKET_ORDER_PRICE means market order — crosses any level
            auto& level = oppLevels.level(besttLevel);

            // Walk the FIFO queue at this price level
            uint16_t entryIndex = level.head;
            while (entryIndex != NONE && remainingQty > 0) {

                OrderBookEntry& resting = _pool[entryIndex];
                uint16_t nextIndex = resting.nextIndex;
                uint32_t fillQty = (remainingQty < resting.quantity)
                                   ? remainingQty : resting.quantity;

                // Report the fill
                if (onFill) {
                    MatchResult fill{};
                    fill.orderId = aggressorId;
                    fill.restingOrderId = resting.orderId;
                    fill.fillPrice = levelPrice;
                    fill.fillQty = fillQty;
                    fill.restingProtocol = resting.exchangeProtocol;
                    fill.aggressorProtocol = aggressorProtocol;
                    fill.aggressorSide = aggressorSide;
                    onFill(fill, userData);
                }

                remainingQty -= fillQty;
                filled = true;

                if (fillQty == resting.quantity) {
                    // Fully filled — remove resting order
                    bool levelEmpty = oppLevels.unlinkFromLevel(entryIndex, _pool);
                    _map.erase(resting.orderId);
                    _pool.deallocate(entryIndex);
                    if (levelEmpty) {
                        scanForNewBest(oppLevels, besttLevel, !isBuyer);
                        break;  // level is gone, re-enter outer loop
                    }
                } 
                else {
                    // Partial fill — reduce qty
                    oppLevels.reduceQuantity(entryIndex, fillQty, _pool);
                }

                entryIndex = nextIndex;
            }
        }
        return filled;
    }

    // Check available liquidity for FOK validation
    //
    // Sums total quantity available to cross at the specified price (FOK validation).
    //
    // For a buy order: walks ask side levels below or at the price, accumulating qty.
    // For a sell order: walks bid side levels above or at the price, accumulating qty.
    //
    // Used to validate FOK ('4') orders before placing them — ensures sufficient
    // liquidity exists to fill the entire quantity without leaving a remainder.
    //
    // Returns total quantity available. Caller checks if available >= requested quantity.
    [[nodiscard]] uint32_t checkAvailableLiquidity(uint32_t price, OrderSide side) const noexcept {
        const auto& oppositeLevels = (side == OrderSide.Buy) ? _asks : _bids;
        bool isBuyer = (side == OrderSide.Buy);
        uint32_t availableQuantity = 0;

        if (!oppositeLevels.hasOrders()) return 0;
        uint16_t bestPriceLevel = oppositeLevels.bestLevel();
        if (bestPriceLevel == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL) return 0;

        // Walk levels from best, accumulate qty until price is exceeded
        for (size_t i = 0; i < MaxLevels && lvl != PriceLevels<MaxLevels, PoolSize>::NO_LEVEL; ++i) {
            uint32_t levelPrice = oppositeLevels.levelToPrice(bestPriceLevel);
            if (isBuyer && price != MARKET_ORDER_PRICE && price < levelPrice) break;
            if (!isBuyer && price != MARKET_ORDER_PRICE && price > levelPrice) break;

            availableQuantity += oppositeLevels.level(lvl).totalQty;
)
            bestPriceLevel = scanNextLevel(oppositeLevels, bestPriceLevel, !isBuyer);
        }
        return availableQuantity;
    }

    // After adding to a level, update best bid (highest) or best ask (lowest)
    void updateBestAfterAdd(char side, uint16_t newLevel) noexcept {
        if (side == SIDE_BUY) {
            uint16_t bestBkd = _bids.bestLevel();
            if (bestBid == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL || newLevel > bestBid) {
                _bids.setBestLevel(newLevel);
            }
        } 
        else {
            uint16_t bestAsk = _asks.bestLevel();
            if (bestAsk == PriceLevels<MaxLevels, PoolSize>::NO_LEVEL || newLevel < bestAsk) {
                _asks.setBestLevel(newLevel);
            }
        }
    }

    // For bids: scans downward (lower indices = lower prices) from emptyLevel.
    // For asks: scans upward (higher indices = higher prices) from emptyLevel.
    //
    // If no orders remain at all, sets best level to NO_LEVEL.
    // Critical for maintaining book invariants: bestLevel always points to the most
    // favorable price with actual resting orders (or NO_LEVEL if empty).
    void scanForNewBest(PriceLevels<MaxLevels, PoolSize>& levels,
                        uint16_t emptyLevel, bool isBidSide) noexcept {
        if (!levels.hasOrders()) {
            levels.setBestLevel(PriceLevels<MaxLevels, PoolSize>::NO_LEVEL);
            return;
        }
        uint16_t next = scanNextLevel(levels, emptyLevel, isBidSide);
        levels.setBestLevel(next);
    }

    // Circular Buffer Design.
    // Notes:
    // 1. Level indices are in range [0, MaxLevels)
    // 2. Price levels wrap around: moving from 0 → MaxLevels-1 → 0...
    // 3. Bids: higher indices = higher prices (scan downward: idx--, wrapping)
    // 4. Asks: lower indices = lower prices (scan upward: idx++, wrapping)
    // 5, Bitmask (MaxLevels-1) handles wrapping efficiently
    //
    // Returns the index of the next non-empty level, or NO_LEVEL if none found within MaxLevels.
    // Time Complexity: O(1) to O(MaxLevels) depending on book sparseness.
    [[nodiscard]] uint16_t scanNextLevel(
            const PriceLevels<MaxLevels, PoolSize>& levels,
            uint16_t from, bool isBidSide) const noexcept {
        if (isBidSide) {
            // Bids: scan downward (lower indices = lower prices)
            for (uint16_t i = 1; i < MaxLevels; ++i) {
                uint16_t index = (from - i) & static_cast<uint16_t>(PriceLevels<MaxLevels, PoolSize>::LEVEL_MASK);
                if (levels.level(idx).head != NONE) return index;
            }
        } else {
            // Asks: scan upward (higher indices = higher prices)
            for (uint16_t i = 1; i < MaxLevels; ++i) {
                uint16_t index = (from + i) & static_cast<uint16_t>(PriceLevels<MaxLevels, PoolSize>::LEVEL_MASK);
                if (levels.level(index).head != NONE) return index;
            }
        }
        return PriceLevels<MaxLevels, PoolSize>::NO_LEVEL;
    }

    static constexpr uint32_t MARKET_ORDER_PRICE = 0;

    // _pool: Pre-allocated slot pool for OrderBookEntry objects.
    //        Allocations are O(1) via free list. One slot = 32 bytes cache-friendly size.

    // A couple of notes:
    // (1) 
    OrderPool<PoolSize>  _pool;

    // _map: Hash table mapping orderId (uint64_t) → {poolIndex, side} for O(1) lookups.
    //       Capacity must be >= 2x PoolSize for low collision rate (~0.5 load factor).
    //       Uses Fibonacci hash and Robin Hood backward-shift deletion.

    // A couple of notes:
    // (1) This code is complex, but does offer a slight performancc
    OrderMap<MapSize>  _map;

    // _bids: Bid side PriceLevel management.
    //        Circular buffer for 4096 price levels, FIFO queue per level.
    //        bestLevel tracks highest price with orders (empty = NO_LEVEL).
    PriceLevels<MaxLevels, PoolSize> _bids;

    // _asks: Ask side PriceLevel management.
    //        Circular buffer for 4096 price levels, FIFO queue per level.
    //        bestLevel tracks lowest price with orders (empty = NO_LEVEL).
    PriceLevels<MaxLevels, PoolSize>  _asks;

    // _tickSize: Minimum price increment in 1/10000 dollar units.
    //            Example: ES=25, AAPL=100, BRK.A=100000.
    //            Set via setTickSize() before first order.
    uint32_t _tickSize{1};

    // _refPriceSet: Flag indicating whether reference price has been established.
    //               Set to true on first order to center the circular buffer.
    //               Prevents unnecessary recalibration on subsequent orders.
    bool _refPriceSet{false};
  };
} // namespace scrimmage::matching