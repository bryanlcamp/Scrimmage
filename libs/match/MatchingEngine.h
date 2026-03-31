#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <functional>
#include <algorithm>
#include <atomic>
#include <memory>
#include "order_pool.h"
#include "networking/tcp_client.h"
#include "networking/udp_multicast_sender.h"

namespace scrimmage::market {

struct Order {
    uint64_t orderId;
    int64_t price;       // Price in integer format
    uint64_t quantity;
    bool isBuy;
    char symbol[16];     // Fixed-size symbol
};

// Execution report callback
using ExecutionReportFn = std::function<void(const Order&, uint64_t filledQuantity)>;

// Match report callback (for multicast)
using MatchReportFn = std::function<void(const Order&, const Order&, uint64_t matchedQuantity)>;

struct MatchingEngineConfig {
    size_t maxOrders;                     // Size of order pool
    ExecutionReportFn executionReportCb;  // TCP
    MatchReportFn matchReportCb;          // UDP
};

class MatchingEngine {
public:
    MatchingEngine(const MatchingEngineConfig& config)
        : _pool(config.maxOrders),
          _executionReportCb(config.executionReportCb),
          _matchReportCb(config.matchReportCb)
    {
    }

    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;

    // Submit new order
    void submitOrder(const Order& order) {
        Order* newOrder = _pool.acquire();
        if (!newOrder) return; // pool exhausted, drop order

        *newOrder = order;
        auto& book = _books[hashSymbol(order.symbol)];
        matchOrder(book, newOrder);
    }

private:
    struct OrderBookEntry {
        std::vector<Order*> buyOrders;
        std::vector<Order*> sellOrders;
    };

    void matchOrder(OrderBookEntry& book, Order* incoming) {
        auto& oppositeSide = incoming->isBuy ? book.sellOrders : book.buyOrders;

        size_t i = 0;
        while (i < oppositeSide.size()) {
            Order* resting = oppositeSide[i];

            if ((incoming->isBuy && incoming->price >= resting->price) ||
                (!incoming->isBuy && incoming->price <= resting->price)) {

                uint64_t fillQty = std::min(incoming->quantity, resting->quantity);
                incoming->quantity -= fillQty;
                resting->quantity -= fillQty;

                _executionReportCb(*incoming, fillQty);
                _executionReportCb(*resting, fillQty);
                _matchReportCb(*incoming, *resting, fillQty);

                if (resting->quantity == 0) {
                    _pool.release(resting);
                    oppositeSide.erase(oppositeSide.begin() + i);
                } else {
                    ++i;
                }

                if (incoming->quantity == 0) {
                    _pool.release(incoming);
                    return;
                }
            } else {
                break; // price does not cross
            }
        }

        // If not fully filled, add to book
        if (incoming->quantity > 0) {
            auto& sameSide = incoming->isBuy ? book.buyOrders : book.sellOrders;
            sameSide.push_back(incoming);
        }
    }

    static size_t hashSymbol(const char* symbol) noexcept {
        // Simple FNV-1a hash
        size_t hash = 14695981039346656037ull;
        for (size_t i = 0; symbol[i] != 0 && i < 16; ++i) {
            hash ^= static_cast<size_t>(symbol[i]);
            hash *= 1099511628211ull;
        }
        return hash % 1024; // fixed-size book table
    }

    std::array<OrderBookEntry, 1024> _books;  // fixed-size symbol hash table
    OrderPool<Order> _pool;                    // preallocated orders

    ExecutionReportFn _executionReportCb;
    MatchReportFn _matchReportCb;
};

} // namespace scrimmage::market