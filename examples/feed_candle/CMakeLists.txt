# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(qdcxx_feed_candle_example LANGUAGES CXX)
    include(CTest)
endif()
if(NOT TARGET qdcxx::feed)
    find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
endif()

add_executable(qdcxx_feed_candle main.cpp)
target_include_directories(qdcxx_feed_candle PRIVATE ../feed_common)
target_link_libraries(qdcxx_feed_candle PRIVATE qdcxx::feed)
set_target_properties(qdcxx_feed_candle PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-feed-candle)

if(BUILD_TESTING)
    add_test(NAME feed_candle_help COMMAND qdcxx_feed_candle --help)
endif()
