# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_feed_tls_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)

option(ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS "Exercise the installed verified TLS profile" OFF)
add_executable(qdcxx_feed_tls_consumer main.cpp)
target_link_libraries(qdcxx_feed_tls_consumer PRIVATE qdcxx::feed)
set_target_properties(qdcxx_feed_tls_consumer PROPERTIES CXX_EXTENSIONS OFF)

if(ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS)
    target_compile_definitions(qdcxx_feed_tls_consumer PRIVATE ORG_TTLDTOR_QDCXX_TEST_INSTALLED_TLS)
endif()
