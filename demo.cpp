#include "ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <atomic> // Added for multi-threading tests

// 测试数据结构
struct TestData {
    int id;
    double value;
    char padding[48]; // 增加结构体大小，测试内存操作效率
    
    TestData() : id(0), value(0.0) {}
    TestData(int i, double v) : id(i), value(v) {}
};

// 性能计时器
class Timer {
    std::chrono::high_resolution_clock::time_point start_;
public:
    Timer() : start_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() const {
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }
};

// 单线程性能测试
void single_thread_test() {
    std::cout << "\n=== 单线程性能测试 ===" << std::endl;
    
    constexpr size_t BUFFER_SIZE = 1024; // 必须是2的幂
    constexpr size_t TEST_COUNT = 1000000;
    
    RingBuffer<int, BUFFER_SIZE> buffer;
    
    // 写入测试
    {
        Timer timer;
        for (size_t i = 0; i < TEST_COUNT; ++i) {
            while (!buffer.push(static_cast<int>(i))) {
                // 如果缓冲区满，等待或处理
                int dummy;
                buffer.pop(dummy);
            }
        }
        std::cout << "写入 " << TEST_COUNT << " 个元素耗时: " 
                  << std::fixed << std::setprecision(2) << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // 读取测试
    {
        Timer timer;
        int value;
        size_t read_count = 0;
        while (buffer.pop(value)) {
            ++read_count;
        }
        std::cout << "读取 " << read_count << " 个元素耗时: " 
                  << std::fixed << std::setprecision(2) << timer.elapsed_ms() << " ms" << std::endl;
    }
}

// 批量操作测试
void batch_operations_test() {
    std::cout << "\n=== 批量操作性能测试 ===" << std::endl;
    
    constexpr size_t BUFFER_SIZE = 4096;
    constexpr size_t TEST_COUNT = 1000000;
    constexpr size_t BATCH_SIZE = 256;
    
    RingBuffer<TestData, BUFFER_SIZE> buffer;
    std::vector<TestData> write_data(BATCH_SIZE);
    std::vector<TestData> read_data(BATCH_SIZE);
    
    // 准备测试数据
    for (size_t i = 0; i < BATCH_SIZE; ++i) {
        write_data[i] = TestData(static_cast<int>(i), i * 1.5);
    }
    
    // 批量写入测试
    {
        Timer timer;
        size_t total_written = 0;
        while (total_written < TEST_COUNT) {
            size_t written = buffer.push_batch(write_data.data(), BATCH_SIZE);
            total_written += written;
            
            if (written == 0) {
                // 缓冲区满，读取一些数据
                buffer.pop_batch(read_data.data(), BATCH_SIZE);
            }
        }
        std::cout << "批量写入 " << total_written << " 个元素耗时: " 
                  << std::fixed << std::setprecision(2) << timer.elapsed_ms() << " ms" << std::endl;
    }
    
    // 批量读取测试
    {
        Timer timer;
        size_t total_read = 0;
        size_t read_count;
        while ((read_count = buffer.pop_batch(read_data.data(), BATCH_SIZE)) > 0) {
            total_read += read_count;
        }
        std::cout << "批量读取 " << total_read << " 个元素耗时: " 
                  << std::fixed << std::setprecision(2) << timer.elapsed_ms() << " ms" << std::endl;
    }
}

// 多线程测试
void multi_thread_test() {
    std::cout << "\n=== 多线程测试 ===" << std::endl;
    
    constexpr size_t BUFFER_SIZE = 8192;
    constexpr size_t MESSAGES_PER_THREAD = 500000;
    
    RingBuffer<int, BUFFER_SIZE> buffer;
    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> total_produced{0};
    std::atomic<size_t> total_consumed{0};
    
    // 生产者线程
    auto producer = [&]() {
        size_t produced = 0;
        while (produced < MESSAGES_PER_THREAD) {
            if (buffer.push(static_cast<int>(produced))) {
                ++produced;
                ++total_produced;
            } else {
                // 缓冲区满，短暂休眠
                std::this_thread::yield();
            }
        }
    };
    
    // 消费者线程
    auto consumer = [&]() {
        int value;
        while (!stop_flag.load() || !buffer.empty()) {
            if (buffer.pop(value)) {
                ++total_consumed;
            } else {
                std::this_thread::yield();
            }
        }
    };
    
    Timer timer;
    
    // 启动线程
    std::thread producer_thread(producer);
    std::thread consumer_thread(consumer);
    
    // 等待生产者完成
    producer_thread.join();
    stop_flag.store(true);
    
    // 等待消费者完成
    consumer_thread.join();
    
    double elapsed = timer.elapsed_ms();
    
    std::cout << "生产者生产: " << total_produced.load() << " 个消息" << std::endl;
    std::cout << "消费者消费: " << total_consumed.load() << " 个消息" << std::endl;
    std::cout << "总耗时: " << std::fixed << std::setprecision(2) << elapsed << " ms" << std::endl;
    std::cout << "吞吐量: " << std::fixed << std::setprecision(0) 
              << (total_consumed.load() * 1000.0 / elapsed) << " 消息/秒" << std::endl;
}

// 基本功能演示
void basic_demo() {
    std::cout << "=== 基本功能演示 ===" << std::endl;
    
    RingBuffer<std::string, 8> buffer;
    
    // 写入一些数据
    std::cout << "写入数据..." << std::endl;
    for (int i = 0; i < 5; ++i) {
        std::string data = "message_" + std::to_string(i);
        if (buffer.push(data)) {
            std::cout << "  成功写入: " << data << std::endl;
        }
    }
    
    std::cout << "缓冲区状态: size=" << buffer.size() 
              << ", capacity=" << buffer.capacity()
              << ", empty=" << buffer.empty()
              << ", full=" << buffer.full() << std::endl;
    
    // 读取数据
    std::cout << "\n读取数据..." << std::endl;
    std::string item;
    while (buffer.pop(item)) {
        std::cout << "  读取到: " << item << std::endl;
    }
    
    std::cout << "缓冲区状态: size=" << buffer.size() 
              << ", empty=" << buffer.empty() << std::endl;
}

// 内存使用效率测试
void memory_efficiency_test() {
    std::cout << "\n=== 内存使用效率测试 ===" << std::endl;
    
    constexpr size_t BUFFER_SIZE = 1024;
    RingBuffer<int, BUFFER_SIZE> buffer;
    
    std::cout << "RingBuffer<int, " << BUFFER_SIZE << "> 大小: " 
              << sizeof(buffer) << " 字节" << std::endl;
    std::cout << "理论最小大小: " << BUFFER_SIZE * sizeof(int) + 2 * sizeof(std::atomic<size_t>) 
              << " 字节" << std::endl;
    std::cout << "内存对齐开销: " << sizeof(buffer) - (BUFFER_SIZE * sizeof(int) + 2 * sizeof(std::atomic<size_t>)) 
              << " 字节" << std::endl;
}

int main() {
    std::cout << "高效循环缓冲区 (Ring Buffer) 演示程序\n" << std::endl;
    
    try {
        basic_demo();
        single_thread_test();
        batch_operations_test();
        multi_thread_test();
        memory_efficiency_test();
        
        std::cout << "\n所有测试完成！" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}