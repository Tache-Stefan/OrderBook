cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

file(GLOB_RECURSE SRC_FILES src/*.cpp)
add_executable(OrderBook ${SRC_FILES})
target_include_directories(OrderBook PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
