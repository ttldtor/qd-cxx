# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal check: CTest supplies existing PowerShell, script and an owned binary-directory root.
# Help needs no JDK/classpath/output inputs; invalid arguments must not start capture work.
if(NOT PWSH OR NOT SCRIPT OR NOT RUN)
    message(FATAL_ERROR "PWSH, SCRIPT and RUN are required")
endif()
execute_process(COMMAND "${PWSH}" -NoProfile -File "${SCRIPT}"
    RESULT_VARIABLE noArgsResult OUTPUT_VARIABLE noArgsOutput ERROR_VARIABLE noArgsError TIMEOUT 20)
execute_process(COMMAND "${PWSH}" -NoProfile -File "${SCRIPT}" -Help
    -JavaHome "${RUN}/missing-jdk" -OutputDirectory "${RUN}/must-not-exist"
    RESULT_VARIABLE helpResult OUTPUT_VARIABLE helpOutput ERROR_VARIABLE helpError TIMEOUT 20)
if(NOT noArgsResult STREQUAL "0" OR NOT helpResult STREQUAL "0" OR
   NOT noArgsError STREQUAL "" OR NOT helpError STREQUAL "" OR
   NOT noArgsOutput STREQUAL helpOutput OR noArgsOutput STREQUAL "")
    message(FATAL_ERROR "Container oracle help/no-argument contract failed: ${noArgsResult}/${helpResult}; ${noArgsError}${helpError}")
endif()
execute_process(COMMAND "${PWSH}" -NoProfile -File "${SCRIPT}" -Mode Capture
    -OutputDirectory "${RUN}/must-not-exist"
    RESULT_VARIABLE invalidResult OUTPUT_VARIABLE invalidOutput ERROR_VARIABLE invalidError TIMEOUT 20)
if(invalidResult STREQUAL "0" OR EXISTS "${RUN}/must-not-exist")
    message(FATAL_ERROR "Invalid/help arguments started container capture work")
endif()
