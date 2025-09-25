# BitRPC C++ Runtime Library

本目录包含BitRPC框架的C++运行时库实现，与C#实现保持功能一致。

## 已完成的功能

### 1. 序列化框架 (Serialization Framework)
- **BitMask**: 完整的位掩码实现，支持32位分组和动态扩展
- **TypeHandler**: 类型处理器接口和基础类型实现
- **StreamWriter/StreamReader**: 流式读写器，支持各种数据类型
- **BufferSerializer**: 单例模式的序列化管理器

### 2. 基础类型处理器
- `Int32Handler` (Hash Code: 101)
- `Int64Handler` (Hash Code: 102)
- `FloatHandler` (Hash Code: 103)
- `DoubleHandler` (Hash Code: 104)
- `BoolHandler` (Hash Code: 105)
- `StringHandler` (Hash Code: 106)
- `BytesHandler` (Hash Code: 107)

### 3. 网络通信层
- **TcpRpcClient**: 完整的TCP客户端实现，支持跨平台
- **TcpRpcServer**: 多线程TCP服务端实现
- **ServiceBase**: 服务基类，支持方法注册和调用

### 4. 平台支持
- Windows (Winsock2)
- Linux/Unix (BSD sockets)
- 跨平台网络抽象

## 与C#实现的一致性

### 序列化协议一致性
- ✅ 位掩码序列化算法完全一致
- ✅ 类型处理器Hash Code完全一致
- ✅ 流读写器字节格式完全一致
- ✅ 网络通信协议完全一致

### 功能特性对比
| 特性 | C# | C++ | 状态 |
|------|----|----|------|
| BitMask序列化 | ✅ | ✅ | 完全一致 |
| 类型处理器 | ✅ | ✅ | 完全一致 |
| 流读写器 | ✅ | ✅ | 完全一致 |
| TCP客户端 | ✅ | ✅ | 功能一致 |
| TCP服务端 | ✅ | ✅ | 功能一致 |
| 服务注册 | ✅ | ✅ | 功能一致 |
| 方法调用 | ✅ | ✅ | 功能一致 |
| 跨平台 | ✅ | ✅ | 都支持 |

## 使用示例

### 基本序列化
```cpp
#include "rpc_core.h"

using namespace bitrpc;

// 序列化字符串
std::string test_str = "Hello, BitRPC!";
StreamWriter writer;
writer.write_string(test_str);
auto data = writer.to_array();

// 反序列化
StreamReader reader(data);
std::string result = reader.read_string();
```

### 使用BufferSerializer
```cpp
auto& serializer = BufferSerializer::instance();

// 注册自定义类型处理器
serializer.register_handler<MyType>(std::make_shared<MyTypeHandler>());

// 序列化对象
MyType obj;
StreamWriter writer;
writer.write_object(&obj, typeid(MyType).hash_code());
```

### 创建RPC服务
```cpp
class MyService : public ServiceBase {
public:
    MyService() : ServiceBase("MyService") {
        register_method<std::string, std::string>("echo",
            [](const std::string* req) -> std::string* {
                return new std::string("Echo: " + *req);
            });
    }
};

// 使用服务
auto service = std::make_shared<MyService>();
TcpRpcServer server;
server.register_service(service);
server.start(8080);
```

### 使用RPC客户端
```cpp
TcpRpcClient client;
client.connect("localhost", 8080);

// 调用远程方法
std::string method = "MyService.echo";
std::vector<uint8_t> request = /* 序列化的请求 */;
auto response = client.call(method, request);

client.disconnect();
```

## 构建说明

### 使用CMake
```bash
mkdir build
cd build
cmake ..
make
```

### 使用Visual Studio
1. 创建新的C++项目
2. 添加 `rpc_core.cpp` 和 `rpc_core.h` 文件
3. 链接 `ws2_32.lib` (Windows)
4. 设置C++17标准

### 编译要求
- C++17 或更高版本
- Windows: Winsock2
- Linux: 标准socket库

## 测试

运行测试程序验证功能：
```bash
./bitrpc_test
```

测试覆盖：
- 基本序列化/反序列化
- BitMask功能
- 类型处理器注册
- 服务方法调用
- 网络组件初始化

## 注意事项

1. **内存管理**: C++实现使用new/delete分配对象，调用者负责释放内存
2. **线程安全**: 服务端实现使用多线程，确保方法调用是线程安全的
3. **错误处理**: 所有网络操作都有异常处理
4. **平台兼容**: 代码在Windows和Linux上都经过测试

## 与Python实现的对比

Python实现使用了pickle简化序列化，而C++实现完全按照C#的BitMask序列化规范实现，确保三种语言间的互操作性。