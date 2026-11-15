# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_kraken_acceptance LANGUAGES CXX)

find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
add_executable(qdcxx_kraken_quote_client quote_client.cpp)
target_link_libraries(qdcxx_kraken_quote_client PRIVATE qdcxx::feed)
set_target_properties(qdcxx_kraken_quote_client PROPERTIES CXX_EXTENSIONS OFF)

option(ORG_TTLDTOR_QDCXX_KRAKEN_TEST_PEER "Build the local WSS fixture peer" ON)
if(ORG_TTLDTOR_QDCXX_KRAKEN_TEST_PEER)
    find_package(qdcxx_beast_wss_sdk CONFIG REQUIRED)
    add_executable(qdcxx_kraken_fixture_peer fixture_peer.cpp)
    target_link_libraries(qdcxx_kraken_fixture_peer PRIVATE qdcxx_private::beast_wss)
    set_target_properties(qdcxx_kraken_fixture_peer PROPERTIES CXX_EXTENSIONS OFF)
endif()
