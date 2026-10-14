cmake_minimum_required(VERSION 3.10)
project(hrpc)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_SOURCE_DIR}/src/include)

add_executable(main main.cpp)