#pragma once

#include <atomic>
#include <memory>
#include <cstring>
#include <type_traits>

template<typename T, size_t N>
class RingBuffer {
    static_assert((N & (N - 1)) == 0, "Buffer size must be power of 2");
    static_assert(N > 0, "Buffer size must be greater than 0");

private:
    // 使用2的幂次方大小，可以用位运算代替取模操作
    static constexpr size_t MASK = N - 1;
    
    // 内存对齐，避免false sharing
    alignas(64) std::atomic<size_t> write_pos_{0};
    alignas(64) std::atomic<size_t> read_pos_{0};
    alignas(64) T buffer_[N];

public:
    RingBuffer() = default;
    ~RingBuffer() = default;
    
    // 禁止拷贝和移动
    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // 写入单个元素（无锁，单生产者）
    bool push(const T& item) noexcept {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & MASK;
        
        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false; // 缓冲区满
        }
        
        buffer_[current_write] = item;
        write_pos_.store(next_write, std::memory_order_release);
        return true;
    }
    
    // 写入单个元素（移动语义）
    bool push(T&& item) noexcept {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t next_write = (current_write + 1) & MASK;
        
        if (next_write == read_pos_.load(std::memory_order_acquire)) {
            return false; // 缓冲区满
        }
        
        buffer_[current_write] = std::move(item);
        write_pos_.store(next_write, std::memory_order_release);
        return true;
    }

    // 读取单个元素（无锁，单消费者）
    bool pop(T& item) noexcept {
        const size_t current_read = read_pos_.load(std::memory_order_relaxed);
        
        if (current_read == write_pos_.load(std::memory_order_acquire)) {
            return false; // 缓冲区空
        }
        
        item = std::move(buffer_[current_read]);
        read_pos_.store((current_read + 1) & MASK, std::memory_order_release);
        return true;
    }

    // 批量写入（提高效率）
    size_t push_batch(const T* items, size_t count) noexcept {
        const size_t current_write = write_pos_.load(std::memory_order_relaxed);
        const size_t current_read = read_pos_.load(std::memory_order_acquire);
        
        // 计算可用空间
        size_t available = (current_read - current_write - 1) & MASK;
        size_t to_write = std::min(count, available);
        
        if (to_write == 0) return 0;
        
        // 处理缓冲区环绕
        size_t first_chunk = std::min(to_write, N - current_write);
        std::memcpy(&buffer_[current_write], items, first_chunk * sizeof(T));
        
        if (to_write > first_chunk) {
            std::memcpy(&buffer_[0], items + first_chunk, 
                       (to_write - first_chunk) * sizeof(T));
        }
        
        write_pos_.store((current_write + to_write) & MASK, std::memory_order_release);
        return to_write;
    }

    // 批量读取（提高效率）
    size_t pop_batch(T* items, size_t count) noexcept {
        const size_t current_read = read_pos_.load(std::memory_order_relaxed);
        const size_t current_write = write_pos_.load(std::memory_order_acquire);
        
        // 计算可读数据量
        size_t available = (current_write - current_read) & MASK;
        size_t to_read = std::min(count, available);
        
        if (to_read == 0) return 0;
        
        // 处理缓冲区环绕
        size_t first_chunk = std::min(to_read, N - current_read);
        std::memcpy(items, &buffer_[current_read], first_chunk * sizeof(T));
        
        if (to_read > first_chunk) {
            std::memcpy(items + first_chunk, &buffer_[0], 
                       (to_read - first_chunk) * sizeof(T));
        }
        
        read_pos_.store((current_read + to_read) & MASK, std::memory_order_release);
        return to_read;
    }

    // 查询状态
    bool empty() const noexcept {
        return read_pos_.load(std::memory_order_acquire) == 
               write_pos_.load(std::memory_order_acquire);
    }
    
    bool full() const noexcept {
        return ((write_pos_.load(std::memory_order_acquire) + 1) & MASK) == 
               read_pos_.load(std::memory_order_acquire);
    }
    
    size_t size() const noexcept {
        return (write_pos_.load(std::memory_order_acquire) - 
                read_pos_.load(std::memory_order_acquire)) & MASK;
    }
    
    constexpr size_t capacity() const noexcept {
        return N - 1; // 实际容量比缓冲区小1
    }
};