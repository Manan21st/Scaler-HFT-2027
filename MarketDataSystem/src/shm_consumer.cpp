// Shared Memory Consumer - Reads market data from ring buffer

#include <atomic>
#include <chrono>
#include <csignal>
#include <limits>
#include <memory>
#include <thread>
#include <fmt/core.h>
#include "clock.hpp"
#include "cpu_affinity.hpp"
#include "market_data.hpp"
#include "shared_memory.hpp"
#include "spsc_ring_buffer.hpp"

using namespace mds;

static constexpr size_t RING_BUFFER_SIZE = 1024;
static constexpr const char* SHM_NAME = "/market_data_shm";
static constexpr int CONSUMER_CPU_CORE = 1;

static std::atomic<bool> g_running{true};
void signal_handler(int) { g_running = false; }

int main() {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    fmt::print("=== SHM Consumer ===\n");

    CPUAffinity::try_pin_to_core(CONSUMER_CPU_CORE);
    CPUAffinity::try_set_realtime_priority(49);

    try {
        using RingBuffer = SPSCRingBuffer<MarketData, RING_BUFFER_SIZE>;
        
        std::unique_ptr<SharedMemory> shm;
        for (int i = 0; i < 30 && g_running && !shm; ++i) {
            try {
                shm = std::make_unique<SharedMemory>(SHM_NAME, sizeof(RingBuffer), SharedMemory::Mode::Open);
            } catch (...) {
                fmt::print("Waiting for publisher... ({})\n", i + 1);
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
        if (!shm) { fmt::print("Failed to connect\n"); return 1; }

        auto* ring_buffer = shm->get<RingBuffer>();
        fmt::print("Connected. Consuming...\n");

        uint64_t msgs = 0;
        int64_t total_lat = 0, max_lat = 0, min_lat = std::numeric_limits<int64_t>::max();
        auto last_stats = std::chrono::steady_clock::now();
        MarketData data;

        while (g_running) {
            if (ring_buffer->pop(data)) {
                int64_t lat = Clock::now_ns() - data.timestamp_ns;
                total_lat += lat;
                max_lat = std::max(max_lat, lat);
                min_lat = std::min(min_lat, lat);
                ++msgs;

                fmt::print("[{}] {} BID={:.2f} ASK={:.2f} ({}ns)\n",
                    Clock::format_timestamp(data.timestamp_ns), data.get_instrument(),
                    data.bid, data.ask, lat);

                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::seconds>(now - last_stats).count() >= 5) {
                    fmt::print("--- Stats: {} msgs, avg={:.0f}ns, min={}ns, max={}ns ---\n",
                        msgs, msgs > 0 ? (double)total_lat / msgs : 0, 
                        min_lat == std::numeric_limits<int64_t>::max() ? 0 : min_lat, max_lat);
                    last_stats = now;
                }
            } else {
                #if defined(__x86_64__) || defined(__i386__)
                    __builtin_ia32_pause();
                #else
                    std::this_thread::yield();
                #endif
            }
        }

        fmt::print("\nTotal: {} msgs, avg latency: {:.0f}ns\n", msgs, 
            msgs > 0 ? (double)total_lat / msgs : 0);

    } catch (const std::exception& e) {
        fmt::print(stderr, "Error: {}\n", e.what());
        return 1;
    }
    return 0;
}
