# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(qdcxx_feed_market_depth_example LANGUAGES CXX)
    include(CTest)
endif()
if(NOT TARGET qdcxx_feed)
    find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
endif()
add_executable(qdcxx_feed_market_depth main.cpp)
target_link_libraries(qdcxx_feed_market_depth PRIVATE qdcxx_feed)
set_target_properties(qdcxx_feed_market_depth PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-feed-market-depth)
if(BUILD_TESTING)
    add_test(NAME feed_market_depth_help COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_feed_market_depth>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/help_test.cmake")
endif()
