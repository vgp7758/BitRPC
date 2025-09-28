cmake_minimum_required(VERSION 3.14)
project(CppCore)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(../C++Core)

file(GLOB_RECURSE LIB_SRCS
     ../C++Core/*.cpp
)
