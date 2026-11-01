# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# CTest supplies PROGRAM and RUN. Only help/invalid arguments run; no listener or fixture is needed.
cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED PROGRAM OR NOT DEFINED RUN OR NOT IS_ABSOLUTE "${RUN}")
    message(FATAL_ERROR "Explicit peer executable and absolute test directory required")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../cli_help.cmake")
qdcxx_assert_cli_help("${PROGRAM}")
foreach(argument --unknown --control)
    execute_process(COMMAND "${PROGRAM}" "${argument}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
    if(NOT result EQUAL 2 OR NOT output STREQUAL "" OR error STREQUAL "")
        message(FATAL_ERROR "Invalid peer argument contract")
    endif()
endforeach()
# The nonexistent input path exercises a static diagnostic without creating or removing anything.
execute_process(COMMAND "${PROGRAM}" --control "${RUN}/qdcxx-tls-peer-missing-control/control"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 5)
if(NOT result EQUAL 1 OR NOT output STREQUAL "" OR NOT error STREQUAL "Internal TLS peer failed.\n")
    message(FATAL_ERROR "Invalid peer control contract")
endif()
message(STATUS "TLS peer help, usage and static failure checks passed")
