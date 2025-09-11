@echo off
echo Building BitRPC Test Applications
echo ================================

echo.
echo Building C# Server...
cd /d "%~dp0CSharpServer"
dotnet build -c Release
if %ERRORLEVEL% NEQ 0 (
    echo C# Server build failed!
    pause
    exit /b 1
)

echo.
echo Building C++ Client...
cd /d "%~dp0CppClient"
if not exist build mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
if %ERRORLEVEL% NEQ 0 (
    echo CMake generation failed!
    pause
    exit /b 1
)

cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo C++ Client build failed!
    pause
    exit /b 1
)

echo.
echo Both applications built successfully!
echo.
echo To run the test:
echo 1. Start the C# server: CSharpServer\bin\Release\net6.0\CSharpServer.exe
echo 2. In another terminal, run the C++ client: CppClient\build\bin\Release\CppClient.exe
echo.
pause