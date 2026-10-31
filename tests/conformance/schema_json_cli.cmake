# External input must be validated before tape IO and preserve generic typed rows.
file(MAKE_DIRECTORY "${RUN}")
include("${CMAKE_CURRENT_LIST_DIR}/cli_help.cmake")
qdcxx_assert_cli_help("${PROGRAM}")
set(examples "${ROOT}/spec/schema/examples/native")
set(fixtures "${ROOT}/tests/fixtures/encoding/field-types/expected")

function(expect_failure expected token)
    execute_process(COMMAND "${PROGRAM}" ${ARGN}
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT code EQUAL expected OR NOT out STREQUAL "" OR NOT error MATCHES "${token}")
        message(FATAL_ERROR "Expected ${expected}/${token}, got ${code}: stdout=${out}; stderr=${error}")
    endif()
endfunction()

# Invalid binary-selection combinations precede file IO.
foreach(args IN ITEMS
    "--schema;missing;--validate-schema"
    "--schema;missing;--schema-format;binary-v1"
    "--schema;missing;--schema-format;wrong;--validate-schema"
    "--schema;missing;--schema-format;binary-v1;--validate-schema;--input;missing"
    "--schema;missing;--schema-format;binary-v1;--validate-schema;--output-format;human"
    "--schema;missing;--schema-format;binary-v1;--validate-schema;--profile;oracle-sre-v1"
    "--schema;missing;--schema-format;binary-v1;--validate-schema;--max-schema-work;-1"
    "--schema;missing;--schema-format;binary-v1;--validate-schema;--validate-schema"
    "--input;missing;--schema;missing;--max-schema-parser-bytes;1")
    expect_failure(2 usage ${args})
endforeach()

if(ENABLED)
    set(binary_examples "${ROOT}/spec/schema/examples/native-binary")
    foreach(stem IN ITEMS pair single ordinary)
        execute_process(COMMAND "${PROGRAM}" --schema "${binary_examples}/${stem}.json"
            --schema-format binary-v1 --validate-schema
            RESULT_VARIABLE binary_code OUTPUT_VARIABLE binary_out ERROR_VARIABLE binary_error)
        if(NOT binary_code EQUAL 0 OR NOT binary_out STREQUAL "schema-valid\t1\tqdcxx-native-binary\t1\n"
            OR NOT binary_error STREQUAL "")
            message(FATAL_ERROR "Binary validation ${stem}: ${binary_code}: ${binary_out}: ${binary_error}")
        endif()
    endforeach()
    foreach(axis IN ITEMS parser-bytes work model-bytes)
        expect_failure(6 "schema-error\t1\tRESOURCE_LIMIT\n"
            --schema "${binary_examples}/pair.json" --schema-format binary-v1 --validate-schema
            "--max-schema-${axis}" 0)
    endforeach()
    expect_failure(4 "schema-error\t1\tUNSUPPORTED_PROFILE\n"
        --schema "${examples}/empty.json" --schema-format binary-v1 --validate-schema)
    expect_failure(3 "schema-error\t1\tIO_ERROR\n"
        --schema "${RUN}/absent-binary.json" --schema-format binary-v1 --validate-schema)
    if(UNIX AND EXISTS "/dev/full")
        execute_process(COMMAND "${PROGRAM}" --schema "${binary_examples}/pair.json"
            --schema-format binary-v1 --validate-schema OUTPUT_FILE /dev/full
            RESULT_VARIABLE full_code ERROR_VARIABLE full_error)
        if(NOT full_code EQUAL 3 OR NOT full_error STREQUAL "schema-error\t1\tIO_ERROR\n")
            message(FATAL_ERROR "Binary success-output failure returned ${full_code}: ${full_error}")
        endif()
    endif()
else()
    expect_failure(4 "schema-error\t1\tLOADER_DISABLED\n"
        --schema missing --schema-format binary-v1 --validate-schema)
endif()

foreach(args IN ITEMS
    "--input;missing;--schema"
    "--input;missing;--schema;"
    "--input;missing;--schema;a;--profile;oracle-sre-v1"
    "--input;missing;--schema;a;--schema;b"
    "--schema;a"
    "--input;missing;--schema;-"
    "--input;missing;--schema;a;--max-schema-depth;-1"
    "--input;missing;--schema;a;--max-schema-bytes;18446744073709551616")
    expect_failure(2 usage ${args})
endforeach()

