cmake_minimum_required(VERSION 3.10)
project(TestServer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    server_main.cpp
    ../cpp/src/models.cpp
    ../cpp/src/echorequest_serializer.cpp
    ../cpp/src/echoresponse_serializer.cpp
    ../cpp/src/getuserrequest_serializer.cpp
    ../cpp/src/getuserresponse_serializer.cpp
    ../cpp/src/loginrequest_serializer.cpp
    ../cpp/src/loginresponse_serializer.cpp
    ../cpp/src/userinfo_serializer.cpp
    ../cpp/src/serializer_registry.cpp
    ../cpp/src/testservice_service_base.cpp
    ../cpp/runtime/rpc_core.cpp
)

include_directories(../cpp/include)
include_directories(../cpp/runtime)

add_executable(test_server ${SOURCES})

if(WIN32)
    target_link_libraries(test_server ws2_32)
endif()