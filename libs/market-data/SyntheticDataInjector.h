#pragma once

#include "order_book_feed.h"
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>
#include <functional>

namespace beacon::market {

/**
 * @class SyntheticDataInjector
 * @brief Streams synthetic ticks/orders into an OrderBookFeed.
 *
 * Design goals:
 * - Simulates live market data (bid/ask levels, trades)
 * - Multi-thread capable
 * - Hot-path optimized
 */
class SyntheticDataInjector {
public:
    SyntheticDataInjector(OrderBookFeed& book)
        : _book(book)
    {
    }

    SyntheticDataInjector(const SyntheticDataInjector&) = delete;
    SyntheticDataInjector& operator=(const SyntheticDataInjector&) = delete;
    SyntheticDataInjector(SyntheticDataInjector&&) = delete;
    SyntheticDataInjector& operator=(SyntheticDataInjector&&) = delete;

    ~SyntheticDataInjector() {
        stop();
    }

    /**
     * @brief Start streaming synthetic data
     * @param levels Number of levels per side
     * @param intervalMicros Time between updates in microseconds
     */
    void start(size_t levels = 10, uint32_t intervalMicros = 1000) {
        _running = true;
        _thread = std::thread(&SyntheticDataInjector::injectLoop, this, levels, intervalMicros);
    }

    /**
     * @brief Stop the synthetic data stream
     */
    void stop() {
        _running = false;
        if (_thread.joinable())
            _thread.join();
    }

private:
    void injectLoop(size_t levels, uint32_t intervalMicros) {
        std::mt19937_64 rng(std::random_device{}());
        std::uniform_real_distribution<double> priceShift(-0.01, 0.01);
        std::uniform_int_distribution<uint32_t> qtyDist(1, 100);

        double midPrice = 100.0;

        while (_running) {
            // Generate bids
            for (size_t i = 0; i < levels; ++i) {
                Order bid;
                bid._id = i + 1;
                bid._price = midPrice - i * 0.01;
                bid._quantity = qtyDist(rng);
                _book.upsert(true, bid);
            }

            // Generate asks
            for (size_t i = 0; i < levels; ++i) {
                Order ask;
                ask._id = i + 1000; // Separate ID space
                ask._price = midPrice + i * 0.01;
                ask._quantity = qtyDist(rng);
                _book.upsert(false, ask);
            }

            // Optional: small random mid-price shift
            midPrice += priceShift(rng);

            // Sleep between updates
            std::this_thread::sleep_for(std::chrono::microseconds(intervalMicros));
        }
    }

    OrderBookFeed& _book;
    std::atomic<bool> _running{false};
    std::thread _thread;
};

} // namespace beacon::market