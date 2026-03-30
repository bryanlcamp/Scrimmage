#pragma once

#include "udp_multicast_receiver.h"
#include "udp_multicast_sender.h"
#include "decoder_pipeline.h"
#include "encoder_pipeline.h"

#include <thread>
#include <atomic>

namespace beacon::networking {

/**
 * @class UdpPipeline
 * @brief Hot-path UDP multicast pipeline.
 *
 * Design goals:
 * - Receives multicast messages, dispatches to DecoderPipeline
 * - Sends messages via EncoderPipeline -> UdpMulticastSender
 * - Zero-copy, hot-path optimized
 */
class UdpPipeline {
public:
    UdpPipeline(UdpMulticastReceiver& receiver,
                UdpMulticastSender& sender,
                DecoderPipeline& decoder,
                EncoderPipeline& encoder)
        : _receiver(receiver), _sender(sender), _decoder(decoder), _encoder(encoder) {}

    UdpPipeline(const UdpPipeline&) = delete;
    UdpPipeline& operator=(const UdpPipeline&) = delete;
    UdpPipeline(UdpPipeline&&) = delete;
    UdpPipeline& operator=(UdpPipeline&&) = delete;

    ~UdpPipeline() { stop(); }

    void start() {
        _running = true;
        _readThread = std::thread(&UdpPipeline::readLoop, this);
    }

    void stop() {
        _running = false;
        if (_readThread.joinable()) _readThread.join();
    }

    inline void send(const uint8_t* data, size_t len) {
        _encoder.encode(data, len);
    }

private:
    void readLoop() {
        constexpr size_t BUFFER_SIZE = 4096;
        uint8_t buffer[BUFFER_SIZE];

        while (_running) {
            ssize_t n = _receiver.recv(buffer, BUFFER_SIZE);
            if (n <= 0) continue; // skip on error
            _decoder.decode(buffer, static_cast<size_t>(n));
        }
    }

    UdpMulticastReceiver& _receiver;
    UdpMulticastSender& _sender;
    DecoderPipeline& _decoder;
    EncoderPipeline& _encoder;

    std::atomic<bool> _running{false};
    std::thread _readThread;
};

} // namespace beacon::networking