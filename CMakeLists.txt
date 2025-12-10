cmake_minimum_required(VERSION 3.10)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(OrderBook VERSION 1.0 LANGUAGES CXX)

include(FetchContent)

FetchContent_Declare(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.11.0
)
FetchContent_MakeAvailable(catch2)

# ------------
# Library
# ------------
add_library(orderbook_lib
    src/PriceLevel.cpp
    src/Order.cpp
    src/OrderBook.cpp   
)

target_include_directories(orderbook_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(orderbook_lib PRIVATE /W4 /WX)
else()
    target_compile_options(orderbook_lib PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# ------------
# Main exe
# ------------
add_executable(OrderBook src/main.cpp)
target_link_libraries(OrderBook PRIVATE orderbook_lib)

# ------------
# Tests
# ------------
add_executable(OrderBookTests tests/test_orderbook.cpp)
target_link_libraries(OrderBookTests 
    PRIVATE 
    Catch2::Catch2WithMain
    orderbook_lib
)

include(CTest)
include(Catch)
catch_discover_tests(OrderBookTests)