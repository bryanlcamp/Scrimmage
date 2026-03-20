#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>

class Network {
public:
    static bool isValidIP(const std::string& ip);
    static int getPort() { return 8080; }
};

#endif // NETWORK_HPP
