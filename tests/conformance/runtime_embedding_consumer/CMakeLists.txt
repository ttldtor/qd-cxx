# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal acceptance fixture: configure with an installed qdcxx prefix and explicit qualified SDK root.
# CTest runs the finite public-header consumer; failure is a nonzero process result or a20s timeout.
cmake_minimum_required(VERSION 3.25)
project(qdcxx_runtime_embedding_consumer LANGUAGES CXX)
find_package(qdcxx CONFIG REQUIRED COMPONENTS feed)
find_package(Threads REQUIRED)
add_executable(qdcxx_runtime_embedding_consumer main.cpp)
target_link_libraries(qdcxx_runtime_embedding_consumer PRIVATE qdcxx::feed Threads::Threads)
set_target_properties(qdcxx_runtime_embedding_consumer PROPERTIES CXX_EXTENSIONS OFF)
include(CTest)
add_test(NAME runtime_embedding_consumer COMMAND qdcxx_runtime_embedding_consumer)
set_tests_properties(runtime_embedding_consumer PROPERTIES TIMEOUT 20)