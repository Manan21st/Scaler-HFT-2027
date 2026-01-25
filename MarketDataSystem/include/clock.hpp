#pragma once

#include <chrono>
#include <ctime>
#include <string>
#include <fmt/core.h>

namespace mds {

/// Nanosecond-resolution clock utilities
class Clock {
public:
    [[nodiscard]] static int64_t now_ns() noexcept {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    [[nodiscard]] static std::string format_timestamp(int64_t timestamp_ns) {
        auto secs = timestamp_ns / 1'000'000'000;
        auto nanos = timestamp_ns % 1'000'000'000;
        
        std::time_t time = static_cast<std::time_t>(secs);
        std::tm tm_buf;
        localtime_r(&time, &tm_buf);
        return fmt::format("{:02d}:{:02d}:{:02d}.{:09d}",
            tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, nanos);
    }

    [[nodiscard]] static std::string format_now() {
        return format_timestamp(now_ns());
    }
};

} // namespace mds
