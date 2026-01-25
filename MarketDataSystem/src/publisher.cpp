// Market Data Publisher - Publishes via TCP and Shared Memory

#include <atomic>
#include <chrono>
#include <csignal>
#include <random>
#include <thread>
#include <boost/asio.hpp>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include "clock.hpp"
#include "cpu_affinity.hpp"
#include "market_data.hpp"
#include "shared_memory.hpp"
#include "spsc_ring_buffer.hpp"
#include "tcp_server.hpp"

using json = nlohmann::json;
using namespace mds;

static constexpr size_t RING_BUFFER_SIZE = 1024;
static constexpr unsigned short TCP_PORT = 9000;
static constexpr const char* SHM_NAME = "/market_data_shm";
static constexpr int PUBLISHER_CPU_CORE = 0;
static constexpr int PUBLISH_INTERVAL_US = 100;

static std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

static const char* INSTRUMENTS[] = {"RELIANCE", "TCS", "INFY", "HDFCBANK", "ICICIBANK"};
static double PRICES[] = {2850.25, 4125.50, 1825.75, 1650.00, 1125.25};
static constexpr int NUM_INSTRUMENTS = 5;

class MarketDataGenerator {
public:
    MarketDataGenerator() : rng_(std::random_device{}()), 
        price_change_(-0.5, 0.5), spread_(0.25, 1.0), inst_(0, NUM_INSTRUMENTS - 1) {}

    MarketData generate() {
        int idx = inst_(rng_);
        PRICES[idx] += price_change_(rng_);
        if (PRICES[idx] < 1.0) PRICES[idx] = 1000.0;
        double s = spread_(rng_);
        return MarketData(INSTRUMENTS[idx], PRICES[idx] - s/2, PRICES[idx] + s/2, Clock::now_ns());
    }

private:
    std::mt19937 rng_;
    std::uniform_real_distribution<double> price_change_, spread_;
    std::uniform_int_distribution<int> inst_;
};

std::string to_json(const MarketData& d) {
    return json{{"instrument", std::string(d.get_instrument())}, {"bid", d.bid}, 
                {"ask", d.ask}, {"timestamp_ns", d.timestamp_ns}}.dump();
}

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    fmt::print("=== Market Data Publisher ===\n");
    fmt::print("TCP Port: {} | SHM: {} | Interval: {}us\n", TCP_PORT, SHM_NAME, PUBLISH_INTERVAL_US);

    CPUAffinity::try_pin_to_core(PUBLISHER_CPU_CORE);
    CPUAffinity::try_set_realtime_priority(50);

    try {
        using RingBuffer = SPSCRingBuffer<MarketData, RING_BUFFER_SIZE>;
        SharedMemory shm(SHM_NAME, sizeof(RingBuffer), SharedMemory::Mode::Create);
        auto* ring_buffer = new (shm.data()) RingBuffer();

        boost::asio::io_context io_context;
        TCPServer tcp_server(io_context, TCP_PORT);
        tcp_server.start();
        
        std::thread io_thread([&io_context]() { io_context.run(); });

        MarketDataGenerator generator;
        uint64_t msgs = 0, drops = 0;
        auto last_stats = std::chrono::steady_clock::now();

        fmt::print("Publishing...\n");

        while (g_running) {
            MarketData data = generator.generate();
            tcp_server.broadcast(to_json(data));
            if (!ring_buffer->push(data)) ++drops;
            ++msgs;

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 1) {
                fmt::print("[{}] {} msgs | Drops: {} | Clients: {}\n",
                    Clock::format_now(), msgs, drops, tcp_server.client_count());
                last_stats = now;
            }

            std::this_thread::sleep_for(std::chrono::microseconds(PUBLISH_INTERVAL_US));
        }

        tcp_server.stop();
        io_context.stop();
        if (io_thread.joinable()) io_thread.join();

        fmt::print("Total: {} msgs, {} drops\n", msgs, drops);

    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    return 0;
}
