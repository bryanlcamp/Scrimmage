#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <functional>
#include <stdexcept>

namespace scrimmage::networking {

/**
 * @class DecoderPipeline
 * @brief Decodes raw bytes into structured messages.
 *
 * Design goals:
 * - Zero-copy decoding when possible
 * - Hot-path optimized, inline decoding for fixed-size messages
 * - Supports callback subscription for downstream consumers
 */
class DecoderPipeline {
public:
    using MessageCallback = std::function<void(const uint8_t* data, size_t len)>;

    DecoderPipeline() = default;

    DecoderPipeline(const DecoderPipeline&) = delete;
    DecoderPipeline& operator=(const DecoderPipeline&) = delete;

    DecoderPipeline(DecoderPipeline&&) = delete;
    DecoderPipeline& operator=(DecoderPipeline&&) = delete;

    /**
     * @brief Register a callback for decoded messages
     */
    void subscribe(MessageCallback cb) { _callback = std::move(cb); }

    /**
     * @brief Decode raw bytes (hot-path)
     * @param buffer Raw byte buffer
     * @param len Length of buffer
     */
    inline void decode(const uint8_t* buffer, size_t len) noexcept {
        // Example: forward entire buffer as a single message
        if (_callback) {
            _callback(buffer, len);
        }
    }

private:
    MessageCallback _callback;
};

} // namespace scrimmage::networking