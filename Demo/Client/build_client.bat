@echo off
echo Building C++ client...

set SRC_ROOT=..\..\..
set GEN=..\..\Generated\cpp
set CORE=..\..\Src\C++Core

g++ -std=c++17 -O2 -I%GEN%\include -I%CORE% ^
    client_main.cpp ^
    %GEN%\src\models.cpp ^
    %GEN%\src\echorequest_serializer.cpp ^
    %GEN%\src\echoresponse_serializer.cpp ^
    %GEN%\src\getuserrequest_serializer.cpp ^
    %GEN%\src\getuserresponse_serializer.cpp ^
    %GEN%\src\loginrequest_serializer.cpp ^
    %GEN%\src\loginresponse_serializer.cpp ^
    %GEN%\src\userinfo_serializer.cpp ^
    %GEN%\src\serializer_registry.cpp ^
    %GEN%\src\testservice_client.cpp ^
    %CORE%\rpc_core.cpp ^
    -o test_client.exe -lws2_32

echo Build completed: test_client.exe