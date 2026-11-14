# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_beast_wss_consumer LANGUAGES CXX)
if(NOT IS_ABSOLUTE "${SDK_ROOT}" OR NOT IS_DIRECTORY "${SDK_ROOT}")
    message(FATAL_ERROR "Supply the absolute installed SDK_ROOT")
endif()
find_package(qdcxx_beast_wss_sdk CONFIG REQUIRED NO_DEFAULT_PATH
    PATHS "${SDK_ROOT}/lib/cmake/qdcxx_beast_wss_sdk")
add_executable(qdcxx_beast_wss_consumer "../probe.cpp")
set_target_properties(qdcxx_beast_wss_consumer PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(qdcxx_beast_wss_consumer PRIVATE qdcxx_private::beast_wss)