if(NOT ENABLED)
    expect_failure(4 "schema-error\t1\tLOADER_DISABLED\n"
        --input missing --schema missing)
    execute_process(COMMAND "${PROGRAM}" --input "${fixtures}/mixed-stream.qtp"
        --profile oracle-field-types-v1 RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
    if(NOT code EQUAL 0 OR NOT error STREQUAL "" OR NOT out MATCHES "field-record")
        message(FATAL_ERROR "Loader-disabled build lost built-in inspection")
    endif()
    return()
endif()

expect_failure(3 "schema-error\t1\tIO_ERROR\n" --input missing --schema "${RUN}/absent.json")
expect_failure(3 "schema-error\t1\tIO_ERROR\n" --input missing --schema "${RUN}")
file(WRITE "${RUN}/bad.json" "{")
expect_failure(5 "schema-error\t1\tMALFORMED_JSON\n" --input missing --schema "${RUN}/bad.json")
file(READ "${examples}/empty.json" empty)
string(REPLACE "qdcxx-native" "qd-3.355-xml-captured" old "${empty}")
file(WRITE "${RUN}/old.json" "${old}")
expect_failure(4 "schema-error\t1\tUNSUPPORTED_PROFILE\n" --input missing --schema "${RUN}/old.json")
expect_failure(6 "schema-error\t1\tRESOURCE_LIMIT\n"
    --input missing --schema "${examples}/empty.json" --max-schema-bytes 0)
expect_failure(6 "schema-error\t1\tRESOURCE_LIMIT\n"
    --input missing --schema "${examples}/field-types.json" --max-schema-fields 0)
expect_failure(3 open --input "${RUN}/absent.qtp" --schema "${examples}/empty.json")

file(READ "${examples}/field-types.json" typed)
foreach(case IN ITEMS type indexed duplicate version)
    set(changed "${typed}")
    set(exit_code 4)
    if(case STREQUAL type)
        string(REPLACE "\"serialTypeId\": 10" "\"serialTypeId\": 137" changed "${changed}")
        set(token UNSUPPORTED_TYPE)
    elseif(case STREQUAL indexed)
        string(REPLACE "\"hasTime\": false" "\"hasTime\": true" changed "${changed}")
        set(token UNSUPPORTED_INDEXED)
    elseif(case STREQUAL duplicate)
        string(REPLACE "\"formatVersion\": 1" "\"formatVersion\": 1, \"formatVersion\": 1" changed "${changed}")
        set(token INVALID_SCHEMA)
        set(exit_code 5)
    else()
        string(REPLACE "\"formatVersion\": 1" "\"formatVersion\": 2" changed "${changed}")
        set(token UNSUPPORTED_VERSION)
    endif()
    if(changed STREQUAL typed)
        message(FATAL_ERROR "Invalid test mutation ${case}")
    endif()
    file(WRITE "${RUN}/${case}.json" "${changed}")
    foreach(format IN ITEMS tsv human)
        foreach(version IN ITEMS 1 2)
            expect_failure(${exit_code} "schema-error\t1\t${token}\n" --input missing
                --schema "${RUN}/${case}.json" --output-format "${format}" --diagnostics-version "${version}")
        endforeach()
    endforeach()
endforeach()

file(SIZE "${examples}/field-types.json" schema_bytes)
file(COPY_FILE "${examples}/field-types.json" "${RUN}/schema with spaces.json")
execute_process(COMMAND "${PROGRAM}" --input "${fixtures}/mixed-stream.qtp"
    --schema "${RUN}/schema with spaces.json" --max-schema-bytes "${schema_bytes}"
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
if(NOT code EQUAL 0 OR NOT error STREQUAL "")
    message(FATAL_ERROR "Exact schema file byte bound/quoted path failed")
endif()
math(EXPR short_schema "${schema_bytes}-1")
expect_failure(6 "schema-error\t1\tRESOURCE_LIMIT\n" --input missing
    --schema "${examples}/field-types.json" --max-schema-bytes "${short_schema}")

foreach(stem IN ITEMS mixed-stream mixed-raw)
    if(stem STREQUAL mixed-raw)
        set(mode raw-message-v1)
    else()
        set(mode legacy)
    endif()
    foreach(format IN ITEMS tsv human)
        execute_process(COMMAND "${PROGRAM}" --input "${fixtures}/${stem}.qtp"
            --profile oracle-field-types-v1 --tape-mode "${mode}" --output-format "${format}"
            RESULT_VARIABLE code OUTPUT_VARIABLE baseline ERROR_VARIABLE error)
        if(NOT code EQUAL 0 OR NOT error STREQUAL "")
            message(FATAL_ERROR "Existing Java fixture baseline failed")
        endif()
        if(format STREQUAL tsv)
            string(REPLACE "format\tqdcxx-tape-inspect\t4\toracle-field-types-v1"
                "format\tqdcxx-tape-inspect\t6\texternal-schema-v1" expected "${baseline}")
        else()
            string(REPLACE "human 4 profile=oracle-field-types-v1"
                "human 6 profile=external-schema-v1" expected "${baseline}")
        endif()
        set(args --input "${fixtures}/${stem}.qtp" --schema "${examples}/field-types.json"
            --tape-mode "${mode}" --output-format "${format}")
        foreach(chunk IN ITEMS 1 7 4096)
            execute_process(COMMAND "${PROGRAM}" ${args} --chunk-size "${chunk}"
                RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
            if(NOT code EQUAL 0 OR NOT out STREQUAL expected OR NOT error STREQUAL "")
                message(FATAL_ERROR "External generic output mismatch ${stem}/${format}/${chunk}: ${code}: ${error}")
            endif()
        endforeach()
        string(LENGTH "${expected}" length)
        execute_process(COMMAND "${PROGRAM}" ${args} --max-output "${length}"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
        if(NOT code EQUAL 0 OR NOT out STREQUAL expected OR NOT error STREQUAL "")
            message(FATAL_ERROR "Exact external output limit failed")
        endif()
        math(EXPR short "${length}-1")
        execute_process(COMMAND "${PROGRAM}" ${args} --max-output "${short}"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
        if(NOT code EQUAL 6 OR out MATCHES "(^|\n)(complete\t|Complete:)" OR NOT error MATCHES "write")
            message(FATAL_ERROR "External short output limit published completion")
        endif()
        foreach(version IN ITEMS 1 2)
            execute_process(COMMAND "${PROGRAM}" ${args} --diagnostics-version "${version}"
                RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
            if(NOT code EQUAL 0 OR NOT out STREQUAL expected OR NOT error STREQUAL "")
                message(FATAL_ERROR "External diagnostics selection changed success")
            endif()
        endforeach()
        foreach(option IN ITEMS bytes depth records fields lookups name-units total-name-units)
            expect_failure(6 "schema-error\t1\tRESOURCE_LIMIT\n" ${args} "--max-schema-${option}" 0)
        endforeach()
    endforeach()
endforeach()
foreach(shape IN ITEMS empty empty-fields all-int)
    set(records "[]")
    if(shape STREQUAL empty-fields)
        set(records [=[[{"id":0,"name":"S","hasTime":false,"fields":[],"lookups":[]}]]=])
    elseif(shape STREQUAL all-int)
        set(records [=[[{"id":0,"name":"S","hasTime":false,"fields":[{"serialTypeId":8}],"lookups":[{"name":"v","fieldId":0}]}]]=])
    endif()
    file(WRITE "${RUN}/${shape}.json"
        "{\"formatVersion\":1,\"profile\":{\"id\":\"qdcxx-native\",\"version\":1},\"records\":${records}}")
    foreach(format IN ITEMS tsv human)
        execute_process(COMMAND "${PROGRAM}" --input "${ROOT}/tests/fixtures/protocol/tape/expected/constrained.qtp"
            --schema "${RUN}/${shape}.json" --output-format "${format}"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE error)
        if(NOT code EQUAL 0 OR NOT error STREQUAL "" OR NOT out MATCHES "external-schema-v1")
            message(FATAL_ERROR "External ${shape}/${format} profile failed: ${code}: ${error}")
        endif()
        if(shape STREQUAL "empty")
            if(out MATCHES "field-record|Fields [0-9]")
                message(FATAL_ERROR "Empty local schema exposed skipped wire records")
            endif()
        elseif(shape STREQUAL empty-fields)
            if(format STREQUAL tsv AND NOT out MATCHES "field-record[^\n]*\t0053\t004100420043\t\n")
                message(FATAL_ERROR "Empty fields lost the empty seventh TSV cell")
            elseif(format STREQUAL human AND NOT out MATCHES "fields=\\{\\}")
                message(FATAL_ERROR "Empty fields lost the generic human row")
            endif()
        elseif(format STREQUAL tsv AND NOT out MATCHES "\t8:7:-\n")
            message(FATAL_ERROR "All-int schema lost generic typed TSV values")
        elseif(format STREQUAL human AND NOT out MATCHES "type:8 raw:7")
            message(FATAL_ERROR "All-int schema lost generic typed human values")
        endif()
    endforeach()
endforeach()
message(STATUS "External schema CLI contract passed")
