#pragma once

#include <vector>
#include <cstdint>
#include <cstring>

namespace beacon::networking {

/**
 * @class TcpEncoder
 * @brief Encodes messages into length-prefixed byte arrays for TCP send.
 *
 * Hot-path notes:
 * - Inline, zero-overhead framing
 * - Compatible with TcpFramer
 */
class TcpEncoder {
public:
    /**
     * @brief Encode a message
     * @param payload Pointer to data
     * @param len Length of payload
     * @param out Output vector to append encoded bytes
     */
    static inline void encode(const void* payload, size_t len, std::vector<char>& out) {
        uint16_t frameLen = static_cast<uint16_t>(len);
        out.resize(out.size() + sizeof(frameLen) + len);
        std::memcpy(out.data() + out.size() - (sizeof(frameLen) + len), &frameLen, sizeof(frameLen));
        std::memcpy(out.data() + out.size() - len, payload, len);
    }
};

} // namespace beacon::networking