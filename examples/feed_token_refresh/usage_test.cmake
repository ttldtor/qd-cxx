# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE no_args OUTPUT_VARIABLE usage ERROR_VARIABLE no_args_error)
execute_process(COMMAND "${PROGRAM}" --help RESULT_VARIABLE help OUTPUT_VARIABLE help_text ERROR_VARIABLE help_error)
if(NOT no_args EQUAL 0 OR NOT help EQUAL 0 OR NOT usage MATCHES "Usage:" OR NOT usage STREQUAL help_text
    OR NOT no_args_error STREQUAL "" OR NOT help_error STREQUAL "")
    message(FATAL_ERROR "Expected identical help on stdout and an empty stderr for both invocations")
endif()
execute_process(COMMAND "${PROGRAM}" --help unexpected RESULT_VARIABLE invalid OUTPUT_VARIABLE invalid_out
    ERROR_VARIABLE invalid_error)
if(NOT invalid EQUAL 2 OR NOT invalid_out STREQUAL "" OR NOT invalid_error MATCHES "Invalid arguments")
    message(FATAL_ERROR "Expected usage rejection for additional arguments after --help")
endif()
