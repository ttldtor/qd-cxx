# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
if(NOT DEFINED PROGRAM OR NOT DEFINED ROOT)
    message(FATAL_ERROR "PROGRAM and ROOT are required")
endif()

execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE no_args OUTPUT_VARIABLE help ERROR_VARIABLE error)
if(NOT no_args EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "no-argument help failed: ${no_args}: ${error}")
endif()
execute_process(COMMAND "${PROGRAM}" --help RESULT_VARIABLE helped OUTPUT_VARIABLE explicit_help ERROR_VARIABLE error)
if(NOT helped EQUAL 0 OR NOT error STREQUAL "" OR NOT help STREQUAL explicit_help)
    message(FATAL_ERROR "standalone help differs or failed: ${helped}: ${error}")
endif()
if(NOT help MATCHES "--wire-schema ordinary-v1")
    message(FATAL_ERROR "wire discovery is absent from help")
endif()

# Invalid source combinations must be rejected before the nonexistent input is opened.
function(expect_usage)
    execute_process(COMMAND "${PROGRAM}" --input "${ROOT}/does-not-exist.qtp" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT code EQUAL 2 OR NOT output STREQUAL "" OR NOT error MATCHES "usage")
        message(FATAL_ERROR "source usage contract failed for ${ARGN}: ${code}: ${output}: ${error}")
    endif()
endfunction()
expect_usage(--wire-schema)
expect_usage(--wire-schema unknown)
expect_usage(--wire-schema ordinary-v1 --profile oracle-sre-v1)
expect_usage(--wire-schema ordinary-v1 --schema missing.json)
expect_usage(--wire-schema ordinary-v1 --wire-schema ordinary-v1)
expect_usage(--wire-schema ordinary-v1 --chunk-size 0)

foreach(mode IN ITEMS legacy raw-message-v1)
    if(mode STREQUAL "legacy")
        set(input "${ROOT}/tests/fixtures/encoding/field-types/expected/mixed-stream.qtp")
    else()
        set(input "${ROOT}/tests/fixtures/encoding/field-types/expected/mixed-raw.qtp")
    endif()
    foreach(chunk IN ITEMS 1 4096)
        execute_process(COMMAND "${PROGRAM}" --input "${input}" --wire-schema ordinary-v1
            --tape-mode "${mode}" --chunk-size "${chunk}"
            RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
        if(NOT code EQUAL 0 OR NOT error STREQUAL "" OR NOT output MATCHES "format.qdcxx-tape-inspect.7.ordinary-v1.${mode}"
           OR NOT output MATCHES "schema-end.1" OR NOT output MATCHES "wire-data" OR NOT output MATCHES "complete")
            message(FATAL_ERROR "production wire CLI failed ${mode}/${chunk}: ${code}: ${error}")
        endif()
        if(chunk EQUAL 1)
            set(one_byte "${output}")
        elseif(NOT output STREQUAL one_byte)
            message(FATAL_ERROR "chunking changes source metadata or values for ${mode}")
        endif()
    endforeach()
    execute_process(COMMAND "${PROGRAM}" --input "${input}" --wire-schema ordinary-v1 --tape-mode "${mode}"
        --max-schema-fields 0 RESULT_VARIABLE code OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(NOT code EQUAL 6 OR output MATCHES "schema-end" OR output MATCHES "complete")
        message(FATAL_ERROR "final schema bound did not reject without metadata: ${code}: ${error}")
    endif()
endforeach()
