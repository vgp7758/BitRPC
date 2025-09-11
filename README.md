# BitRPC Protocol Framework

基于位掩码序列化技术的多语言网络通信协议框架。

## 功能特性

- **多语言支持**: C#、Python、C++
- **位掩码序列化**: 高效的网络传输
- **代码生成**: 从协议定义文件自动生成目标语言代码
- **客户端/服务端框架**: 自动生成RPC客户端和服务端代码
- **类型安全**: 强类型的数据结构定义

## 项目结构

```
BitRPC/
├── Docs/                           # 文档
│   └── protocol-framework-design.md
├── Examples/                       # 示例协议定义
│   └── user_service.pdl
├── Generated/                      # 生成的代码
├── Src/                           # 源代码
│   ├── C++Core/                   # C++运行时库
│   ├── Client/                    # 客户端框架
│   ├── Generator/                 # 代码生成器
│   │   ├── CodeGeneratorCore.cs
│   │   ├── CSharpCodeGenerator.cs
│   │   ├── PythonCodeGenerator.cs
│   │   └── CppCodeGenerator.cs
│   ├── Parser/                    # 协议解析器
│   │   └── PDLParser.cs
│   ├── Python/                    # Python运行时库
│   │   └── bitrpc/
│   ├── Serialization/             # 序列化框架
│   │   └── BitMaskSerialization.cs
│   ├── Server/                    # 服务端框架
│   │   └── RpcServer.cs
│   └── GeneratorApp/              # 生成器应用程序
│       ├── Program.cs
│       └── GeneratorApp.csproj
└── *.bat                          # 批处理脚本
```

## 协议定义语言 (PDL)

### 基本语法

```pdl
namespace Example.Protocol

// 消息定义
message MessageName {
    field_type field_name = field_id;
    repeated item_type field_name = 2; // 列表类型
}

// 服务定义
service ServiceName {
    rpc MethodName(RequestType) returns (ResponseType);
}

// 选项定义
option version = "1.0.0"
```

### 支持的数据类型

- 基本类型: `int32`, `int64`, `float`, `double`, `bool`, `string`
- 复合类型: `repeated` (列表)
- 特殊类型: `DateTime`, `Vector3`

## 使用方法

### 1. 定义协议

创建 `.pdl` 文件定义你的协议:

```pdl
namespace MyService.Protocol

message Request {
    string message = 1;
    int32 value = 2;
}

message Response {
    bool success = 1;
    string result = 2;
}

service MyService {
    rpc Process(Request) returns (Response);
}
```

### 2. 生成代码

有几种方式可以生成代码:

#### 使用批处理脚本 (推荐)

```bash
# 生成所有语言的代码
generate-all.bat

# 只生成C#代码
generate-csharp.bat

# 只生成Python代码
generate-python.bat

# 只生成C++代码
generate-cpp.bat
```

#### 使用命令行

```bash
# 生成所有语言的代码
dotnet run --project Src/GeneratorApp -- generator-config.json

# 生成特定语言的代码 (需要先修改generator-config.json)
dotnet run --project Src/GeneratorApp -- generator-config.json
```

### 3. 生成的代码结构

```
Generated/
├── csharp/                    # C# 代码
│   ├── Data/                  # 数据结构
│   ├── Serialization/         # 序列化代码
│   ├── Client/               # 客户端代码
│   ├── Server/               # 服务端代码
│   └── Factory/              # 工厂类
├── python/                   # Python 代码
│   ├── data/                 # 数据模型
│   ├── serialization/        # 序列化器
│   ├── client/               # 客户端
│   ├── server/               # 服务端
│   └── factory/              # 工厂
└── cpp/                      # C++ 代码
    ├── include/              # 头文件
    ├── src/                  # 源文件
    └── CMakeLists.txt
```

## 配置文件

框架使用 `generator-config.json` 配置文件来控制代码生成:

```json
{
  "protocolFile": "Examples/user_service.pdl",
  "outputDirectory": "Generated",
  "languages": [
    {
      "name": "csharp",
      "enabled": true,
      "namespace": "Example.Protocol",
      "runtimePath": "Src"
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

## 位掩码序列化

本框架使用位掩码序列化技术来优化网络传输:

- 每32个字段分为一组
- 使用位掩码标识哪些字段需要序列化
- 只传输有变化的字段数据，减少传输量

### 序列化示例

```csharp
var user = new User
{
    UserId = 123,
    Username = "john_doe",
    IsActive = true
};

var serializer = BufferSerializer.Instance;
var data = serializer.Serialize(user);
```

## 客户端/服务端使用

### C# 客户端

```csharp
var client = new TcpRpcClient("localhost", 8080);
var userService = new UserServiceClient(client);

await client.ConnectAsync();
var response = await userService.GetUserAsync(new GetUserRequest { UserId = 123 });
```

### C# 服务端

```csharp
var server = new RpcServer(8080);
server.RegisterService("UserService", new UserServiceImplementation());
server.Start();
```

## 代码生成器选项

```csharp
var options = new GenerationOptions
{
    Language = TargetLanguage.CSharp,
    OutputDirectory = "./output",
    Namespace = "My.Protocol",
    GenerateSerialization = true,
    GenerateClientServer = true,
    GenerateFactories = true
};
```

## 构建和运行

### 构建生成器

```bash
dotnet build Src/GeneratorApp/GeneratorApp.csproj
```

### 运行生成器

```bash
# 使用配置文件
dotnet run --project Src/GeneratorApp -- generator-config.json

# 直接指定协议文件和输出目录
dotnet run --project Src/GeneratorApp -- Examples/user_service.pdl Generated
```

## 示例

查看 `Examples/user_service.pdl` 了解完整的协议定义示例。

## 生产环境集成

### 需要复制的代码文件

#### 核心运行时库
对于每种目标语言，需要将以下文件复制到生产项目中：

##### C#
1. `Src/Serialization/BitMaskSerialization.cs` - 序列化核心库
2. `Src/Client/RpcClient.cs` - 客户端框架
3. `Src/Server/RpcServer.cs` - 服务端框架

##### Python
1. `Src/Python/bitrpc/` 目录下的所有文件 - Python运行时库

##### C++
1. `Src/C++Core/` 目录下的所有文件 - C++运行时库

### 生成的协议代码
每次更新协议定义后，需要将生成的代码复制到对应项目中：

#### C#
- `Generated/csharp/Example/Protocol/Data/` - 数据结构类
- `Generated/csharp/Example/Protocol/Serialization/` - 序列化代码
- `Generated/csharp/Example/Protocol/Client/` - 客户端代码
- `Generated/csharp/Example/Protocol/Server/` - 服务端代码
- `Generated/csharp/Example/Protocol/Factory/` - 工厂类

#### Python
- `Generated/python/data/` - 数据模型
- `Generated/python/serialization/` - 序列化器
- `Generated/python/client/` - 客户端
- `Generated/python/server/` - 服务端
- `Generated/python/factory/` - 工厂

#### C++
- `Generated/cpp/include/` - 头文件
- `Generated/cpp/src/` - 源文件

## 许可证

本项目采用 MIT 许可证。