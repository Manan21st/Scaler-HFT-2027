// TCP Consumer - Connects to publisher over TCP and logs messages

#include <atomic>
#include <chrono>
#include <csignal>
#include <limits>
#include <thread>
#include <boost/asio.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include "clock.hpp"
#include "cpu_affinity.hpp"

using json = nlohmann::json;
using boost::asio::ip::tcp;
using namespace mds;

static constexpr const char* SERVER_HOST = "127.0.0.1";
static constexpr unsigned short SERVER_PORT = 9000;
static constexpr int CONSUMER_CPU_CORE = 2;

static std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    fmt::print("=== TCP Consumer ===\n");
    fmt::print("Server: {}:{}\n", SERVER_HOST, SERVER_PORT);

    CPUAffinity::try_pin_to_core(CONSUMER_CPU_CORE);
    CPUAffinity::try_set_realtime_priority(48);

    try {
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::socket socket(io_context);

        bool connected = false;
        for (int i = 0; i < 30 && g_running && !connected; ++i) {
            try {
                boost::asio::connect(socket, resolver.resolve(SERVER_HOST, std::to_string(SERVER_PORT)));
                connected = true;
            } catch (...) {
                fmt::print("Waiting for publisher... ({})\n", i + 1);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        if (!connected) { fmt::print("Failed to connect\n"); return 1; }

        socket.set_option(tcp::no_delay(true));
        fmt::print("Connected. Consuming...\n");

        uint64_t msgs = 0, errors = 0;
        int64_t total_lat = 0, max_lat = 0, min_lat = std::numeric_limits<int64_t>::max();
        auto last_stats = std::chrono::steady_clock::now();
        
        std::array<char, 4096> buffer;
        std::string incomplete;

        while (g_running) {
            boost::system::error_code ec;
            size_t bytes = socket.read_some(boost::asio::buffer(buffer), ec);
            
            if (ec == boost::asio::error::eof) break;
            if (ec) { if (g_running) fmt::print("Read error: {}\n", ec.message()); break; }

            int64_t recv_time = Clock::now_ns();
            incomplete.append(buffer.data(), bytes);

            size_t pos;
            while ((pos = incomplete.find('\n')) != std::string::npos) {
                std::string msg = incomplete.substr(0, pos);
                incomplete.erase(0, pos + 1);
                if (msg.empty()) continue;

                try {
                    json j = json::parse(msg);
                    int64_t lat = recv_time - j["timestamp_ns"].get<int64_t>();
                    total_lat += lat;
                    max_lat = std::max(max_lat, lat);
                    min_lat = std::min(min_lat, lat);
                    ++msgs;

                    fmt::print("[{}] {} BID={:.2f} ASK={:.2f} ({}ns)\n",
                        Clock::format_timestamp(j["timestamp_ns"]), 
                        j["instrument"].get<std::string>(),
                        j["bid"].get<double>(), j["ask"].get<double>(), lat);
                } catch (...) { ++errors; }
            }

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 5) {
                fmt::print("--- Stats: {} msgs, avg={:.0f}ns, errors={} ---\n",
                    msgs, msgs > 0 ? (double)total_lat / msgs : 0, errors);
                last_stats = now;
            }
        }

        fmt::print("\nTotal: {} msgs, {} errors, avg latency: {:.0f}ns\n", 
            msgs, errors, msgs > 0 ? (double)total_lat / msgs : 0);
        socket.close();

    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    return 0;
}
