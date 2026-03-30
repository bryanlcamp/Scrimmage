#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>

namespace beacon::networking {

/**
 * @class TcpFramer
 * @brief Handles framing of incoming TCP byte stream into discrete messages.
 *
 * Design goals:
 * - Inline hot-path buffer copy
 * - Minimal allocations
 * - Template-friendly for fixed-size messages
 */
class TcpFramer {
public:
    TcpFramer(size_t maxFrameSize) : _maxFrameSize(maxFrameSize) {}

    /**
     * @brief Push raw bytes into the framer
     * @param data Pointer to raw bytes
     * @param len Length of bytes
     */
    void push(const char* data, size_t len) {
        if (_buffer.size() + len > _maxFrameSize) {
            throw std::overflow_error("TCP framer buffer overflow");
        }
        _buffer.insert(_buffer.end(), data, data + len);
    }

    /**
     * @brief Extract complete frames from buffer
     * @param frame Output vector
     * @return true if a frame was available
     */
    bool nextFrame(std::vector<char>& frame) {
        if (_buffer.size() < sizeof(uint16_t)) return false;
        uint16_t frameLen;
        std::memcpy(&frameLen, _buffer.data(), sizeof(frameLen));
        if (_buffer.size() < sizeof(frameLen) + frameLen) return false;
        frame.assign(_buffer.begin() + sizeof(frameLen), _buffer.begin() + sizeof(frameLen) + frameLen);
        _buffer.erase(_buffer.begin(), _buffer.begin() + sizeof(frameLen) + frameLen);
        return true;
    }

private:
    std::vector<char> _buffer;
    size_t _maxFrameSize;
};

} // namespace beacon::networking