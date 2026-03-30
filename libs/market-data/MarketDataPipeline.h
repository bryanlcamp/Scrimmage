#pragma once

#include "order_pool.h"   // Your OrderPool class
#include "order_index.h"  // Your OrderIndex class
#include "order_book_feed.h" // OrderBookFeed
#include <cstdint>
#include <functional>

namespace beacon::market {

/**
 * @class MarketDataPipeline
 * @brief Wires incoming market data into the order pool and order book feed.
 *
 * Concept:
 *   Raw packets → Decoder → OrderPool/OrderIndex → OrderBookFeed → Consumers
 */
class MarketDataPipeline {
public:
    using OrderCallback = std::function<void(const Order&)>;

    MarketDataPipeline(OrderPool& pool, OrderBookFeed& feed)
        : _pool(pool), _feed(feed)
    {}

    MarketDataPipeline(const MarketDataPipeline&) = delete;
    MarketDataPipeline& operator=(const MarketDataPipeline&) = delete;

    /**
     * @brief Process a new raw order message
     * @param order_id Unique order ID
     * @param price Price of the order
     * @param quantity Order size
     * @param side true = bid, false = ask
     */
    void processOrder(uint64_t order_id, double price, uint32_t quantity, bool side) {
        // Allocate or reuse order from pool
        Order* o = _pool.allocate(order_id, price, quantity);

        // Update index for fast lookup
        _index.insert(order_id, o->poolIndex()); // hypothetical poolIndex() method

        // Update full depth book
        _feed.upsert(side, *o);

        // Optional: notify any subscribers
        if (_callback) _callback(*o);
    }

    void setOrderCallback(OrderCallback cb) { _callback = std::move(cb); }

private:
    OrderPool& _pool;
    OrderIndex<65536> _index; // power-of-2 mask for low-collision ID → idx
    OrderBookFeed& _feed;
    OrderCallback _callback;
};

} // namespace beacon::market