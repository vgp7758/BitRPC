cmake_minimum_required(VERSION 3.10)
project(BitRPCTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    rpc_core.cpp
)

# Add header files
set(HEADERS
    rpc_core.h
)

# Create library
add_library(bitrpc STATIC ${SOURCES} ${HEADERS})

# Platform-specific settings
if(WIN32)
    target_link_libraries(bitrpc ws2_32)
endif()

# Create test executable
add_executable(bitrpc_test test_main.cpp)
target_link_libraries(bitrpc_test bitrpc)

# Include directories
target_include_directories(bitrpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(bitrpc_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Platform-specific compile definitions
if(WIN32)
    target_compile_definitions(bitrpc PRIVATE _WIN32)
    target_compile_definitions(bitrpc_test PRIVATE _WIN32)
endif()