# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_feed_quote_example LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
if(NOT TARGET qdcxx_feed)
    message(FATAL_ERROR "The installed package requires BUILD_FEED_API=ON")
endif()
add_executable(qdcxx_feed_demo main.cpp)
target_link_libraries(qdcxx_feed_demo PRIVATE qdcxx_feed)
target_compile_options(qdcxx_feed_demo PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_feed_demo PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-feed-demo)
