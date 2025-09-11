# 网络通信协议框架设计文档

## 1. 概述

### 1.1 项目背景
基于位掩码序列化方案，设计一个通用的网络通信协议框架。该框架通过定义协议的数据结构和服务配置，利用代码生成工具自动生成多种目标语言（Python/C#/C++等）的数据结构定义、客户端/服务端代码及相关工厂类。

### 1.2 设计目标
- 支持多种目标语言代码生成（Python/C#/C++等）
- 基于位掩码序列化技术，优化网络传输效率
- 通过定义文件驱动代码生成，提高开发效率
- 支持服务接口定义和自动生成客户端/服务端框架代码
- 提供统一的序列化/反序列化接口

## 2. 架构设计

### 2.1 整体架构
``mermaid
graph TD
    A[协议定义文件] --> B(代码生成器)
    B --> C1[C#代码]
    B --> C2[Python代码]
    B --> C3[C++代码]
    C1 --> D1[C#客户端/服务端]
    C2 --> D2[Python客户端/服务端]
    C3 --> D3[C++客户端/服务端]
    E[序列化核心] --> F[位掩码序列化]
```

### 2.2 核心组件
1. **协议定义语言（PDL）**：用于定义数据结构和服务接口的DSL
2. **代码生成器**：解析协议定义文件并生成目标语言代码
3. **序列化框架**：基于位掩码技术的序列化/反序列化实现
4. **运行时库**：各语言的运行时支持库

## 3. 协议定义语言设计

### 3.1 语法结构
```
// 数据结构定义
message MessageName {
    field_type field_name = field_id;
    repeated item_type field_name = 2; //列表
    // 更多字段...
}

// 服务接口定义
service ServiceName {
    rpc MethodName(RequestType) returns (ResponseType);
    // 更多方法...
}
```

### 3.2 数据类型支持
- 基本类型：int32, int64, float, double, bool, string
- 复合类型：struct, list, map
- 语言特定类型：Vector3, DateTime等

## 4. 代码生成器设计

### 4.1 核心功能
- 解析协议定义文件
- 生成数据结构类
- 生成序列化/反序列化代码
- 生成客户端/服务端框架代码
- 生成工厂类和注册代码

### 4.2 生成内容结构
```
generated/
├── language/                 # 目标语言目录
│   ├── data/                 # 数据结构定义
│   ├── serialization/        # 序列化相关代码
│   ├── client/               # 客户端框架代码
│   ├── server/               # 服务端框架代码
│   └── factory/              # 工厂类和注册代码
```

## 5. 序列化框架设计

### 5.1 位掩码序列化原理
- 每32个字段分为一组
- 使用位掩码标识哪些字段需要序列化（非默认值）
- 只传输有变化的字段数据，减少传输量

### 5.2 核心接口设计
```csharp
// 类型处理器接口
public interface ITypeHandler
{
    int HashCode { get; }
    void Write(object obj, StreamWriter writer);
    object Read(StreamReader reader);
}

// 序列化器接口
public interface IBufferSerializer
{
    void RegisterHandler<T>(ITypeHandler handler);
    ITypeHandler GetHandler(Type type);
    void InitHandlers();
}
```

## 6. 多语言支持设计

### 6.1 C#语言支持
- 基于现有BitRPC框架实现
- 利用特性标记和代码生成机制
- 支持Unity引擎集成

### 6.2 Python语言支持
- 实现与C#相同的接口
- 使用装饰器替代特性标记
- 提供Python风格的API

### 6.3 C++语言支持
- 使用模板和宏实现类型处理
- 提供STL容器支持
- 实现与C#兼容的序列化协议

## 7. 服务框架设计

### 7.1 客户端框架
- 服务代理生成
- 网络通信封装
- 异步调用支持

### 7.2 服务端框架
- 服务实现模板生成
- 请求分发机制
- 连接管理

## 8. 配置文件设计

### 8.1 服务配置
```yaml
service:
  name: "ExampleService"
  version: "1.0.0"
  port: 8080
  protocol: "tcp"
```

### 8.2 生成配置
```yaml
generation:
  target_languages: ["csharp", "python", "cpp"]
  output_directory: "./generated"
  namespace: "Example.Protocol"
```
