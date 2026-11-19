# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
if(NOT DEFINED PWSH OR NOT DEFINED SCRIPT OR NOT DEFINED RUN)
    message(FATAL_ERROR "Supply PWSH, SCRIPT and RUN")
endif()
foreach(mode IN ITEMS no_arguments -Help -HelpRu)
    if(mode STREQUAL "no_arguments")
        set(arguments)
    else()
        set(arguments "${mode}")
    endif()
    execute_process(COMMAND "${PWSH}" -NoLogo -NoProfile -File "${SCRIPT}" ${arguments}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 20)
    if(NOT "${result}" STREQUAL "0" OR NOT output MATCHES "Test-ProcessMetrics.ps1 -OutputDirectory")
        message(FATAL_ERROR "Resource metric help failed (${mode}): ${result}\n${output}\n${errors}")
    endif()
endforeach()
execute_process(COMMAND "${PWSH}" -NoLogo -NoProfile -File "${SCRIPT}" -UnsupportedOption
    RESULT_VARIABLE invalid_result OUTPUT_VARIABLE invalid_output ERROR_VARIABLE invalid_errors TIMEOUT 20)
if("${invalid_result}" STREQUAL "0")
    message(FATAL_ERROR "Invalid resource test option was accepted")
endif()
string(TIMESTAMP stamp "%Y%m%dT%H%M%SZ" UTC)
string(RANDOM LENGTH 8 ALPHABET 0123456789abcdef suffix)
execute_process(COMMAND "${PWSH}" -NoLogo -NoProfile -File "${SCRIPT}"
    -OutputDirectory "${RUN}/process-metrics-${stamp}-${suffix}"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors TIMEOUT 50)
message(STATUS "${output}")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Resource metric test failed: ${result}\n${output}\n${errors}")
endif()
