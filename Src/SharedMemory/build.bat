@echo off
REM Windows构建脚本

echo BitRPC Shared Memory Library Build Script (Windows)
echo =======================================================

setlocal enabledelayedexpansion

REM 获取脚本所在目录
set SCRIPT_DIR=%~dp0
set PROJECT_ROOT=%SCRIPT_DIR%\..\..

REM 构建配置
set BUILD_DIR=%SCRIPT_DIR%\build
set LIB_NAME=SharedMemoryNative
set INSTALL_DIR=%SCRIPT_DIR%\dist

REM 清理之前的构建
echo Cleaning previous build...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
if exist "%INSTALL_DIR%" rmdir /s /q "%INSTALL_DIR%"

REM 创建构建目录
mkdir "%BUILD_DIR%"
mkdir "%INSTALL_DIR%\lib"
mkdir "%INSTALL_DIR%\include"
mkdir "%INSTALL_DIR%\examples"
mkdir "%INSTALL_DIR%\python"
mkdir "%INSTALL_DIR%\csharp"

REM 检查Visual Studio
where cl >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: Visual Studio compiler not found. Please run in Visual Studio Developer Command Prompt
    exit /b 1
)

REM 检查CMake
where cmake >nul 2>nul
if %errorlevel% neq 0 (
    echo Error: CMake is required but not installed.
    exit /b 1
)

echo Using Visual Studio compiler...

REM 创建CMakeLists.txt
echo Creating CMakeLists.txt...
(
echo cmake_minimum_required^(VERSION 3.10^)
echo project^(%LIB_NAME% CXX^)
echo.
echo set^(CMAKE_CXX_STANDARD 17^)
echo set^(CMAKE_CXX_STANDARD_REQUIRED ON^)
echo.
echo # 源文件
echo set^(SOURCES
echo     "%SCRIPT_DIR%\ring_buffer.cpp"
echo     "%SCRIPT_DIR%\shared_memory_manager.cpp"
echo     "%SCRIPT_DIR%\shared_memory_api.cpp"
echo ^)
echo.
echo # 头文件
echo set^(HEADERS
echo     "%SCRIPT_DIR%\ring_buffer.h"
echo     "%SCRIPT_DIR%\shared_memory_manager.h"
echo     "%SCRIPT_DIR%\shared_memory_api.h"
echo ^)
echo.
echo # 编译选项
echo add_compile_options^(/W4^)
echo.
echo # 创建共享库
echo add_library^(%{PROJECT_NAME} SHARED ${SOURCES} ${HEADERS}^)
echo.
echo # 设置输出名称
echo set_target_properties^(%{PROJECT_NAME} PROPERTIES
echo     OUTPUT_NAME %{PROJECT_NAME}
echo ^)
echo.
echo # 链接库
echo target_link_libraries^(%{PROJECT_NAME}^)
echo.
echo # 安装规则
echo install^(TARGETS %{PROJECT_NAME}
echo         RUNTIME DESTINATION bin
echo         LIBRARY DESTINATION lib
echo         ARCHIVE DESTINATION lib^)
echo.
echo install^(FILES ${HEADERS} DESTINATION include^)
) > "%BUILD_DIR%\CMakeLists.txt"

REM 构建项目
echo Building project...
cd /d "%BUILD_DIR%"

REM 配置
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%INSTALL_DIR%"

REM 编译
echo Compiling...
cmake --build . --config Release

REM 安装
echo Installing...
cmake --install . --config Release

REM 复制示例文件
echo Copying examples...
xcopy /Y /I "%SCRIPT_DIR%\examples\*.cpp" "%INSTALL_DIR%\examples\"
xcopy /Y /I "%SCRIPT_DIR%\examples\*.cs" "%INSTALL_DIR%\examples\"
xcopy /Y /I "%SCRIPT_DIR%\examples\*.py" "%INSTALL_DIR%\examples\"

REM 复制Python API
echo Copying Python API...
copy /Y "%SCRIPT_DIR%\shared_memory_api.py" "%INSTALL_DIR%\python\" >nul

REM 复制C# API
echo Copying C# API...
copy /Y "%SCRIPT_DIR%\SharedMemoryAPI.cs" "%INSTALL_DIR%\csharp\" >nul

