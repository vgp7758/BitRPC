cmake_minimum_required(VERSION 3.10)
project(example.protocolProtocol)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories("${CMAKE_CURRENT_LIST_DIR}/runtime")

set(SOURCES
    src/models.cpp
    src/userinfo_serializer.cpp
    src/loginrequest_serializer.cpp
    src/loginresponse_serializer.cpp
    src/getuserrequest_serializer.cpp
    src/getuserresponse_serializer.cpp
    src/echorequest_serializer.cpp
    src/echoresponse_serializer.cpp
    src/serializer_registry.cpp
    src/testservice_client.cpp
    src/testservice_service_base.cpp
)

add_library(${PROJECT_NAME} STATIC ${SOURCES})

target_link_libraries(${PROJECT_NAME}
    bitrpc
)
