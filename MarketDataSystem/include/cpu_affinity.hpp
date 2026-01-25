#pragma once

#include <pthread.h>
#include <sched.h>
#include <cstring>
#include <stdexcept>
#include <fmt/core.h>

namespace mds {

/// CPU affinity and scheduling utilities (Linux-only)
class CPUAffinity {
public:
    static void pin_to_core(int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        
        if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0)
            throw std::runtime_error(fmt::format("Failed to pin to core {}", cpu_id));
    }

    [[nodiscard]] static bool try_pin_to_core(int cpu_id) noexcept {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
    }

    [[nodiscard]] static int get_num_cpus() noexcept {
        return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    }

    static void set_realtime_priority(int priority) {
        struct sched_param param { priority };
        if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0)
            throw std::runtime_error("Failed to set realtime priority (requires root)");
    }

    [[nodiscard]] static bool try_set_realtime_priority(int priority) noexcept {
        struct sched_param param { priority };
        return pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) == 0;
    }
};

} // namespace mds
