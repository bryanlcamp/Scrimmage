#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <vector>

namespace beacon::networking {

/**
 * @class EncoderPipeline
 * @brief Encodes structured messages into raw bytes for transmission.
 *
 * Design goals:
 * - Zero-copy whenever possible
 * - Inline hot-path encode for fixed-size messages
 * - Supports callback to socket send function
 */
class EncoderPipeline {
public:
    using SendCallback = std::function<void(const uint8_t* data, size_t len)>;

    EncoderPipeline() = default;

    EncoderPipeline(const EncoderPipeline&) = delete;
    EncoderPipeline& operator=(const EncoderPipeline&) = delete;

    EncoderPipeline(EncoderPipeline&&) = delete;
    EncoderPipeline& operator=(EncoderPipeline&&) = delete;

    void setSendCallback(SendCallback cb) { _callback = std::move(cb); }

    inline void encode(const uint8_t* buffer, size_t len) noexcept {
        if (_callback) {
            _callback(buffer, len);
        }
    }

private:
    SendCallback _callback;
};

} // namespace beacon::networking