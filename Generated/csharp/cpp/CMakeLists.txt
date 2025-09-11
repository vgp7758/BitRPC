cmake_minimum_required(VERSION 3.10)
project(example.protocolProtocol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(SOURCES
    src/models.cpp
    src/complexmessage_serializer.cpp
    src/serializer_registry.cpp
    src/complexservice_client.cpp
    src/complexservice_service_base.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    bitrpc::core
)
