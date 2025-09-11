# BitRPC Protocol Framework - Project Context

## Project Overview

This is the BitRPC Protocol Framework, a multi-language network communication protocol framework based on bitmask serialization technology. The framework allows defining protocol data structures and services in a Protocol Definition Language (PDL) file, which can then be used to automatically generate code for multiple target languages (C#, Python, C++).

### Key Features

- Multi-language support: C#, Python, C++
- Bitmask serialization for efficient network transmission
- Code generation from protocol definition files
- Client/server framework code generation
- Type-safe data structure definitions

### Project Structure

```
BitRPC/
├── Docs/                           # Documentation
│   └── protocol-framework-design.md
├── Examples/                       # Example protocol definitions
│   └── user_service.pdl
├── Src/                           # Source code
│   ├── Parser/                     # Protocol parser
│   │   └── PDLParser.cs
│   ├── Generator/                  # Code generators
│   │   ├── CodeGeneratorCore.cs
│   │   ├── CSharpCodeGenerator.cs
│   │   ├── PythonCodeGenerator.cs
│   │   └── CppCodeGenerator.cs
│   ├── Serialization/              # Serialization framework
│   │   └── BitMaskSerialization.cs
│   ├── Client/                     # Client framework
│   │   └── RpcClient.cs
│   ├── Server/                     # Server framework
│   │   └── RpcServer.cs
│   └── GeneratorApp/               # Generator application
│       ├── Program.cs
│       └── GeneratorApp.csproj
└── Tests/                          # Test files
```

## Core Components

1. **Protocol Definition Language (PDL)**: A DSL for defining data structures and service interfaces
2. **Code Generator**: Parses PDL files and generates target language code
3. **Serialization Framework**: Bitmask-based serialization/deserialization implementation
4. **Runtime Libraries**: Language-specific runtime support libraries

## Building and Running

### Building the Generator

```bash
dotnet build Src/GeneratorApp/GeneratorApp.csproj
```

### Running the Generator

```bash
dotnet run --project Src/GeneratorApp -- Examples/user_service.pdl Generated
```

This command will generate code for all supported languages (C#, Python, C++) based on the protocol definition in `Examples/user_service.pdl` and output it to the `Generated` directory.

## Development Conventions

- The project follows a modular architecture with clearly separated concerns (Parser, Generator, Serialization, Client, Server)
- Code generation is driven by protocol definition files (.pdl)
- Each target language has its own code generator implementation
- The serialization framework is designed to be extensible with custom type handlers
- The project uses .NET 6.0 for the generator application

## Example Protocol Definition

The framework uses a custom Protocol Definition Language (PDL) for defining messages and services:

```pdl
namespace Example.Protocol

// User data structure
message User {
    int64 user_id = 1;
    string username = 2;
    string email = 3;
    repeated string roles = 4;
    bool is_active = 5;
    DateTime created_at = 6;
}

// Service definition
service UserService {
    rpc Login(LoginRequest) returns (LoginResponse);
    rpc GetUser(GetUserRequest) returns (GetUserResponse);
}
```

## Bitmask Serialization

The framework uses bitmask serialization to optimize network transmission:
- Fields are grouped in sets of 32
- A bitmask identifies which fields need to be serialized (non-default values)
- Only changed field data is transmitted, reducing payload size