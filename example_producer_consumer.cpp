#include "ring_buffer.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <random>
#include <iomanip>

// 模拟网络数据包
struct NetworkPacket {
    uint32_t packet_id;
    uint16_t source_port;
    uint16_t dest_port;
    uint32_t timestamp;
    uint16_t payload_size;
    char payload[1024];  // 模拟有效载荷
    
    NetworkPacket() : packet_id(0), source_port(0), dest_port(0), 
                     timestamp(0), payload_size(0) {
        std::memset(payload, 0, sizeof(payload));
    }
    
    NetworkPacket(uint32_t id, uint16_t sport, uint16_t dport, 
                  uint16_t size) 
        : packet_id(id), source_port(sport), dest_port(dport),
          timestamp(static_cast<uint32_t>(
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch()).count())),
          payload_size(size) {
        // 生成随机有效载荷
        for (int i = 0; i < size && i < sizeof(payload); ++i) {
            payload[i] = static_cast<char>('A' + (i % 26));
        }
    }
};

// 统计信息结构
struct Statistics {
    std::atomic<uint64_t> packets_produced{0};
    std::atomic<uint64_t> packets_consumed{0};
    std::atomic<uint64_t> bytes_produced{0};
    std::atomic<uint64_t> bytes_consumed{0};
    std::atomic<uint64_t> buffer_full_count{0};
    std::atomic<uint64_t> buffer_empty_count{0};
    
    void print_stats(double elapsed_seconds) const {
        uint64_t produced = packets_produced.load();
        uint64_t consumed = packets_consumed.load();
        uint64_t bytes_prod = bytes_produced.load();
        uint64_t bytes_cons = bytes_consumed.load();
        
        std::cout << "\n=== 统计信息 ===" << std::endl;
        std::cout << "运行时间: " << std::fixed << std::setprecision(2) 
                  << elapsed_seconds << " 秒" << std::endl;
        std::cout << "数据包生产: " << produced << " 个" << std::endl;
        std::cout << "数据包消费: " << consumed << " 个" << std::endl;
        std::cout << "数据生产: " << (bytes_prod / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "数据消费: " << (bytes_cons / 1024.0 / 1024.0) << " MB" << std::endl;
        std::cout << "生产速率: " << (produced / elapsed_seconds) << " 包/秒" << std::endl;
        std::cout << "消费速率: " << (consumed / elapsed_seconds) << " 包/秒" << std::endl;
        std::cout << "吞吐量: " << (bytes_cons / elapsed_seconds / 1024.0 / 1024.0) 
                  << " MB/秒" << std::endl;
        std::cout << "缓冲区满次数: " << buffer_full_count.load() << std::endl;
        std::cout << "缓冲区空次数: " << buffer_empty_count.load() << std::endl;
    }
};

// 网络数据生产者
class NetworkProducer {
private:
    RingBuffer<NetworkPacket, 4096>& buffer_;
    Statistics& stats_;
    std::mt19937 rng_;
    std::uniform_int_distribution<uint16_t> port_dist_;
    std::uniform_int_distribution<uint16_t> size_dist_;
    
public:
    NetworkProducer(RingBuffer<NetworkPacket, 4096>& buffer, Statistics& stats)
        : buffer_(buffer), stats_(stats), rng_(std::random_device{}()),
          port_dist_(1024, 65535), size_dist_(64, 1024) {}
    
