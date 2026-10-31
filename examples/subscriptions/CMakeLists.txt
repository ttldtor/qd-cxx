# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_subscription_example LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
if(NOT TARGET qdcxx_client OR NOT TARGET qdcxx_transport)
    message(FATAL_ERROR "The installed package must have explicit CLIENT and RUNTIME enabled")
endif()
add_library(qdcxx_subscription_example STATIC example.cpp stream_client.cpp)
target_link_libraries(qdcxx_subscription_example PUBLIC qdcxx_client qdcxx_transport)
target_include_directories(qdcxx_subscription_example PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(qdcxx_subscription_example PROPERTIES CXX_EXTENSIONS OFF)
