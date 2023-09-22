cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(xbrz STATIC xbrz.cpp)