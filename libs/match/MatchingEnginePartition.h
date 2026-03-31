#pragma once

#include <atomic>
#include <string>
#include <chrono>
#include "../common/spsc_ringbuffer.h"
#include "../networking/TcpClient.h"
#include "../networking/UdpMulticastSender.h"
#include "Order.h"
#include "ExecutionReport.h"

namespace scrimmage::matching {

template<size_t Capacity = 1024>
class MatchingEnginePartition {
public:
    MatchingEnginePartition(const std::string& symbolStart,
                            const std::string& symbolEnd,
                            scrimmage::networking::TcpClient& tcpClient,
                            scrimmage::networking::UdpMulticastSender& udpSender) 
        : _symbolStart(symbolStart),
          _symbolEnd(symbolEnd),
          _tcpClient(tcpClient),
          _udpSender(udpSender)
    {}

    // Push new order into partition
    bool submitOrder(const Order& order) noexcept {
        return _ordersQueue.tryPush(order);
    }

    // Main hot-path: match orders
    void processOrders() noexcept {
        Order incomingOrder{};
        while (_ordersQueue.tryPop(incomingOrder)) {
            matchOrder(incomingOrder);
        }
    }

    // Returns true if symbol belongs to this partition
    bool ownsSymbol(const std::string& symbol) const noexcept {
        return symbol >= _symbolStart && symbol <= _symbolEnd;
    }

private:
    void matchOrder(const Order& order) noexcept {
        // Simplified: immediate full fill (stub for resting book later)
        ExecutionReport report{};
        report._orderId = order._orderId;
        report._symbol = order._symbol;
        report._price = order._price;
        report._filledQuantity = order._quantity;
        report._remainingQuantity = 0;
        report._isBuy = order._isBuy;
        report._timestampNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count()
        );

        _fillsQueue.tryPush(report);

        // Send immediately to TCP client
        _tcpClient.sendAll(reinterpret_cast<const char*>(&report), sizeof(report));

        // Broadcast fill to UDP
        _udpSender.send(reinterpret_cast<const char*>(&report), sizeof(report));
    }

    std::string _symbolStart;
    std::string _symbolEnd;

    scrimmage::common::SpScRingBuffer<Order, Capacity> _ordersQueue;
    scrimmage::common::SpScRingBuffer<ExecutionReport, Capacity> _fillsQueue;

    scrimmage::networking::TcpClient& _tcpClient;
    scrimmage::networking::UdpMulticastSender& _udpSender;
};

} // namespace scrimmage::matching