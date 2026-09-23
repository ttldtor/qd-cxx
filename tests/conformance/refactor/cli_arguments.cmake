# Characterize public argument grammar independently of the private numeric helper.
file(MAKE_DIRECTORY "${RUN}")
include("${CMAKE_CURRENT_LIST_DIR}/../cli_help.cmake")
qdcxx_assert_cli_help("${PROGRAM}")
foreach(operand IN ITEMS "" "+" "+1" "-1" " 1" "1 " "1x" "18446744073709551616")
    execute_process(COMMAND "${PROGRAM}" --profile oracle-sre-v1 --input "${RUN}/absent.qtp" --max-input "${operand}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT error MATCHES "usage")
        message(FATAL_ERROR "invalid numeric operand reached IO: '${operand}'")
    endif()
endforeach()
foreach(operand IN ITEMS "0" "0001" "18446744073709551615")
    execute_process(COMMAND "${PROGRAM}" --profile oracle-sre-v1 --input "${RUN}/absent.qtp" --max-input "${operand}"
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT code EQUAL 3 OR NOT out STREQUAL "" OR NOT error MATCHES "open")
        message(FATAL_ERROR "valid numeric operand rejected before IO: '${operand}'")
    endif()
endforeach()
execute_process(COMMAND "${PROGRAM}" --profile oracle-sre-v1 --input "${RUN}/absent.qtp" --chunk-size 0
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT error MATCHES "usage")
    message(FATAL_ERROR "zero chunk must remain a usage error")
endif()
foreach(invalid IN ITEMS --input --help)
    execute_process(COMMAND "${PROGRAM}" "${invalid}" extra --profile oracle-sre-v1 --profile oracle-sre-v1
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT error MATCHES "usage")
        message(FATAL_ERROR "duplicate/incomplete arguments accepted")
    endif()
endforeach()
# A semicolon in one quoted process argument must remain part of the filename.
set(copy "${RUN}/input with spaces;and semicolon.qtp")
file(COPY_FILE "${FIXTURE}" "${copy}")
execute_process(COMMAND "${PROGRAM}" --profile oracle-sre-v1 --input "${FIXTURE}"
    RESULT_VARIABLE expected_code OUTPUT_VARIABLE expected_out ERROR_VARIABLE expected_error)
if(NOT expected_code EQUAL 0)
    message(FATAL_ERROR "characterization fixture did not succeed")
endif()
execute_process(COMMAND "${PROGRAM}" --profile oracle-sre-v1 --input "${copy}"
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
if(NOT code EQUAL expected_code OR NOT out STREQUAL expected_out OR NOT error STREQUAL expected_error)
    message(FATAL_ERROR "quoted path changes stdout/stderr/exit")
endif()
message(STATUS "CLI numeric grammar, help and quoted paths preserved")
