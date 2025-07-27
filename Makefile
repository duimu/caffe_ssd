CXX = g++
CXXFLAGS = -std=c++17 -O3 -Wall -Wextra -pthread
TARGET = ring_buffer_demo
TARGET2 = network_example
SOURCES = demo.cpp
SOURCES2 = example_producer_consumer.cpp
HEADERS = ring_buffer.hpp

# 默认目标
all: $(TARGET) $(TARGET2)

# 编译主程序
$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# 编译网络示例
$(TARGET2): $(SOURCES2) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET2) $(SOURCES2)

# Debug版本
debug: CXXFLAGS = -std=c++17 -g -Wall -Wextra -pthread -DDEBUG
debug: $(TARGET)

# 性能分析版本
profile: CXXFLAGS = -std=c++17 -O2 -pg -Wall -Wextra -pthread
profile: $(TARGET)

# 清理
clean:
	rm -f $(TARGET) $(TARGET2) gmon.out

# 运行基础测试
run: $(TARGET)
	./$(TARGET)

# 运行网络示例
run-network: $(TARGET2)
	./$(TARGET2)

# 帮助信息
help:
	@echo "可用的目标:"
	@echo "  all        - 编译所有程序 (默认)"
	@echo "  debug      - 编译调试版本"
	@echo "  profile    - 编译性能分析版本"
	@echo "  clean      - 清理生成的文件"
	@echo "  run        - 运行基础测试程序"
	@echo "  run-network - 运行网络数据包处理示例"
	@echo "  help       - 显示此帮助信息"

.PHONY: all debug profile clean run run-network help
