# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_client_headers_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)

add_executable(qdcxx_client_headers_consumer headers_first.cpp)
if(WIN32)
    target_sources(qdcxx_client_headers_consumer PRIVATE windows_first.cpp)
endif()
target_link_libraries(qdcxx_client_headers_consumer PRIVATE qdcxx::feed)
set_target_properties(qdcxx_client_headers_consumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME client_installed_headers COMMAND qdcxx_client_headers_consumer)
set_tests_properties(client_installed_headers PROPERTIES TIMEOUT 15)
