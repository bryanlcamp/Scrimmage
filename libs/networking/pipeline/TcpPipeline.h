#pragma once

#include "tcp_client.h"
#include "decoder_pipeline.h"
#include "encoder_pipeline.h"

#include <thread>
#include <atomic>
#include <vector>

namespace beacon::networking {

/**
 * @class TcpPipeline
 * @brief Hot-path TCP pipeline: reads/writes messages, decodes/encodes them.
 *
 * Design goals:
 * - Threaded TCP client reading and writing
 * - Zero-copy dispatch to DecoderPipeline
 * - Supports EncoderPipeline for outgoing messages
 */
class TcpPipeline {
public:
    TcpPipeline(TcpClient& client, DecoderPipeline& decoder, EncoderPipeline& encoder)
        : _client(client), _decoder(decoder), _encoder(encoder) {}

    TcpPipeline(const TcpPipeline&) = delete;
    TcpPipeline& operator=(const TcpPipeline&) = delete;

    TcpPipeline(TcpPipeline&&) = delete;
    TcpPipeline& operator=(TcpPipeline&&) = delete;

    ~TcpPipeline() { stop(); }

    void start() {
        _running = true;
        _readThread = std::thread(&TcpPipeline::readLoop, this);
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
            ssize_t n = _client.recv(buffer, BUFFER_SIZE);
            if (n <= 0) break; // error or disconnect
            _decoder.decode(buffer, static_cast<size_t>(n));
        }
    }

    TcpClient& _client;
    DecoderPipeline& _decoder;
    EncoderPipeline& _encoder;

    std::atomic<bool> _running{false};
    std::thread _readThread;
};

} // namespace beacon::networking