cmake_minimum_required(VERSION 3.10)
project(BitRPCTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    serialization.cpp
    client.cpp
    server.cpp
)

# Add header files
set(HEADERS
    serialization.h
    client.h
    server.h
)

# Create library
add_library(bitrpc STATIC ${SOURCES} ${HEADERS})

# Platform-specific settings
if(WIN32)
    target_link_libraries(bitrpc ws2_32)
    target_compile_definitions(bitrpc PRIVATE _WIN32)
endif()

# Include directories
target_include_directories(bitrpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Platform-specific compile definitions
if(WIN32)
    target_compile_definitions(bitrpc PRIVATE _WIN32)
endif()