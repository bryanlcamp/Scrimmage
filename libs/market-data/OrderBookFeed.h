//////////////////////////////////////////////////////////////////////////////
// Project:   Scrimmage
// Library:   market-data
// Purpose:   Maintains full depth order book (bid/ask) for a single product.
//            Notes: (1) Thread-safe for multi-threaded pipeline.
//                   (2) Minimal locking in hot path.
//                   (3) Hot-path updates via callbacks from UDP/TCP pipelines.
// Author :   Bryan Camp
//////////////////////////////////////////////////////////////////////////////

#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <functional>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <optional>
#include <cassert>

namespace scrimmage::market {

// A Limit Order at a Proce Range
struct Order {
    uint64_t id       = 0;
    double   price    = 0.0;
    uint32_t quantity = 0;
};


class OrderBookFeed {
public:
    using UpdateCallback = std::function<void()>;

    OrderBookFeed(size_t maxLevels = 1024)
        : _maxLevels(maxLevels)
    {
        _bids.reserve(_maxLevels);
        _asks.reserve(_maxLevels);
    }

    OrderBookFeed(const OrderBookFeed&) = delete;
    OrderBookFeed& operator=(const OrderBookFeed&) = delete;
    OrderBookFeed(OrderBookFeed&&) = delete;
    OrderBookFeed& operator=(OrderBookFeed&&) = delete;

    ~OrderBookFeed() = default;

    /**
     * @brief Insert or update an order in the book
     * @param side true = bid, false = ask
     * @param order Order data
     */
    void upsert(bool side, const Order& order) {
        auto& book = side ? _bids : _asks;
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = std::find_if(book.begin(), book.end(),
                               [&](const Order& o){ return o._id == order._id; });

        if (it != book.end()) {
            // Update existing order
            *it = order;
        } 
        else {
            // Insert new order
            book.push_back(order);
        }

        // Keep book sorted (hot-path: optimize if needed)
        std::sort(book.begin(), book.end(), [side](const Order& a, const Order& b){
            return side ? a._price > b._price : a._price < b._price;
        });

        // Enforce max depth
        if (book.size() > _maxLevels) {
            book.resize(_maxLevels);
        }

        if (_callback) _callback();
    }

    /**
     * @brief Remove an order by ID
     * @param side true = bid, false = ask
     * @param orderId Order ID
     */
    void remove(bool side, uint64_t orderId) {
        auto& book = side ? _bids : _asks;
        std::lock_guard<std::mutex> lock(_mutex);

        auto it = std::remove_if(book.begin(), book.end(),
                                 [&](const Order& o){ return o._id == orderId; });
        if (it != book.end()) {
            book.erase(it, book.end());
        }

        if (_callback) _callback();
    }

    /**
     * @brief Get a snapshot of current book (thread-safe)
     */
    std::pair<std::vector<Order>, std::vector<Order>> snapshot() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return {_bids, _asks};
    }

    /**
     * @brief Register a callback invoked after each update
     */
    void setUpdateCallback(UpdateCallback cb) {
        _callback = std::move(cb);
    }

private:
    size_t _maxLevels;

    std::vector<Order> _bids;   ///< Bid side, sorted descending
    std::vector<Order> _asks;   ///< Ask side, sorted ascending

    mutable std::mutex _mutex;  ///< Protects _bids and _asks
    UpdateCallback _callback;   ///< Optional update notification
};

} // namespace scrimmage::market