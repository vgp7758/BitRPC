#!/bin/bash

# 跨语言共享内存库构建脚本

set -e

echo "BitRPC Shared Memory Library Build Script"
echo "==========================================="

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# 构建配置
BUILD_DIR="$SCRIPT_DIR/build"
LIB_NAME="SharedMemoryNative"
INSTALL_DIR="$SCRIPT_DIR/dist"

# 清理之前的构建
echo "Cleaning previous build..."
rm -rf "$BUILD_DIR" "$INSTALL_DIR"

# 创建构建目录
mkdir -p "$BUILD_DIR" "$INSTALL_DIR/lib" "$INSTALL_DIR/include"

# 检查CMake是否可用
if ! command -v cmake &> /dev/null; then
    echo "Error: CMake is required but not installed."
    exit 1
fi

# 检查编译器
if command -v gcc &> /dev/null; then
    COMPILER="gcc"
elif command -v clang &> /dev/null; then
    COMPILER="clang"
else
    echo "Error: No C/C++ compiler found."
    exit 1
fi

echo "Using compiler: $COMPILER"

# 创建CMakeLists.txt
cat > "$BUILD_DIR/CMakeLists.txt" << EOF
cmake_minimum_required(VERSION 3.10)
project($LIB_NAME CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    "$SCRIPT_DIR/ring_buffer.cpp"
    "$SCRIPT_DIR/shared_memory_manager.cpp"
    "$SCRIPT_DIR/shared_memory_api.cpp"
)

# 头文件
set(HEADERS
    "$SCRIPT_DIR/ring_buffer.h"
    "$SCRIPT_DIR/shared_memory_manager.h"
    "$SCRIPT_DIR/shared_memory_api.h"
)

# 编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# 创建共享库
add_library(\${PROJECT_NAME} SHARED \${SOURCES} \${HEADERS})

