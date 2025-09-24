# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BitRPC is a multi-language network communication protocol framework based on bitmask serialization technology. It allows defining protocol data structures and services in a Protocol Definition Language (PDL) file, then automatically generates code for C#, Python, and C++.

## Build and Development Commands

### Building the Generator
```bash
dotnet build Src/GeneratorApp/GeneratorApp.csproj
```

### Running the Generator
```bash
# Using batch scripts (recommended)
generate-all.bat              # Generate all languages
generate-csharp.bat          # Generate C# only
generate-python.bat          # Generate Python only
generate-cpp.bat             # Generate C++ only

# Using command line
dotnet run --project Src/GeneratorApp -- generator-config.json
```

### Running Tests
```bash
# Run tests if they exist (currently no test framework configured)
dotnet test
```

## Architecture Overview

### Core Components

1. **Protocol Definition Language (PDL)** - Custom DSL for defining messages and services
2. **PDL Parser** (`Src/Parser/PDLParser.cs`) - Parses .pdl files into ProtocolDefinition objects
3. **Code Generators** - Language-specific generators:
   - `Src/Generator/CSharpCodeGenerator.cs`
   - `Src/Generator/PythonCodeGenerator.cs`
   - `Src/Generator/CppCodeGenerator.cs`
4. **Serialization Framework** - Bitmask-based serialization in `Src/Serialization/BitMaskSerialization.cs`
5. **Runtime Libraries** - Client/server implementations for each language

### File Structure
```
Src/
├── Parser/           # Protocol definition parser
├── Generator/        # Code generators for all languages
├── Serialization/    # Core bitmask serialization
├── Client/          # C# client framework
├── Server/          # C# server framework
├── Python/          # Python runtime library
├── C++Core/         # C++ runtime library
└── GeneratorApp/    # Main generator application
```

### Generated Code Structure
```
Generated/
├── csharp/          # Generated C# code (Data/, Serialization/, Client/, Server/, Factory/)
├── python/          # Generated Python code (data/, serialization/, client/, server/, factory/)
└── cpp/             # Generated C++ code (include/, src/, CMakeLists.txt)
```

## Protocol Definition Language (PDL)

The framework uses a custom PDL syntax similar to Protocol Buffers:

```pdl
namespace Example.Protocol

message User {
    int64 user_id = 1;
    string username = 2;
    repeated string roles = 4;
    bool is_active = 5;
    DateTime created_at = 6;
}

service UserService {
    rpc Login(LoginRequest) returns (LoginResponse);
    rpc GetUser(GetUserRequest) returns (GetUserResponse);
}
```

### Supported Data Types
- **Primitives**: int32, int64, float, double, bool, string
- **Complex**: repeated (lists), custom message types
- **Special**: DateTime, Vector3

## Bitmask Serialization

The framework uses an efficient bitmask serialization approach:
- Fields are grouped in sets of 32
- Bitmask identifies which fields need serialization (non-default values)
- Only changed field data is transmitted, reducing payload size

## Configuration

The generator uses `generator-config.json` to control code generation:
- `protocolFile`: Path to the .pdl file
- `outputDirectory`: Base output directory
- `languages`: Array of language configurations with namespace and runtime paths

## Development Notes

- The generator application is .NET 6.0 based
- Each target language has its own code generator implementation
- The serialization framework is extensible with custom type handlers
- Generated code includes data structures, serialization, client/server stubs, and factory classes
- Protocol changes require regenerating all target language code