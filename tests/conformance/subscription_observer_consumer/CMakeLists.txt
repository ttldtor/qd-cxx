# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_subscription_observer_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
add_executable(qdcxx_subscription_observer_consumer main.cpp feed_header.cpp filter_header.cpp)
target_link_libraries(qdcxx_subscription_observer_consumer PRIVATE qdcxx::feed)
set_target_properties(qdcxx_subscription_observer_consumer PROPERTIES CXX_EXTENSIONS OFF)
enable_testing()
add_test(NAME subscription_observer_consumer COMMAND qdcxx_subscription_observer_consumer)
set_tests_properties(subscription_observer_consumer PROPERTIES TIMEOUT 15)
