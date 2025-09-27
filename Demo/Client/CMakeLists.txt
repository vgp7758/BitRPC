cmake_minimum_required(VERSION 3.10)
project(TestClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    client_main.cpp
    ../cpp/src/models.cpp
    ../cpp/src/echorequest_serializer.cpp
    ../cpp/src/echoresponse_serializer.cpp
    ../cpp/src/getuserrequest_serializer.cpp
    ../cpp/src/getuserresponse_serializer.cpp
    ../cpp/src/loginrequest_serializer.cpp
    ../cpp/src/loginresponse_serializer.cpp
    ../cpp/src/userinfo_serializer.cpp
    ../cpp/src/serializer_registry.cpp
    ../cpp/src/testservice_client.cpp
    ../cpp/runtime/server.cpp
    ../cpp/runtime/serialization.cpp
    ../cpp/runtime/client.cpp
)

include_directories(../cpp/include)
include_directories(../cpp/runtime)

add_executable(test_client ${SOURCES})

if(WIN32)
    target_link_libraries(test_client ws2_32)
endif()