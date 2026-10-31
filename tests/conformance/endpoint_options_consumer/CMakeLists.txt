# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_endpoint_options_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
add_executable(qdcxx_endpoint_options_consumer main.cpp)
target_link_libraries(qdcxx_endpoint_options_consumer PRIVATE qdcxx_client qdcxx_quote)
set_target_properties(qdcxx_endpoint_options_consumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME installed_endpoint_options COMMAND qdcxx_endpoint_options_consumer)
set_tests_properties(installed_endpoint_options PROPERTIES TIMEOUT 10)
