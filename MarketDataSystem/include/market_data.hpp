#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>

namespace mds {

/// Fixed-size market data message (cache-line aligned)
struct alignas(64) MarketData {
    char instrument[16];
    double bid;
    double ask;
    uint64_t timestamp_ns;

    MarketData() : bid(0.0), ask(0.0), timestamp_ns(0) {
        std::memset(instrument, 0, sizeof(instrument));
    }

    MarketData(const char* sym, double b, double a, uint64_t ts)
        : bid(b), ask(a), timestamp_ns(ts) {
        std::strncpy(instrument, sym, sizeof(instrument) - 1);
        instrument[sizeof(instrument) - 1] = '\0';
    }

    [[nodiscard]] std::string_view get_instrument() const noexcept {
        return std::string_view(instrument);
    }
};

static_assert(std::is_trivially_copyable_v<MarketData>);

} // namespace mds
