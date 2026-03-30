#pragma once

#include <atomic>
#include <functional>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

namespace scrimmage::networking {

/**
 * @class TcpServer
 * @brief Generic TCP server for low-latency pipelines
 *
 * Features:
 * - Accepts multiple client connections asynchronously
 * - Each client runs in its own thread
 * - Hot-path callback invoked per message
 * - Thread-safe stop using std::atomic<bool>
 * 
 * Design notes for HFT:
 * - Avoid dynamic memory in hot-path if possible
 * - Fixed-size buffer for reads
 * - Minimal syscalls per loop iteration
 */
class TcpServer
{
public:
    using ClientMessageCallback = std::function<void(const std::string&)>;

    /**
     * @brief Construct the TCP server
     * @param port TCP port to listen on
     * @param callback Hot-path callback for client messages
     * @param backlog Max pending connections in listen queue
     */
    TcpServer(uint16_t port,
              ClientMessageCallback callback,
              size_t backlog = 5) noexcept
        : _port(port), _callback(std::move(callback)), _backlog(backlog)
    {
    }

    ~TcpServer() { stop(); }

    // Non-copyable, non-movable
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    /**
     * @brief Start server: begins listening and accepting clients asynchronously
     */
    void start()
    {
        _running.store(true, std::memory_order_release);
        _accept_thread = std::thread(&TcpServer::listenLoop, this);
    }

    /**
     * @brief Stop server: terminates all threads and closes connections
     */
    void stop()
    {
        _running.store(false, std::memory_order_release);

        if (_accept_thread.joinable())
            _accept_thread.join();

        for (auto& t : _client_threads)
        {
            if (t.joinable())
                t.join();
        }
        _client_threads.clear();
    }

private:
    /**
     * @brief Listen and accept client connections
     *
     * Runs in a dedicated accept thread.
     */
    void listenLoop() noexcept
    {
        int server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd < 0)
        {
            std::cerr << "[TcpServer] Failed to create socket\n";
            return;
        }

        int enable = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);

        if (::bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
        {
            std::cerr << "[TcpServer] Bind failed\n";
            ::close(server_fd);
            return;
        }

        if (::listen(server_fd, static_cast<int>(_backlog)) < 0)
        {
            std::cerr << "[TcpServer] Listen failed\n";
            ::close(server_fd);
            return;
        }

        while (_running.load(std::memory_order_acquire))
        {
            int client_fd = ::accept(server_fd, nullptr, nullptr);
            if (client_fd >= 0)
            {
                _client_threads.emplace_back(&TcpServer::clientLoop, this, client_fd);
            }
        }

        ::close(server_fd);
    }

    /**
     * @brief Handle a single client connection
     * @param client_fd Socket file descriptor
     *
     * Reads messages into a fixed-size buffer and invokes the callback.
     * Thread runs until client disconnects or server stops.
     */
    void clientLoop(int client_fd) noexcept
    {
        constexpr size_t BUFFER_SIZE = 1024;
        alignas(64) char _buffer[BUFFER_SIZE]; ///< Cache-line aligned read buffer

        while (_running.load(std::memory_order_acquire))
        {
            ssize_t bytes = ::read(client_fd, _buffer, BUFFER_SIZE);
            if (bytes <= 0) break;

            // Minimal allocation in hot-path: construct string only once
            std::string message(_buffer, static_cast<size_t>(bytes));
            _callback(message);
        }

        ::close(client_fd);
    }

    uint16_t _port;                           ///< Listening TCP port
    size_t _backlog;                           ///< Max pending connections
    ClientMessageCallback _callback;           ///< Hot-path callback for messages

    std::atomic<bool> _running{false};        ///< Stop flag
    std::thread _accept_thread;               ///< Accept thread
    std::vector<std::thread> _client_threads; ///< Client handler threads
};

} // namespace scrimmage::networking