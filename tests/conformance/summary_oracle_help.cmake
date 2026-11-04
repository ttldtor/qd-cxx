# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# CTest caller supplies the explicit prefixed PowerShell/source variables below. This checks
# help/invalid-input exits without operational paths, Java, network or workload output creation.
foreach(launcher_name IN ITEMS Invoke-Oracle.ps1 Invoke-OpenInterest.ps1)
set(launcher "${ORG_TTLDTOR_QDCXX_SUMMARY_SOURCE}/tools/upstream/summary-oracle/${launcher_name}")
foreach(mode IN ITEMS noargs en ru invalid_paths incomplete)
    set(arguments)
    if(mode STREQUAL "en")
        set(arguments -Help)
    elseif(mode STREQUAL "ru")
        set(arguments -HelpRu)
    elseif(mode STREQUAL "invalid_paths")
        set(arguments -Help -Upstream X:/not-present -JavaHome X:/not-present -Logger X:/not-present -RunDirectory X:/not-present)
    elseif(mode STREQUAL "incomplete")
        set(arguments -Mode Verify)
    endif()
    execute_process(COMMAND "${ORG_TTLDTOR_QDCXX_SUMMARY_ORACLE_PWSH}" -NoProfile -File "${launcher}" ${arguments}
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 15)
    if(mode STREQUAL "incomplete")
        if(NOT result EQUAL 1 OR NOT error MATCHES "[Rr]equired")
            message(FATAL_ERROR "Summary incomplete invocation was not refused: ${result} ${output} ${error}")
        endif()
    elseif(NOT result EQUAL 0 OR NOT output MATCHES "[Hh]elp|[Сс]правк|SYNOPSIS")
        message(FATAL_ERROR "Summary help ${mode} failed: ${result} ${output} ${error}")
    endif()
    if(mode STREQUAL "noargs")
        set(noargs "${output}")
    elseif(mode STREQUAL "en" AND NOT output STREQUAL noargs)
        message(FATAL_ERROR "Summary noargs and explicit help differ")
    endif()
endforeach()

endforeach()
