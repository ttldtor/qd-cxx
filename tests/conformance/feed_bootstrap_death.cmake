# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal CTest helper: PROGRAM is the built bootstrap runner. Inputs are two finite death modes.
# Requires exact sentinel73 before timeout; creates no files/processes beyond the owned child runs.
cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED PROGRAM)
    message(FATAL_ERROR "Provide PROGRAM")
endif()
foreach(mode --bootstrap-owner-death --bootstrap-thread-death)
    execute_process(COMMAND "${PROGRAM}" "${mode}" RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 10)
    if(NOT code STREQUAL "73")
        message(FATAL_ERROR "${mode}: expected fail-fast73, got ${code}\n${out}\n${err}")
    endif()
endforeach()
