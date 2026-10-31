# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Independent production-package consumer: configure with explicit qdcxx_DIR after relocating installation.
cmake_minimum_required(VERSION 3.25)
project(qdcxx_subscription_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED)
get_filename_component(qdcxxSourceRoot "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(qdcxx_subscription_consumer main.cpp
    "${qdcxxSourceRoot}/tests/conformance/connect_peer.cpp"
    "${qdcxxSourceRoot}/examples/subscriptions/stream_client.cpp"
    "${qdcxxSourceRoot}/examples/subscriptions/example.cpp"
    "${qdcxxSourceRoot}/tools/subscribe/output.cpp")
target_include_directories(qdcxx_subscription_consumer PRIVATE
    "${qdcxxSourceRoot}/tests/conformance" "${qdcxxSourceRoot}/examples/subscriptions" "${qdcxxSourceRoot}/tools/subscribe")
target_link_libraries(qdcxx_subscription_consumer PRIVATE qdcxx_client qdcxx_transport qdcxx_quote qdcxx_profile)
set_target_properties(qdcxx_subscription_consumer PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    target_compile_definitions(qdcxx_subscription_consumer PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(qdcxx_subscription_consumer PRIVATE ws2_32)
endif()
enable_testing()
add_test(NAME installed_subscription_consumer COMMAND qdcxx_subscription_consumer)
set_tests_properties(installed_subscription_consumer PROPERTIES TIMEOUT 20)
