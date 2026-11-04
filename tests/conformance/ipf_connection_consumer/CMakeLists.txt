# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.28)
project(IpfConnectionConsumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
enable_testing()
add_executable(ipf_connection_consumer main.cpp header.cpp)
target_link_libraries(ipf_connection_consumer PRIVATE qdcxx_ipf_live)
target_compile_features(ipf_connection_consumer PRIVATE cxx_std_23)
set_target_properties(ipf_connection_consumer PROPERTIES CXX_EXTENSIONS OFF)
add_test(NAME connection_consumer COMMAND ipf_connection_consumer)
set_tests_properties(connection_consumer PROPERTIES TIMEOUT 15)
