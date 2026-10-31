cmake_minimum_required(VERSION 3.25)
# Internal CTest helper: PROBE is the built executable; no fixture/network work is requested.
execute_process(COMMAND "${PROBE}" RESULT_VARIABLE noArgsResult OUTPUT_VARIABLE noArgs
    ERROR_VARIABLE noArgsError TIMEOUT 5)
execute_process(COMMAND "${PROBE}" --help RESULT_VARIABLE helpResult OUTPUT_VARIABLE help
    ERROR_VARIABLE helpError TIMEOUT 5)
if(NOT noArgsResult STREQUAL "0" OR NOT helpResult STREQUAL "0" OR NOT noArgs STREQUAL help
    OR NOT noArgsError STREQUAL "" OR NOT helpError STREQUAL "" OR help STREQUAL "")
    message(FATAL_ERROR "No-argument/--help contract failed")
endif()
execute_process(COMMAND "${PROBE}" --unknown RESULT_VARIABLE invalidResult TIMEOUT 5)
if(NOT invalidResult STREQUAL "2")
    message(FATAL_ERROR "Unknown arguments must return 2")
endif()
execute_process(COMMAND "${PROBE}" --run --fixtures "${CMAKE_CURRENT_BINARY_DIR}/missing-fixtures"
    RESULT_VARIABLE missingResult TIMEOUT 5)
if(NOT missingResult STREQUAL "1")
    message(FATAL_ERROR "Missing required fixture directory must return 1")
endif()
if(UNIX AND EXISTS "/dev/full")
    execute_process(COMMAND "${PROBE}" --help OUTPUT_FILE /dev/full RESULT_VARIABLE outputResult TIMEOUT 5)
    if(NOT outputResult STREQUAL "1")
        message(FATAL_ERROR "A failed help output write must return 1")
    endif()
endif()
