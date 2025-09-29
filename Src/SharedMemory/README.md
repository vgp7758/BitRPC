# BitRPC 跨进程共享内存通信库

一个高性能的SPSC（Single Producer Single Consumer）环形缓冲区实现，支持C++、C#和Python之间的跨进程通信。

## 🚀 主要特性

### 🔧 核心功能
- **高性能SPSC环形缓冲区**：无锁设计，支持高并发
- **跨语言支持**：C++、C#、Python无缝互通
- **内存安全**：自动内存管理和边界检查
- **事件通知**：跨进程事件同步机制
- **类型安全**：强类型消息处理
- **统计信息**：详细的性能统计

### 📊 性能指标
- **吞吐量**：每秒数百万消息
- **延迟**：微秒级延迟
- **内存效率**：零拷贝设计
- **跨进程**：支持同一主机上的多进程通信

## 🏗️ 架构设计

### 内存布局
```
+------------------------+---------------------------+
|        头部元数据       |          数据缓冲区          |
| RingBufferHeader       |         Ring Data           |
|  - 写位置               |                           |
|  - 读位置               |                           |
|  - 缓冲区大小           |                           |
|  - 魔数/版本            |                           |
+------------------------+---------------------------+
```

### 同步机制
- **无锁设计**：SPSC模式避免锁竞争
- **内存屏障**：确保内存可见性
- **事件通知**：跨进程信号量/事件
- **原子操作**：64位原子读写位置

## 🛠️ 构建和安装

### Windows
```batch
# 运行Windows构建脚本
cd Src/SharedMemory
build.bat
```

### Linux/macOS
```bash
# 运行Unix构建脚本
cd Src/SharedMemory
chmod +x build.sh
./build.sh
```

### 构建产物
- `lib/` - 共享库文件
- `include/` - C++头文件
- `examples/` - 示例代码
- `python/` - Python API
- `csharp/` - C# API

## 📚 快速开始

### C++ 示例

```cpp
#include "shared_memory_api.h"
using namespace bitrpc::shared_memory;

// 创建生产者
auto producer = create_producer("test_buffer", 1024 * 1024);
if (producer) {
    // 发送字符串
    producer->send_string("Hello from C++!");

    // 发送结构化数据
    struct TestData { int id; double value; };
    TestData data = {1, 3.14159};
    producer->send_typed(data);

    producer->dispose();
}

// 创建消费者
auto consumer = create_consumer("test_buffer", 1024 * 1024);
if (consumer) {
    // 接收字符串
    std::string message;
    if (consumer->receive_string(message)) {
        std::cout << "Received: " << message << std::endl;
    }

    // 接收结构化数据
    TestData data;
    if (consumer->receive_typed(data)) {
        std::cout << "Received: id=" << data.id << ", value=" << data.value << std::endl;
    }

    consumer->dispose();
}
```

### C# 示例

```csharp
using BitRPC.SharedMemory;

// 创建生产者
var producer = SharedMemoryFactory.CreateProducer("test_buffer", 1024 * 1024);
if (producer != null) {
    // 发送字符串
    producer.SendString("Hello from C#!");

    // 发送结构化数据
    var message = new SharedMemoryMessage(MessageType.Data, dataBytes);
    producer.SendMessage(message);

    producer.Dispose();
}

// 创建消费者
var consumer = SharedMemoryFactory.CreateConsumer("test_buffer", 1024 * 1024);
if (consumer != null) {
    // 接收字符串
    string message = consumer.ReceiveString();
    Console.WriteLine($"Received: {message}");

    // 接收消息
    if (consumer.ReceiveMessage(out var msg)) {
        Console.WriteLine($"Received: type={msg.MessageType}, size={msg.Payload.Length}");
    }

    consumer.Dispose();
}
```

### Python 示例

```python
from shared_memory_api import create_producer, create_consumer

# 创建生产者
producer = create_producer("test_buffer", 1024 * 1024)
if producer:
    # 发送字符串
    producer.send_string("Hello from Python!")

    # 发送结构化数据
    import struct
    data = struct.pack('<id', 1, 3.14159)
    producer.send(data)

    producer.dispose()

# 创建消费者
consumer = create_consumer("test_buffer", 1024 * 1024)
if consumer:
    # 接收字符串
    message = consumer.receive_string()
    print(f"Received: {message}")

    # 接收原始数据
    data = consumer.receive()
    print(f"Received: {len(data)} bytes")

    consumer.dispose()
```

