# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal CTest caller; reads PROGRAM and checks input-free help plus bounded argument failure.
if(NOT PROGRAM)
    message(FATAL_ERROR "PROGRAM is required")
endif()
execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE noargs_code OUTPUT_VARIABLE noargs ERROR_VARIABLE noargs_error TIMEOUT 10)
execute_process(COMMAND "${PROGRAM}" --help RESULT_VARIABLE help_code OUTPUT_VARIABLE help ERROR_VARIABLE help_error TIMEOUT 10)
execute_process(COMMAND "${PROGRAM}" --help-ru RESULT_VARIABLE ru_code OUTPUT_VARIABLE ru ERROR_VARIABLE ru_error TIMEOUT 10)
execute_process(COMMAND "${PROGRAM}" --invalid RESULT_VARIABLE invalid_code OUTPUT_VARIABLE invalid ERROR_VARIABLE invalid_error TIMEOUT 10)
if(NOT noargs_code EQUAL 0 OR NOT help_code EQUAL 0 OR NOT ru_code EQUAL 0
   OR NOT noargs STREQUAL help OR noargs_error OR help_error OR ru_error
   OR NOT help MATCHES "EventTextLimits" OR NOT ru MATCHES "EventTextLimits" OR NOT invalid_code EQUAL 2)
    message(FATAL_ERROR "Event text example help/argument contract failed")
endif()
