cmake_minimum_required(VERSION 3.10)
project(TestClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    client_main.cpp
    ../../Generated/cpp/src/models.cpp
    ../../Generated/cpp/src/echorequest_serializer.cpp
    ../../Generated/cpp/src/echoresponse_serializer.cpp
    ../../Generated/cpp/src/getuserrequest_serializer.cpp
    ../../Generated/cpp/src/getuserresponse_serializer.cpp
    ../../Generated/cpp/src/loginrequest_serializer.cpp
    ../../Generated/cpp/src/loginresponse_serializer.cpp
    ../../Generated/cpp/src/userinfo_serializer.cpp
    ../../Generated/cpp/src/serializer_registry.cpp
    ../../Generated/cpp/src/testservice_client.cpp
    ../../Src/C++Core/rpc_core.cpp
)

include_directories(../../Generated/cpp/include)
include_directories(../../Src/C++Core)

add_executable(test_client ${SOURCES})

if(WIN32)
    target_link_libraries(test_client ws2_32)
endif()