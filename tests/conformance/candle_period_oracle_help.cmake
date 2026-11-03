# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# CTest caller passes POWERSHELL and SOURCE. Inputs: launcher source only; output: CMake failure
# for help/invalid-input regressions. No working paths, Java processes or SDK work are requested.
foreach(mode IN ITEMS noargs en ru invalid)
    set(arguments)
    if(mode STREQUAL "en")
        set(arguments -Help)
    elseif(mode STREQUAL "ru")
        set(arguments -HelpRu)
    elseif(mode STREQUAL "invalid")
        set(arguments -Mode Invalid)
    endif()
    execute_process(COMMAND "${POWERSHELL}" -NoProfile -File "${SOURCE}/tools/upstream/candle-period-oracle/Invoke-Oracle.ps1" ${arguments}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 10)
    if(mode STREQUAL "invalid")
        if(NOT result EQUAL 1 OR NOT error MATCHES "ValidateSet|Capture,Verify|Invalid")
            message(FATAL_ERROR "Invalid input did not fail explicitly: ${result} ${output} ${error}")
        endif()
    elseif(NOT result EQUAL 0 OR NOT output MATCHES "[Hh]elp|[Сс]правк|SYNOPSIS")
        message(FATAL_ERROR "Help ${mode} failed: ${result} ${output} ${error}")
    endif()
endforeach()
