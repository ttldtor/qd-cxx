# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_quote_subscription_example LANGUAGES CXX)

find_package(qdcxx CONFIG REQUIRED)
if(NOT TARGET qdcxx_recovery OR NOT TARGET qdcxx_client OR NOT TARGET qdcxx_quote)
    message(FATAL_ERROR "The installed qdcxx package requires CLIENT, RUNTIME and RECOVERY enabled")
endif()

add_executable(qdcxx_quote_subscription_example quote.cpp)
target_link_libraries(qdcxx_quote_subscription_example PRIVATE qdcxx_recovery qdcxx_client qdcxx_quote)
target_compile_options(qdcxx_quote_subscription_example PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_quote_subscription_example PROPERTIES
    CXX_EXTENSIONS OFF
    OUTPUT_NAME qdcxx-quote-subscription)
install(TARGETS qdcxx_quote_subscription_example RUNTIME DESTINATION bin)
