#pragma once

#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fmt/core.h>

namespace mds {

/// RAII wrapper for POSIX shared memory (Linux-only)
class SharedMemory {
public:
    enum class Mode { Create, Open };

    SharedMemory(const char* name, size_t size, Mode mode)
        : name_(name), size_(size), ptr_(nullptr), fd_(-1), is_owner_(mode == Mode::Create) {
        
        int flags = O_RDWR | (mode == Mode::Create ? O_CREAT : 0);
        fd_ = ::shm_open(name_.c_str(), flags, 0666);
        if (fd_ == -1)
            throw std::runtime_error(fmt::format("shm_open failed: {}", std::strerror(errno)));

        if (mode == Mode::Create && ::ftruncate(fd_, static_cast<off_t>(size_)) == -1) {
            ::close(fd_);
            ::shm_unlink(name_.c_str());
            throw std::runtime_error(fmt::format("ftruncate failed: {}", std::strerror(errno)));
        }

        ptr_ = ::mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr_ == MAP_FAILED) {
            ::close(fd_);
            if (mode == Mode::Create) ::shm_unlink(name_.c_str());
            throw std::runtime_error(fmt::format("mmap failed: {}", std::strerror(errno)));
        }

        if (mode == Mode::Create)
            std::memset(ptr_, 0, size_);
    }

    ~SharedMemory() {
        if (ptr_ && ptr_ != MAP_FAILED) ::munmap(ptr_, size_);
        if (fd_ != -1) ::close(fd_);
        if (is_owner_) ::shm_unlink(name_.c_str());
    }

    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;

    template<typename T>
    [[nodiscard]] T* get() noexcept { return static_cast<T*>(ptr_); }
    [[nodiscard]] void* data() noexcept { return ptr_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }

private:
    std::string name_;
    size_t size_;
    void* ptr_;
    int fd_;
    bool is_owner_;
};

} // namespace mds
