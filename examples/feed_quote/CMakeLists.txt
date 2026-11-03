# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(qdcxx_feed_quote_example LANGUAGES CXX)
endif()
if(NOT TARGET qdcxx_feed)
    find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
endif()
if(NOT TARGET qdcxx_feed)
    message(FATAL_ERROR "The installed package requires BUILD_FEED_API=ON")
endif()
if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR AND NOT DEFINED CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
find_package(Threads REQUIRED)
add_executable(qdcxx_feed_demo main.cpp signal_stop.cpp)
target_link_libraries(qdcxx_feed_demo PRIVATE qdcxx_feed Threads::Threads)
target_compile_options(qdcxx_feed_demo PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_feed_demo PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-feed-demo)

# Internal repository tests are intentionally absent from copied standalone/installed sample sources.
if((NOT DEFINED BUILD_TESTING OR BUILD_TESTING) AND PROJECT_IS_TOP_LEVEL AND PROJECT_NAME STREQUAL "qdcxx"
    AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/conformance/feed_signal_tests.cpp")
    add_executable(qdcxx_feed_signal_tests ../../tests/conformance/feed_signal_tests.cpp
        ../../tests/conformance/connect_peer.cpp signal_stop.cpp)
    target_include_directories(qdcxx_feed_signal_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(qdcxx_feed_signal_tests PRIVATE qdcxx_feed Threads::Threads)
    set_target_properties(qdcxx_feed_signal_tests PROPERTIES CXX_EXTENSIONS OFF)
    if(WIN32)
        target_compile_definitions(qdcxx_feed_signal_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_link_libraries(qdcxx_feed_signal_tests PRIVATE ws2_32)
    endif()
    enable_testing()
    add_test(NAME feed_signals COMMAND qdcxx_feed_signal_tests "$<TARGET_FILE:qdcxx_feed_demo>"
        "${CMAKE_CURRENT_BINARY_DIR}/signal-captures")
    set_tests_properties(feed_signals PROPERTIES TIMEOUT 15)
endif()
