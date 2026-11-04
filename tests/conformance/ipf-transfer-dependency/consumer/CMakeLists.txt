# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_ipf_transfer_sdk_consumer LANGUAGES CXX)
find_package(qdcxx_ipf_transfer_sdk CONFIG REQUIRED)
add_executable(qdcxx_ipf_transfer_consumer ../parser_probe.cpp)
target_link_libraries(qdcxx_ipf_transfer_consumer PRIVATE qdcxx_private::url)
enable_testing()
add_test(NAME qdcxx_ipf_transfer_installed_probe COMMAND qdcxx_ipf_transfer_consumer)
set_tests_properties(qdcxx_ipf_transfer_installed_probe PROPERTIES TIMEOUT 30)
