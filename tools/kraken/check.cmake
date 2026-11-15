# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

function(expect_exit expected marker)
    execute_process(COMMAND "${PROGRAM}" ${ARGN} RESULT_VARIABLE status
        OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 5)
    if(NOT "${status}" STREQUAL "${expected}" OR NOT "${output}${diagnostic}" MATCHES "${marker}")
        message(FATAL_ERROR "Unexpected gateway result for ${ARGN}: ${status}\n${output}${diagnostic}")
    endif()
    if(output MATCHES "LISTEN|PUBLISH")
        message(FATAL_ERROR "Gateway started runtime work for ${ARGN}: ${output}")
    endif()
endfunction()

expect_exit(0 "Usage:" --help)
expect_exit(0 "Использование:" --help-ru)
expect_exit(2 "Invalid arguments" --help --unknown)
expect_exit(2 "Invalid arguments")
expect_exit(2 "Invalid arguments" --trust-pem)
expect_exit(2 "Invalid arguments" --trust-pem unused.pem --duration-ms 0)
expect_exit(2 "Invalid arguments" --trust-pem unused.pem --source-port 65536)
expect_exit(2 "Invalid arguments" --trust-pem unused.pem --listen-port -1)
expect_exit(2 "Invalid arguments" --trust-pem unused.pem --trace-quotes --trace-quotes)
expect_exit(1 "Cannot read" --trust-pem "${CMAKE_CURRENT_BINARY_DIR}/qdcxx-nonexistent-ca.pem")

execute_process(COMMAND "${PROGRAM}" --trust-pem "${TRUST}" --source-host invalid_hostname --duration-ms 10
    RESULT_VARIABLE status OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic TIMEOUT 5)
if(NOT status STREQUAL "1" OR NOT diagnostic STREQUAL ""
   OR NOT output MATCHES "^LISTEN 127\\.0\\.0\\.1 [1-9][0-9]*"
   OR NOT output MATCHES "STATS observed=0 published=0 sent_frames=0 retired_peers=0 pending_publications=0")
    message(FATAL_ERROR "Gateway should listen without starting the source when nobody subscribes: ${status}\n${output}${diagnostic}")
endif()