REM 创建文档
echo Creating documentation...
(
echo # BitRPC Shared Memory Library
echo.
echo 高性能跨进程共享内存通信库，支持C++、C#和Python。
echo.
echo ## 特性
echo.
echo - **高性能SPSC环形缓冲区**：无锁设计，支持高并发
echo - **跨语言支持**：C++、C#、Python无缝互通
echo - **内存安全**：自动内存管理和边界检查
echo - **事件通知**：跨进程事件同步机制
echo - **类型安全**：强类型消息处理
echo - **统计信息**：详细的性能统计
echo.
echo ## 快速开始
echo.
echo ### C++示例
echo.
echo ```cpp
echo #include "shared_memory_api.h"
echo.
echo using namespace bitrpc::shared_memory;
echo.
echo // 创建生产者
echo auto producer = create_producer^("test_buffer"^);
echo if ^(producer^) {
echo     producer->send_string^("Hello from C++!"^);
echo     producer->dispose^(^);
echo }
echo ```
echo.
echo ### C#示例
echo.
echo ```csharp
echo using BitRPC.SharedMemory;
echo.
echo // 创建消费者
echo var consumer = SharedMemoryFactory.CreateConsumer^("test_buffer"^);
echo if ^(consumer != null^) {
echo     string message = consumer.ReceiveString^(^);
echo     consumer.Dispose^(^);
echo }
echo ```
echo.
echo ### Python示例
echo.
echo ```python
echo from shared_memory_api import create_consumer
echo.
echo # 创建消费者
echo consumer = create_consumer^("test_buffer"^)
echo if consumer:
echo     message = consumer.receive_string^(^)
echo     consumer.dispose^(^)
echo ```
echo.
echo ## 跨语言测试
echo.
echo 运行跨语言通信测试：
echo.
echo ```bash
echo # 终端1 - 启动C++生产者
echo examples\cross_language_test.exe --mode producer --name CrossLangTest
echo.
echo # 终端2 - 启动C#消费者
echo examples\cross_language_test.exe --mode consumer --name CrossLangTest
echo.
echo # 终端3 - 启动Python消费者
echo python examples\cross_language_test.py --mode consumer --name CrossLangTest
echo ```
echo.
echo ## API文档
echo.
echo 详细的API文档请参考头文件和示例代码。
echo.
echo ## 性能特性
echo.
echo - **吞吐量**：每秒数百万消息
echo - **延迟**：微秒级延迟
echo - **内存效率**：零拷贝设计
echo - **跨进程**：支持同一主机上的多进程通信
echo.
echo ## 构建要求
echo.
echo - Visual Studio 2019 or later
echo - CMake 3.10+
echo - Python 3.6+ ^(仅Python绑定^)
echo - .NET Core 3.1+ ^(仅C#绑定^)
) > "%INSTALL_DIR%\README.md"

echo Build completed successfully!
echo.
echo Build artifacts:
echo   Library: %INSTALL_DIR%\lib\
echo   Headers: %INSTALL_DIR%\include\
echo   Examples: %INSTALL_DIR%\examples\
echo   Python API: %INSTALL_DIR%\python\
echo   C# API: %INSTALL_DIR%\csharp\
echo.
echo To use the library, add the include and library paths to your project.

REM 创建简单的测试脚本
(
echo @echo off
echo echo Testing shared memory library...
echo.
echo REM 检查库文件是否存在
echo if not exist "lib\SharedMemoryNative.dll" (
echo     echo Error: Shared library not found
echo     exit /b 1
echo ^)
echo.
echo REM 检查头文件
echo if not exist "include\shared_memory_api.h" (
echo     echo Error: Header files not found
echo     exit /b 1
echo ^)
echo.
echo REM 检查示例文件
echo if not exist "examples\cross_language_test.cpp" (
echo     echo Error: Example files not found
echo     exit /b 1
echo ^)
echo.
echo echo ✓ All files present
echo echo ✓ Build verification complete
) > "%INSTALL_DIR%\test.bat"

cd /d "%SCRIPT_DIR%"
echo Build script completed successfully!