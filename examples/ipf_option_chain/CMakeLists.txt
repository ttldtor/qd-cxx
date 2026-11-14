# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(qdcxx_ipf_option_chain_example LANGUAGES CXX)
    include(CTest)
endif()
if(NOT TARGET qdcxx_ipf)
    find_package(qdcxx CONFIG REQUIRED)
endif()
if(NOT TARGET qdcxx_ipf)
    message(FATAL_ERROR "The installed package requires ORG_TTLDTOR_QDCXX_BUILD_IPF=ON")
endif()

add_executable(qdcxx_ipf_option_chain main.cpp)
target_link_libraries(qdcxx_ipf_option_chain PRIVATE qdcxx_ipf)
target_compile_options(qdcxx_ipf_option_chain PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_ipf_option_chain PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-ipf-option-chain)

if(BUILD_TESTING)
    add_test(NAME ipf_option_chain_example COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_ipf_option_chain>"
        "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/example.ipf" -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
endif()