## 🔄 跨语言通信测试

运行完整的跨语言通信测试：

### 步骤1：启动生产者（任选一种语言）

**C++生产者：**
```bash
cd build/dist/examples
./cross_language_test --mode producer --name CrossLangTest
```

**C#生产者：**
```bash
cd build/dist/examples
./cross_language_test.exe --mode producer --name CrossLangTest
```

**Python生产者：**
```bash
cd build/dist/examples
python3 cross_language_test.py --mode producer --name CrossLangTest
```

### 步骤2：启动消费者（不同语言）

**C++消费者：**
```bash
./cross_language_test --mode consumer --name CrossLangTest
```

**C#消费者：**
```bash
./cross_language_test.exe --mode consumer --name CrossLangTest
```

**Python消费者：**
```bash
python3 cross_language_test.py --mode consumer --name CrossLangTest
```

### 预期输出
生产者将发送：
- 文本消息（字符串）
- 结构化数据（TestData结构）
- 心跳消息

消费者将接收并解析：
- 来自不同语言的文本消息
- 跨语言的结构化数据
- 心跳消息

## 📖 详细API文档

### C++ API

#### 核心类
- `RingBuffer`：基础环形缓冲区
- `SharedMemoryProducer`：生产者封装
- `SharedMemoryConsumer`：消费者封装
- `SharedMemoryManager`：高级管理器

#### 工厂方法
```cpp
auto producer = create_producer("name", size);
auto consumer = create_consumer("name", size);
auto manager = create_manager("name", is_producer, size);
```

### C# API

#### 核心类
- `RingBuffer`：基础环形缓冲区
- `SharedMemoryProducer`：生产者封装
- `SharedMemoryConsumer`：消费者封装
- `SharedMemoryManager`：高级管理器

#### 工厂方法
```csharp
var producer = SharedMemoryFactory.CreateProducer("name", size);
var consumer = SharedMemoryFactory.CreateConsumer("name", size);
var manager = SharedMemoryFactory.CreateManager("name", isProducer, size);
```

### Python API

#### 核心类
- `RingBuffer`：基础环形缓冲区
- `SharedMemoryProducer`：生产者封装
- `SharedMemoryConsumer`：消费者封装
- `SharedMemoryManager`：高级管理器

#### 工厂方法
```python
producer = create_producer("name", size)
consumer = create_consumer("name", size)
manager = create_manager("name", is_producer, size)
```

## ⚙️ 高级特性

### 消息类型系统
```cpp
enum class MessageType : uint32_t {
    DATA = 1,
    CONTROL = 2,
    HEARTBEAT = 3,
    ERROR = 4,
    CUSTOM_MIN = 1000
};
```

### 消息标志
```cpp
enum class MessageFlags : uint8_t {
    NONE = 0,
    URGENT = 0x01,
    COMPRESSED = 0x02,
    ENCRYPTED = 0x04,
    LAST_FRAGMENT = 0x08
};
```

### 统计信息
```cpp
struct Statistics {
    uint64_t messages_sent{0};
    uint64_t messages_received{0};
    uint64_t bytes_sent{0};
    uint64_t bytes_received{0};
    uint64_t errors{0};
    double avg_message_size{0.0};
};
```

## 🔧 故障排除

### 常见问题

1. **库加载失败**
   - 确保共享库在PATH中
   - 检查依赖库是否完整

2. **权限错误**
   - 确保有创建共享内存的权限
   - 在Linux上可能需要root权限

3. **连接失败**
   - 确保生产者先启动
   - 检查共享内存名称是否匹配

4. **内存不足**
   - 调整缓冲区大小
   - 检查系统可用内存

### 调试技巧
- 启用详细日志输出
- 使用调试模式编译
- 检查系统错误信息
- 监控内存使用情况

## 📝 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 🤝 贡献

欢迎提交Issue和Pull Request！

## 📞 联系方式

如有问题或建议，请通过以下方式联系：
- GitHub Issues
- Email: your-email@example.com

---

**BitRPC Team** - 高性能跨语言通信解决方案