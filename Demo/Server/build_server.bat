@echo off
echo Building C++ server with Visual Studio compiler...

REM 获取Visual Studio的包含路径
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set GEN=..\cpp

cl.exe /std:c++17 /O2 /I%GEN%\include /I%GEN%\runtime ^
    server_main.cpp ^
    %GEN%\src\models.cpp ^
    %GEN%\src\echorequest_serializer.cpp ^
    %GEN%\src\echoresponse_serializer.cpp ^
    %GEN%\src\getuserrequest_serializer.cpp ^
    %GEN%\src\getuserresponse_serializer.cpp ^
    %GEN%\src\loginrequest_serializer.cpp ^
    %GEN%\src\loginresponse_serializer.cpp ^
    %GEN%\src\userinfo_serializer.cpp ^
    %GEN%\src\serializer_registry.cpp ^
    %GEN%\src\testservice_service_base.cpp ^
    %GEN%\runtime\rpc_core.cpp ^
    /link /out:test_server.exe ws2_32.lib

echo Build completed: test_server.exe