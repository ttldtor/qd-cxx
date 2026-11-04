# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal caller: cmake -DJAVA=<JDK8 java> -DCLASSES=<compiled oracle classes> -P this file.
# The isolated classpath omits QD classes; help must succeed without inputs or output files.
if(NOT JAVA OR NOT CLASSES)
    message(FATAL_ERROR "JAVA and CLASSES are required")
endif()
execute_process(COMMAND "${JAVA}" -cp "${CLASSES}" qdcxx.oracle.ScheduleFilterOracle
    RESULT_VARIABLE noargs_code OUTPUT_VARIABLE noargs ERROR_VARIABLE noargs_error TIMEOUT 20)
execute_process(COMMAND "${JAVA}" -cp "${CLASSES}" qdcxx.oracle.ScheduleFilterOracle --help
    RESULT_VARIABLE help_code OUTPUT_VARIABLE help ERROR_VARIABLE help_error TIMEOUT 20)
execute_process(COMMAND "${JAVA}" -cp "${CLASSES}" qdcxx.oracle.ScheduleFilterOracle --help-ru
    RESULT_VARIABLE ru_code OUTPUT_VARIABLE ru ERROR_VARIABLE ru_error TIMEOUT 20)
if(NOT noargs_code EQUAL 0 OR NOT help_code EQUAL 0 OR NOT ru_code EQUAL 0 OR noargs_error OR help_error OR ru_error
   OR NOT noargs STREQUAL help OR NOT help MATCHES "4096" OR NOT ru MATCHES "наблюдения фильтров")
    message(FATAL_ERROR "Filter oracle input-free help contract failed")
endif()
message(STATUS "Filter oracle EN/RU and no-argument help passed without QD classes")
