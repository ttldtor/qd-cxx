# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_schema_json_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
add_executable(qdcxx_schema_json_consumer main.cpp)
target_link_libraries(qdcxx_schema_json_consumer PRIVATE qdcxx_schema_json qdcxx_collectors)
set_target_properties(qdcxx_schema_json_consumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME installed_schema_json_history COMMAND qdcxx_schema_json_consumer)
set_tests_properties(installed_schema_json_history PROPERTIES TIMEOUT 15)
