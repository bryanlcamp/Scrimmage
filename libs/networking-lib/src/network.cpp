#include "network.hpp"

bool Network::isValidIP(const std::string& ip) {
    // Simple validation - can be extended
    return !ip.empty();
}