    void run(int duration_seconds) {
        auto start_time = std::chrono::steady_clock::now();
        auto end_time = start_time + std::chrono::seconds(duration_seconds);
        
        uint32_t packet_id = 1;
        
        while (std::chrono::steady_clock::now() < end_time) {
            // 模拟网络数据包到达
            uint16_t sport = port_dist_(rng_);
            uint16_t dport = port_dist_(rng_);
            uint16_t size = size_dist_(rng_);
            
            NetworkPacket packet(packet_id++, sport, dport, size);
            
            if (buffer_.push(std::move(packet))) {
                stats_.packets_produced.fetch_add(1);
                stats_.bytes_produced.fetch_add(size);
            } else {
                stats_.buffer_full_count.fetch_add(1);
                // 模拟背压处理：短暂等待
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
            
            // 模拟网络数据包之间的间隔
            if (packet_id % 1000 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }
        
        std::cout << "生产者停止，总共生产 " << packet_id - 1 << " 个数据包" << std::endl;
    }
};

// 网络数据处理器
class NetworkProcessor {
private:
    RingBuffer<NetworkPacket, 4096>& buffer_;
    Statistics& stats_;
    
    // 模拟数据包处理
    void process_packet(const NetworkPacket& packet) {
        // 模拟一些处理工作
        volatile int sum = 0;
        for (int i = 0; i < packet.payload_size; ++i) {
            sum += packet.payload[i];
        }
        
        // 模拟日志记录（每1000个包打印一次）
        if (packet.packet_id % 1000 == 0) {
            std::cout << "处理数据包 #" << packet.packet_id 
                      << " (端口: " << packet.source_port << "->" << packet.dest_port
                      << ", 大小: " << packet.payload_size << " 字节)" << std::endl;
        }
    }
    
public:
    NetworkProcessor(RingBuffer<NetworkPacket, 4096>& buffer, Statistics& stats)
        : buffer_(buffer), stats_(stats) {}
    
    void run(const std::atomic<bool>& should_stop) {
        NetworkPacket packet;
        
        while (!should_stop.load() || !buffer_.empty()) {
            if (buffer_.pop(packet)) {
                process_packet(packet);
                stats_.packets_consumed.fetch_add(1);
                stats_.bytes_consumed.fetch_add(packet.payload_size);
            } else {
                stats_.buffer_empty_count.fetch_add(1);
                // 缓冲区空，短暂休眠
                std::this_thread::yield();
            }
        }
        
        std::cout << "处理器停止" << std::endl;
    }
};

// 批量处理器（展示批量操作的效率）
class BatchProcessor {
private:
    RingBuffer<NetworkPacket, 4096>& buffer_;
    Statistics& stats_;
    static constexpr size_t BATCH_SIZE = 32;
    
public:
    BatchProcessor(RingBuffer<NetworkPacket, 4096>& buffer, Statistics& stats)
        : buffer_(buffer), stats_(stats) {}
    
    void run(const std::atomic<bool>& should_stop) {
        std::vector<NetworkPacket> batch(BATCH_SIZE);
        
        while (!should_stop.load() || !buffer_.empty()) {
            size_t read_count = buffer_.pop_batch(batch.data(), BATCH_SIZE);
            
            if (read_count > 0) {
                // 批量处理
                for (size_t i = 0; i < read_count; ++i) {
                    // 简单的处理逻辑
                    volatile int checksum = 0;
                    for (int j = 0; j < batch[i].payload_size; ++j) {
                        checksum ^= batch[i].payload[j];
                    }
                    
                    stats_.packets_consumed.fetch_add(1);
                    stats_.bytes_consumed.fetch_add(batch[i].payload_size);
                }
                
                // 每处理一批数据打印一次状态
                if (stats_.packets_consumed.load() % 10000 == 0) {
                    std::cout << "批量处理器已处理 " << stats_.packets_consumed.load() 
                              << " 个数据包" << std::endl;
                }
            } else {
                stats_.buffer_empty_count.fetch_add(1);
                std::this_thread::yield();
            }
        }
        
        std::cout << "批量处理器停止" << std::endl;
    }
};

// 监控线程
class BufferMonitor {
private:
    RingBuffer<NetworkPacket, 4096>& buffer_;
    
public:
    BufferMonitor(RingBuffer<NetworkPacket, 4096>& buffer) : buffer_(buffer) {}
    
    void run(const std::atomic<bool>& should_stop) {
        while (!should_stop.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            size_t current_size = buffer_.size();
            size_t capacity = buffer_.capacity();
            double usage_percent = (current_size * 100.0) / capacity;
            
            std::cout << "[监控] 缓冲区使用率: " << std::fixed << std::setprecision(1)
                      << usage_percent << "% (" << current_size << "/" << capacity << ")"
                      << std::endl;
        }
    }
};

int main() {
    std::cout << "网络数据包处理演示程序" << std::endl;
    std::cout << "展示高效循环缓冲区在实际场景中的应用\n" << std::endl;
    
    // 创建缓冲区和统计对象
    RingBuffer<NetworkPacket, 4096> buffer;
    Statistics stats;
    
    // 控制变量
    std::atomic<bool> should_stop{false};
    const int test_duration = 5; // 运行5秒
    
    // 创建工作对象
    NetworkProducer producer(buffer, stats);
    
    // 选择处理模式
    std::cout << "选择处理模式:" << std::endl;
    std::cout << "1. 单个处理器" << std::endl;
    std::cout << "2. 批量处理器" << std::endl;
    std::cout << "请输入选择 (1 或 2): ";
    
    int choice;
    std::cin >> choice;
    
    auto start_time = std::chrono::steady_clock::now();
    
    if (choice == 2) {
        // 批量处理模式
        BatchProcessor processor(buffer, stats);
        BufferMonitor monitor(buffer);
        
        std::cout << "\n启动批量处理模式..." << std::endl;
        
        // 启动线程
        std::thread producer_thread([&producer, test_duration]() {
            producer.run(test_duration);
        });
        
        std::thread processor_thread([&processor, &should_stop]() {
            processor.run(should_stop);
        });
        
        std::thread monitor_thread([&monitor, &should_stop]() {
            monitor.run(should_stop);
        });
        
        // 等待生产者完成
        producer_thread.join();
        should_stop = true;
        
        // 等待其他线程完成
        processor_thread.join();
        monitor_thread.join();
    } else {
        // 单个处理模式
        NetworkProcessor processor(buffer, stats);
        BufferMonitor monitor(buffer);
        
        std::cout << "\n启动单个处理模式..." << std::endl;
        
        // 启动线程
        std::thread producer_thread([&producer, test_duration]() {
            producer.run(test_duration);
        });
        
        std::thread processor_thread([&processor, &should_stop]() {
            processor.run(should_stop);
        });
        
        std::thread monitor_thread([&monitor, &should_stop]() {
            monitor.run(should_stop);
        });
        
        // 等待生产者完成
        producer_thread.join();
        should_stop = true;
        
        // 等待其他线程完成
        processor_thread.join();
        monitor_thread.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    // 打印统计信息
    stats.print_stats(elapsed);
    
    // 验证数据完整性
    uint64_t produced = stats.packets_produced.load();
    uint64_t consumed = stats.packets_consumed.load();
    
    std::cout << "\n=== 数据完整性检查 ===" << std::endl;
    if (produced == consumed) {
        std::cout << "✓ 所有数据包都被正确处理" << std::endl;
    } else {
        std::cout << "⚠ 数据包丢失: " << (produced - consumed) << " 个" << std::endl;
    }
    
    return 0;
}