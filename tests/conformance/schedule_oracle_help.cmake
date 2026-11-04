# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Internal CTest caller: verifies the maintained oracle's input-free help without Java/reference work.
get_filename_component(repo "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(tool "${repo}/tools/upstream/schedule-oracle/Invoke-Oracle.ps1")
set(forbidden "${CMAKE_CURRENT_BINARY_DIR}/schedule-help-must-not-create")
if(EXISTS "${forbidden}")
    message(FATAL_ERROR "Help test marker already exists; no cleanup is attempted")
endif()
execute_process(COMMAND pwsh -NoProfile -File "${tool}" RESULT_VARIABLE noargs_code OUTPUT_VARIABLE noargs ERROR_VARIABLE noargs_error TIMEOUT 20)
execute_process(COMMAND pwsh -NoProfile -File "${tool}" -Help -Upstream missing -JavaHome missing -Logger missing -RunDirectory "${forbidden}"
    RESULT_VARIABLE help_code OUTPUT_VARIABLE help ERROR_VARIABLE help_error TIMEOUT 20)
execute_process(COMMAND pwsh -NoProfile -File "${tool}" -HelpRu
    RESULT_VARIABLE ru_code OUTPUT_VARIABLE ru ERROR_VARIABLE ru_error TIMEOUT 20)
execute_process(COMMAND pwsh -NoProfile -Command "Get-Help -Full '${tool}'"
    RESULT_VARIABLE gethelp_code OUTPUT_VARIABLE gethelp ERROR_VARIABLE gethelp_error TIMEOUT 20)
if(NOT noargs_code EQUAL 0 OR NOT help_code EQUAL 0 OR NOT ru_code EQUAL 0 OR NOT gethelp_code EQUAL 0
   OR noargs_error OR help_error OR ru_error OR gethelp_error OR EXISTS "${forbidden}"
   OR NOT noargs STREQUAL help OR NOT noargs MATCHES "RunDirectory" OR NOT ru MATCHES "RunDirectory"
   OR NOT gethelp MATCHES "128MiB")
    message(FATAL_ERROR "Schedule oracle no-input help contract failed")
endif()
