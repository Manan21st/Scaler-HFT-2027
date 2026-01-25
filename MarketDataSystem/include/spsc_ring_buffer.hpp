#pragma once

#include <atomic>
#include <cstddef>
#include <type_traits>

namespace mds {

static constexpr size_t CACHE_LINE_SIZE = 64;

/// Lock-free SPSC ring buffer with cache-line padding to avoid false sharing
template<typename T, size_t Capacity>
class SPSCRingBuffer {
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");

public:
    SPSCRingBuffer() : write_idx_{0}, read_idx_{0} {}

    bool push(const T& value) noexcept {
        size_t write_pos = write_idx_.load(std::memory_order_relaxed);
        size_t next = (write_pos + 1) & (Capacity - 1);
        if (next == read_idx_.load(std::memory_order_acquire)) return false;
        
        buffer_[write_pos] = value;
        write_idx_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& value) noexcept {
        size_t read_pos = read_idx_.load(std::memory_order_relaxed);
        if (read_pos == write_idx_.load(std::memory_order_acquire)) return false;
        
        value = buffer_[read_pos];
        read_idx_.store((read_pos + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_idx_.load(std::memory_order_acquire) ==
               write_idx_.load(std::memory_order_acquire);
    }

private:
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_idx_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_idx_;
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
};

} // namespace mds
