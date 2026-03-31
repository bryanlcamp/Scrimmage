#pragma once
#include <string>
#include <vector>

namespace scrimmage::matching {

struct MatchingEngineConfig {
    // Symbol ranges this instance is responsible for (inclusive)
    // Example: {"A","E"} means symbols A-E
    std::vector<std::pair<std::string, std::string>> _symbolRanges;

    // TCP/UDP reporting
    std::string _tcpExecutionReportHost;
    uint16_t _tcpExecutionReportPort;
    std::string _udpMatchReportMulticast;
    uint16_t _udpMatchReportPort;

    // Heartbeat interval in nanoseconds
    uint64_t _heartbeatIntervalNs = 1'000'000'000; // default 1 second
};

} // namespace scrimmage::matching