# 设置输出名称
set_target_properties(\${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME \${PROJECT_NAME}
    POSITION_INDEPENDENT_CODE ON
)

# 链接库
if(UNIX)
    target_link_libraries(\${PROJECT_NAME} pthread rt)
endif()

# 安装规则
install(TARGETS \${PROJECT_NAME}
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin)

install(FILES \${HEADERS} DESTINATION include)

# 导出配置
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "\${CMAKE_CURRENT_BINARY_DIR}/\${PROJECT_NAME}ConfigVersion.cmake"
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

export(EXPORT \${PROJECT_NAME}Targets
       FILE "\${CMAKE_CURRENT_BINARY_DIR}/\${PROJECT_NAME}Targets.cmake"
       NAMESPACE SharedMemory::)

configure_file("\${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in"
               "\${CMAKE_CURRENT_BINARY_DIR}/\${PROJECT_NAME}Config.cmake"
               @ONLY)

install(EXPORT \${PROJECT_NAME}Targets
        FILE \${PROJECT_NAME}Targets.cmake
        NAMESPACE SharedMemory::
        DESTINATION lib/cmake/\${PROJECT_NAME})

install(FILES "\${CMAKE_CURRENT_BINARY_DIR}/\${PROJECT_NAME}Config.cmake"
              "\${CMAKE_CURRENT_BINARY_DIR}/\${PROJECT_NAME}ConfigVersion.cmake"
              DESTINATION lib/cmake/\${PROJECT_NAME})
EOF

# 创建配置文件模板
cat > "$BUILD_DIR/Config.cmake.in" << EOF
@PACKAGE_INIT@

include("\${CMAKE_CURRENT_LIST_DIR}/\${PROJECT_NAME}Targets.cmake")

check_required_components(\${PROJECT_NAME})
EOF

# 构建项目
echo "Building project..."
cd "$BUILD_DIR"

# 配置
if command -v ninja &> /dev/null; then
    GENERATOR="Ninja"
    BUILD_CMD="ninja"
else
    GENERATOR="Unix Makefiles"
    BUILD_CMD="make"
fi

cmake .. -G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

# 编译
echo "Compiling..."
$BUILD_CMD -j$(nproc)

# 安装
echo "Installing..."
$BUILD_CMD install

# 复制示例文件
echo "Copying examples..."
mkdir -p "$INSTALL_DIR/examples"
cp "$SCRIPT_DIR/examples/"* "$INSTALL_DIR/examples/"

# 复制Python API
echo "Copying Python API..."
mkdir -p "$INSTALL_DIR/python"
cp "$SCRIPT_DIR/shared_memory_api.py" "$INSTALL_DIR/python/"

# 复制C# API
echo "Copying C# API..."
mkdir -p "$INSTALL_DIR/csharp"
cp "$SCRIPT_DIR/SharedMemoryAPI.cs" "$INSTALL_DIR/csharp/"

# 创建文档
echo "Creating documentation..."
cat > "$INSTALL_DIR/README.md" << 'EOF'
# BitRPC Shared Memory Library

高性能跨进程共享内存通信库，支持C++、C#和Python。

## 特性

- **高性能SPSC环形缓冲区**：无锁设计，支持高并发
- **跨语言支持**：C++、C#、Python无缝互通
- **内存安全**：自动内存管理和边界检查
- **事件通知**：跨进程事件同步机制
- **类型安全**：强类型消息处理
- **统计信息**：详细的性能统计

## 快速开始

### C++示例

```cpp
#include "shared_memory_api.h"

using namespace bitrpc::shared_memory;

// 创建生产者
auto producer = create_producer("test_buffer");
if (producer) {
    producer->send_string("Hello from C++!");
    producer->dispose();
}
```

### C#示例

```csharp
using BitRPC.SharedMemory;

// 创建消费者
var consumer = SharedMemoryFactory.CreateConsumer("test_buffer");
if (consumer != null) {
    string message = consumer.ReceiveString();
    consumer.Dispose();
}
```

### Python示例

```python
from shared_memory_api import create_consumer

# 创建消费者
consumer = create_consumer("test_buffer")
if consumer:
    message = consumer.receive_string()
    consumer.dispose()
```

## 跨语言测试

运行跨语言通信测试：

```bash
# 终端1 - 启动C++生产者
./examples/cross_language_test --mode producer --name CrossLangTest

# 终端2 - 启动C#消费者
./examples/cross_language_test --mode consumer --name CrossLangTest

# 终端3 - 启动Python消费者
python3 examples/cross_language_test.py --mode consumer --name CrossLangTest
```

## API文档

详细的API文档请参考头文件和示例代码。

## 性能特性

- **吞吐量**：每秒数百万消息
- **延迟**：微秒级延迟
- **内存效率**：零拷贝设计
- **跨进程**：支持同一主机上的多进程通信

## 构建要求

- C++17兼容编译器
- CMake 3.10+
- Python 3.6+ (仅Python绑定)
- .NET Core 3.1+ (仅C#绑定)
EOF

echo "Build completed successfully!"
echo ""
echo "Build artifacts:"
echo "  Library: $INSTALL_DIR/lib/"
echo "  Headers: $INSTALL_DIR/include/"
echo "  Examples: $INSTALL_DIR/examples/"
echo "  Python API: $INSTALL_DIR/python/"
echo "  C# API: $INSTALL_DIR/csharp/"
echo ""
echo "To use the library, add the include and library paths to your project."

# 创建简单的测试脚本
cat > "$INSTALL_DIR/test.sh" << 'EOF'
#!/bin/bash

# 简单的功能测试脚本

echo "Testing shared memory library..."

# 检查库文件是否存在
if [ ! -f "lib/libSharedMemoryNative.so" ] && [ ! -f "lib/libSharedMemoryNative.dylib" ]; then
    echo "Error: Shared library not found"
    exit 1
fi

# 检查头文件
if [ ! -f "include/shared_memory_api.h" ]; then
    echo "Error: Header files not found"
    exit 1
fi

# 检查示例文件
if [ ! -f "examples/cross_language_test.cpp" ]; then
    echo "Error: Example files not found"
    exit 1
fi

echo "✓ All files present"
echo "✓ Build verification complete"