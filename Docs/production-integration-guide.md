# 生产环境集成指南

本文档详细说明了如何将BitRPC框架集成到生产环境中。

## 目录结构

```
BitRPC/
├── Docs/                    # 文档
├── Examples/                # 示例协议定义
├── Generated/               # 生成的代码
├── Src/                    # 源代码
│   ├── C++Core/            # C++运行时库
│   ├── Client/             # 客户端框架
│   ├── Generator/          # 代码生成器
│   ├── GeneratorApp/       # 生成器应用程序
│   ├── Parser/             # 协议解析器
│   ├── Python/             # Python运行时库
│   ├── Serialization/      # 序列化框架
│   └── Server/             # 服务端框架
├── generator-config.json   # 生成器配置文件
├── generate-all.bat        # 生成所有语言代码的脚本
├── generate-csharp.bat     # 生成C#代码的脚本
├── generate-python.bat     # 生成Python代码的脚本
├── generate-cpp.bat        # 生成C++代码的脚本
└── install-dependencies.bat # 安装依赖的脚本
```

## 1. 配置文件详解

`generator-config.json` 是代码生成器的主要配置文件，用于指定协议文件、输出目录和各语言的生成选项。

```json
{
  "protocolFile": "Examples/user_service.pdl",  // 协议定义文件路径
  "outputDirectory": "Generated",               // 生成代码的输出目录
  "languages": [                                // 各语言的配置
    {
      "name": "csharp",                         // 语言名称
      "enabled": true,                         // 是否启用该语言生成
      "namespace": "Example.Protocol",         // 命名空间
      "runtimePath": "Src"                     // 运行时库路径
    },
    {
      "name": "python",
      "enabled": true,
      "namespace": "example.protocol",
      "runtimePath": "Src/Python"
    },
    {
      "name": "cpp",
      "enabled": true,
      "namespace": "example.protocol",
      "runtimePath": "Src/C++Core"
    }
  ]
}
```

## 2. 代码生成流程

### 2.1 安装依赖

首次使用前需要安装必要的依赖：

```bash
install-dependencies.bat
```

### 2.2 生成代码

有多种方式可以生成代码：

#### 生成所有语言的代码
```bash
generate-all.bat
```

#### 生成特定语言的代码
```bash
generate-csharp.bat    # 生成C#代码
generate-python.bat    # 生成Python代码
generate-cpp.bat       # 生成C++代码
```

#### 使用命令行直接生成
```bash
dotnet run --project Src/GeneratorApp -- generator-config.json
```

## 3. 各语言集成指南

### 3.1 C# 集成

#### 3.1.1 复制运行时库
将以下文件复制到您的C#项目中：
- `Src/Serialization/BitMaskSerialization.cs`
- `Src/Client/RpcClient.cs`
- `Src/Server/RpcServer.cs`

#### 3.1.2 复制生成的协议代码
将 `Generated/csharp/` 目录下的所有文件复制到您的项目中。

#### 3.1.3 服务端实现
```csharp
var server = new RpcServer(8080);
server.RegisterService("UserService", new UserServiceImplementation());
server.Start();
```

#### 3.1.4 客户端使用
```csharp
var client = new TcpRpcClient("localhost", 8080);
var userService = new UserServiceClient(client);
await client.ConnectAsync();
var response = await userService.GetUserAsync(new GetUserRequest { UserId = 123 });
```

### 3.2 Python 集成

#### 3.2.1 复制运行时库
将 `Src/Python/bitrpc/` 目录复制到您的Python项目中。

#### 3.2.2 复制生成的协议代码
将 `Generated/python/` 目录下的所有文件复制到您的项目中。

#### 3.2.3 客户端使用
```python
client = TcpRpcClient("localhost", 8080)
user_service = UserServiceClient(client)
await client.connect_async()
response = await user_service.GetUser_async(GetUserRequest(user_id=123))
```

### 3.3 C++ 集成

#### 3.3.1 复制运行时库
将 `Src/C++Core/` 目录下的所有文件复制到您的C++项目中。

#### 3.3.2 复制生成的协议代码
将 `Generated/cpp/` 目录下的所有文件复制到您的项目中。

#### 3.3.3 构建配置
按照生成的 `Generated/cpp/CMakeLists.txt` 配置您的构建系统。

## 4. 生产环境最佳实践

### 4.1 版本控制
- 将协议定义文件(.pdl)纳入版本控制
- 确保所有团队成员使用相同版本的协议定义

### 4.2 自动化构建
- 将代码生成过程集成到CI/CD流水线中
- 确保协议更新时自动生成代码

### 4.3 文档同步
- 维护协议文档与实现代码的同步
- 使用工具自动生成API文档

### 4.4 向后兼容
- 在更新协议时考虑向后兼容性
- 避免破坏现有服务

### 4.5 安全考虑
- 在生产环境中实施适当的安全措施
- 实现身份验证和授权机制
- 对敏感数据进行加密传输

## 5. 故障排除

### 5.1 生成器无法运行
- 确保已安装.NET 6.0或更高版本
- 运行 `install-dependencies.bat` 安装必要的NuGet包

### 5.2 生成的代码无法编译
- 确保复制了所有必要的运行时库文件
- 检查命名空间和引用是否正确

### 5.3 运行时错误
- 检查网络连接和端口配置
- 确保服务端和客户端使用相同的协议版本

## 6. 扩展和定制

### 6.1 添加新的目标语言
1. 实现新的代码生成器类（继承自 `BaseCodeGenerator`）
2. 在 `CodeGeneratorFactory` 中注册新的生成器
3. 更新配置文件以支持新语言

### 6.2 自定义序列化
1. 实现新的类型处理器（实现 `ITypeHandler` 接口）
2. 在 `BufferSerializer` 中注册新的处理器

### 6.3 扩展协议定义语言
1. 修改 `PDLParser` 以支持新的语法结构
2. 更新各语言的代码生成器以处理新的结构