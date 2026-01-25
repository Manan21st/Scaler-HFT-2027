# Low-Latency Market Data Publishing System

**Author:** Manan Agrawal (23bcs10206)  
**Course:** System Programming and Performance Engineering 

A low-latency market data distribution system demonstrating TCP networking and shared memory IPC with a lock-free ring buffer.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    Process A: Publisher                         │
│  ┌─────────────┐   ┌─────────────┐   ┌────────────────────────┐ │
│  │  Market     │──>│   JSON      │──>│    TCP Server          │ │
│  │  Generator  │   │   Encoder   │   │    (Boost.Asio)        │ │
│  └─────────────┘   └─────────────┘   └────────────────────────┘ │
│         │                                       │               │
│         │                                       │ TCP Loopback  │
│         ▼                                       │               │
│  ┌─────────────────────────────────┐            │               │
│  │  SPSC Ring Buffer (mmap)        │            │               │
│  │  Lock-free, cache-line aligned  │            │               │
│  └─────────────────────────────────┘            │               │
└───────────│─────────────────────────────────────│───────────────┘
            │ Shared Memory                       │
            ▼                                     ▼
┌───────────────────────────┐        ┌───────────────────────────┐
│  Process B: SHM Consumer  │        │  Process C: TCP Consumer  │
│  ~1μs latency             │        │  ~10μs latency            │
└───────────────────────────┘        └───────────────────────────┘
```

## Features

| Feature | Implementation |
|---------|----------------|
| Lock-free SPSC Queue | Cache-line aligned atomics with acquire/release semantics |
| TCP Tuning | `TCP_NODELAY`, non-blocking sockets |
| CPU Affinity | Thread pinning to specific cores |
| Realtime Priority | `SCHED_FIFO` scheduling (requires root) |
| Nanosecond Timestamps | High-resolution timing with latency measurement |
| Zero-copy Instrument Access | `std::string_view` for instrument names |

## Market Data Format

```json
{
  "instrument": "RELIANCE",
  "bid": 2850.25,
  "ask": 2850.75,
  "timestamp_ns": 1737822000000000000
}
```

## Building

### Prerequisites
- Linux or WSL (POSIX shared memory APIs)
- C++17 compiler (GCC 8+ or Clang 8+)
- CMake 3.16+
- Boost.Asio

### Build Commands

```bash
cd MarketDataSystem
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Running

Run each process in a separate terminal:

```bash
# Terminal 1 - Publisher
./publisher

# Terminal 2 - SHM Consumer  
./shm_consumer

# Terminal 3 - TCP Consumer
./tcp_consumer
```

## Example Output

**Publisher:**
```
=== Market Data Publisher ===
TCP Port: 9000 | SHM: /market_data_shm | Interval: 100us
Publishing...
[20:45:12.123456789] 10000 msgs | Drops: 0 | Clients: 1
```

**Consumers:**
```
[20:45:12.123456789] RELIANCE BID=2850.25 ASK=2850.75 (1234ns)
```

## Configuration

| Parameter | File | Default |
|-----------|------|---------|
| TCP Port | `publisher.cpp` | 9000 |
| Ring Buffer Size | `publisher.cpp` | 1024 |
| Publish Interval | `publisher.cpp` | 100μs |
| CPU Affinity | All source files | 0, 1, 2 |

## Cleanup

If shared memory persists after a crash:
```bash
rm /dev/shm/market_data_shm
```
