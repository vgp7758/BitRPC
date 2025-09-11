cmake_minimum_required(VERSION 3.10)
project(CppClient)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add source files
set(SOURCES
    main.cpp
    ../../Src/C++Core/rpc_core.cpp
    ../../Generated/cpp/src/models.cpp
    ../../Generated/cpp/src/user_serializer.cpp
    ../../Generated/cpp/src/loginrequest_serializer.cpp
    ../../Generated/cpp/src/loginresponse_serializer.cpp
    ../../Generated/cpp/src/getuserrequest_serializer.cpp
    ../../Generated/cpp/src/getuserresponse_serializer.cpp
    ../../Generated/cpp/src/userservice_client.cpp
    ../../Generated/cpp/src/serializer_registry.cpp
)

# Add include directories
include_directories(../../Generated/cpp/include)
include_directories(../../Src/C++Core)

# Create executable
add_executable(CppClient ${SOURCES})

# Platform-specific libraries
if(WIN32)
    target_link_libraries(CppClient ws2_32)
endif()

# Set output directory
set_target_properties(CppClient PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)