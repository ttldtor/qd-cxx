# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_subscription_receiver_consumer LANGUAGES CXX)

find_package(qdcxx CONFIG REQUIRED)
find_package(Threads REQUIRED)
if(NOT TARGET qdcxx_quote OR NOT TARGET qdcxx_transport)
    message(FATAL_ERROR "The installed qdcxx package requires Quote and RUNTIME support")
endif()

add_executable(qdcxx_quote_example_peer
    ../subscription_receiver_example_peer.cpp
    ../recovery_test_peer.cpp
    ../transport/tls_dns_peer.cpp)
target_include_directories(qdcxx_quote_example_peer PRIVATE ..)
target_link_libraries(qdcxx_quote_example_peer PRIVATE qdcxx_quote qdcxx_transport Threads::Threads)
if(WIN32)
    target_link_libraries(qdcxx_quote_example_peer PRIVATE ws2_32)
endif()
set_target_properties(qdcxx_quote_example_peer PROPERTIES CXX_EXTENSIONS OFF)
