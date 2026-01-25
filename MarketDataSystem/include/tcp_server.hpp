#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <fmt/core.h>

namespace mds {

using boost::asio::ip::tcp;

/// Async TCP server with TCP_NODELAY for low-latency broadcasting
class TCPServer {
public:
    TCPServer(boost::asio::io_context& io_context, unsigned short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), running_(false) {
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    }

    ~TCPServer() { stop(); }

    void start() {
        running_ = true;
        do_accept();
    }

    void stop() {
        running_ = false;
        boost::system::error_code ec;
        acceptor_.close(ec);
        
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& client : clients_)
            if (client && client->is_open()) client->close(ec);
        clients_.clear();
    }

    void broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto msg = std::make_shared<std::string>(message + "\n");
        
        for (auto it = clients_.begin(); it != clients_.end();) {
            if (!(*it) || !(*it)->is_open()) {
                it = clients_.erase(it);
                continue;
            }
            
            std::weak_ptr<tcp::socket> weak_client = *it;
            boost::asio::async_write(**it, boost::asio::buffer(*msg),
                [msg, weak_client](boost::system::error_code ec, std::size_t) {
                    if (ec) {
                        if (auto client = weak_client.lock()) {
                            boost::system::error_code close_ec;
                            client->close(close_ec);
                        }
                    }
                });
            ++it;
        }
    }

    [[nodiscard]] size_t client_count() const {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        return clients_.size();
    }

private:
    void do_accept() {
        if (!running_) return;
        acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec && running_) {
                socket.set_option(tcp::no_delay(true));
                socket.non_blocking(true);
                auto client = std::make_shared<tcp::socket>(std::move(socket));
                {
                    std::lock_guard<std::mutex> lock(clients_mutex_);
                    clients_.push_back(client);
                }
                fmt::print("[TCPServer] Client connected ({})\n", client_count());
            }
            if (running_) do_accept();
        });
    }

    tcp::acceptor acceptor_;
    std::atomic<bool> running_;
    mutable std::mutex clients_mutex_;
    std::vector<std::shared_ptr<tcp::socket>> clients_;
};

} // namespace mds
