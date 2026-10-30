# Actual executable checks, using only explicitly named temporary paths and bounded small output files.
# PROGRAM/COMPARATOR are actual native executables; ROOT supplies reviewed immutable fixtures; RUN is owned scratch.
file(MAKE_DIRECTORY "${RUN}")
set(base --source-profile self-contained-segments-v1 --wire-schema binary-catalog-v1 --tape-mode binary-catalog-v1)
execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE code OUTPUT_VARIABLE no_args ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT err STREQUAL "" OR no_args STREQUAL "")
    message(FATAL_ERROR "Owning no-argument help")
endif()
execute_process(COMMAND "${PROGRAM}" --help RESULT_VARIABLE code OUTPUT_VARIABLE help ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT err STREQUAL "" OR NOT help STREQUAL no_args OR
   NOT help MATCHES "self-contained-segments-v1" OR NOT help MATCHES "--aggregate-reservation-copies 1")
    message(FATAL_ERROR "Owning standalone help/defaults")
endif()
foreach(args IN ITEMS
    "${base};--time-policy;none"
    "${base};--input;missing"
    "${base};--time-policy;auto;--input;missing"
    "${base};--time-policy;none;--input;missing;--diagnostics-version;2"
    "${base};--time-policy;none;--input;missing;--time-input;missing"
    "${base};--time-policy;none;--input;missing;--time-container;none"
    "${base};--time-policy;none;--input;missing;--time-lines;0"
    "${base};--time-policy;long;--input;missing"
    "${base};--time-policy;long;--input;missing;--time-input;missing;--time-input;missing"
    "${base};--time-policy;none;--input;-"
    "${base};--time-policy;none;--input;missing;--aggregate-reservation-copies;0"
    "${base};--time-policy;none;--input;missing;--aggregate-reservation-copies;257"
    "${base};--time-policy;none;--input;missing;--chunk-size;0"
    "${base};--time-policy;none;--input;missing;--source-chunk-bytes;0"
    "${base};--time-policy;none;--input;missing;--max-input;-1"
    "${base};--time-policy;none;--input;missing;--max-input;18446744073709551616"
    "${base};--time-policy;none;--input;missing;--time-policy;none"
    "${base};--time-policy;none;--input;missing;--schema;missing"
    "${base};--time-policy;none;--input;missing;--help")
    execute_process(COMMAND "${PROGRAM}" ${args} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT err MATCHES "incomplete-v3\t2\tusage")
        message(FATAL_ERROR "Owning usage case: ${args}; code=${code}; error=${err}")
    endif()
endforeach()
# A legacy path operand that looks like an option must never select the new dispatch branch.
set(legacy_fixture "${ROOT}/tests/fixtures/protocol/tape/expected/constrained.qtp")
file(COPY_FILE "${legacy_fixture}" "${RUN}/--source-profile")
execute_process(COMMAND "${PROGRAM}" --input "${legacy_fixture}" --profile oracle-sre-v1
    RESULT_VARIABLE reference_code OUTPUT_VARIABLE reference_out ERROR_VARIABLE reference_err)
execute_process(COMMAND "${PROGRAM}" --input --source-profile --profile oracle-sre-v1 WORKING_DIRECTORY "${RUN}"
    RESULT_VARIABLE literal_code OUTPUT_VARIABLE literal_out ERROR_VARIABLE literal_err)
if(NOT reference_code EQUAL 0 OR NOT reference_err STREQUAL "" OR NOT literal_code EQUAL 0 OR
   NOT literal_err STREQUAL "" OR NOT literal_out STREQUAL reference_out)
    message(FATAL_ERROR "Legacy literal option-like filename changed dispatch/output")
endif()
execute_process(COMMAND "${PROGRAM}" --input missing --profile --source-profile
    RESULT_VARIABLE literal_code OUTPUT_VARIABLE literal_out ERROR_VARIABLE literal_err)
if(NOT literal_code EQUAL 2 OR NOT literal_out STREQUAL "" OR literal_err MATCHES "incomplete-v3" OR NOT literal_err MATCHES "usage")
    message(FATAL_ERROR "Legacy literal option-like profile operand changed diagnostic dispatch")
endif()
# Resolve the command-wide diagnostic cap before any failing argument, independent of ordering.
foreach(prefix IN ITEMS
    "${base};--input;missing;--time-policy;none;--bogus;1"
    "${base};--input;missing;--time-policy;invalid"
    "${base};--input;missing;--time-policy;none;--time-policy;none"
    "${base};--input;missing;--time-policy;none;--max-output;invalid"
    "${base};--input;missing;--time-policy;none;--max-output;1000000;--max-output;2000000")
    execute_process(COMMAND "${PROGRAM}" ${prefix} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE uncapped_error)
    if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR NOT uncapped_error MATCHES "^incomplete-v3\t2\tusage")
        message(FATAL_ERROR "Diagnostic-cap regression reference")
    endif()
    string(LENGTH "${uncapped_error}" exact_diagnostic)
    math(EXPR below_diagnostic "${exact_diagnostic} - 1")
    math(EXPR above_diagnostic "${exact_diagnostic} + 1")
    foreach(cap IN ITEMS 0 "${below_diagnostic}" "${exact_diagnostic}" "${above_diagnostic}")
        foreach(placement IN ITEMS before after)
            if(placement STREQUAL before)
                set(arguments --max-output "${cap}" ${prefix})
            else()
                set(arguments ${prefix} --max-output "${cap}")
            endif()
            execute_process(COMMAND "${PROGRAM}" ${arguments} RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
            string(LENGTH "${err}" diagnostic_bytes)
            if(NOT code EQUAL 2 OR NOT out STREQUAL "" OR diagnostic_bytes GREATER cap)
                message(FATAL_ERROR "Pre-work output cap escaped: ${placement}, cap=${cap}, bytes=${diagnostic_bytes}")
            endif()
            if(placement STREQUAL after)
                if(cap LESS exact_diagnostic)
                    if(NOT err STREQUAL "")
                        message(FATAL_ERROR "A partial diagnostic escaped its exact finite gate")
                    endif()
                elseif(NOT err STREQUAL uncapped_error)
                    message(FATAL_ERROR "Exact/excess diagnostic gate changed the first argument cause")
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()
set(fixture "${RUN}/small source.qtp")
execute_process(COMMAND "${COMPARATOR}" --fixture "${fixture}" 17 RESULT_VARIABLE code OUTPUT_VARIABLE generated ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT err STREQUAL "")
    message(FATAL_ERROR "Fixture generator: ${code}: ${err}")
endif()
# Exercise native non-ASCII names and spaces through the actual process boundary.
set(native "${RUN}/Юникод 日本 source.qtp")
file(COPY_FILE "${fixture}" "${native}")
foreach(chunk IN ITEMS 1 4096)
    set(output "${RUN}/small-${chunk}.tsv")
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${native}" --chunk-size "${chunk}"
        RESULT_VARIABLE code OUTPUT_FILE "${output}" ERROR_VARIABLE err)
    if(NOT code EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "Actual owning CLI: ${code}: ${err}")
    endif()
    file(READ "${output}" actual_rows)
    foreach(default_row IN ITEMS "tape-context-schema-fields\t16384" "tape-context-schema-lookups\t32768"
        "tape-data-ownedFieldTimeRecords\t1048576" "outputBytes\t16777216")
        if(NOT actual_rows MATCHES "resource\tsequence\t-\t${default_row}\t-")
            message(FATAL_ERROR "Owning explicit default reserve missing: ${default_row}")
        endif()
    endforeach()
    execute_process(COMMAND "${COMPARATOR}" --verify "${native}" "${output}" 17 RESULT_VARIABLE code OUTPUT_VARIABLE checked ERROR_VARIABLE err)
    if(NOT code EQUAL 0 OR NOT err STREQUAL "")
        message(FATAL_ERROR "Actual CLI canonical equality: ${code}: ${err}")
    endif()
endforeach()
# Prove the bounded verifier rejects a changed business byte and a false actual resource observation.
foreach(mutation IN ITEMS record resource)
    if(mutation STREQUAL record)
        string(REPLACE "8:0:-:-" "8:1:-:-" changed_rows "${actual_rows}")
        set(expected_failure "canonical equality")
    else()
        file(SIZE "${native}" source_size)
        math(EXPR false_size "${source_size} + 1")
        string(REPLACE "resource\tparser\t0\tparserAccepted\t67108864\t${source_size}\n"
            "resource\tparser\t0\tparserAccepted\t67108864\t${false_size}\n" changed_rows "${actual_rows}")
        set(expected_failure "parser accepted observation")
    endif()
    if(changed_rows STREQUAL actual_rows)
        message(FATAL_ERROR "Verifier mutation did not alter its intended row")
    endif()
    set(changed_file "${RUN}/changed-${mutation}.tsv")
    file(CONFIGURE OUTPUT "${changed_file}" CONTENT "${changed_rows}" @ONLY NEWLINE_STYLE UNIX)
    execute_process(COMMAND "${COMPARATOR}" --verify "${native}" "${changed_file}" 17
        RESULT_VARIABLE code OUTPUT_VARIABLE checked ERROR_VARIABLE err)
    if(NOT code EQUAL 1 OR NOT err MATCHES "${expected_failure}")
        message(FATAL_ERROR "Verifier accepted or misclassified ${mutation} mutation: ${code}: ${err}")
    endif()
endforeach()
# Full declared reservations, rather than tiny actual source size, govern repeated occurrences.
foreach(copies IN ITEMS 1 2 256)
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" --input "${fixture}"
        --aggregate-reservation-copies "${copies}" RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(copies EQUAL 1)
        if(NOT code EQUAL 6 OR NOT out STREQUAL "" OR NOT err MATCHES "admission")
            message(FATAL_ERROR "Copies1 admitted two declared source vectors")
        endif()
    elseif(NOT code EQUAL 0 OR NOT err STREQUAL "" OR NOT out MATCHES "source-begin\t0" OR
           NOT out MATCHES "source-begin\t1" OR NOT out MATCHES "complete-v9\t2")
        message(FATAL_ERROR "Copies${copies} repeated occurrence: ${code}: ${err}")
    endif()
endforeach()
foreach(option IN ITEMS --max-sources --max-path-units --max-total-path-units --max-path-output-bytes
    --max-source-output --max-output --max-physical-probes --max-inflated-probes
    --max-input --max-frame --max-frames --max-records --max-fields --max-values --max-construction
    --max-snapshot-units --source-compressed-bytes --source-inflated-bytes --source-work-bytes --source-retained-bytes)
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" "${option}" 0
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code EQUAL 6 OR out MATCHES "complete-v9")
        message(FATAL_ERROR "Literal zero gate ${option}: ${code}: ${err}")
    endif()
endforeach()
# Exact independent workload boundaries: seventeen one-field attempts, three frames and actual bytes.
file(SIZE "${fixture}" physical_bytes)
string(LENGTH "${fixture}" native_units)
if(WIN32)
    math(EXPR path_encoded "${native_units} * 4")
else()
    math(EXPR path_encoded "${native_units} * 2")
endif()
foreach(boundary IN ITEMS "--max-input;${physical_bytes}" "--source-compressed-bytes;${physical_bytes}"
    "--source-inflated-bytes;${physical_bytes}" "--max-records;17" "--max-fields;17" "--max-values;17"
    "--max-frames;3" "--max-sources;1" "--max-physical-probes;1" "--max-inflated-probes;1"
    "--max-path-units;${native_units}" "--max-total-path-units;${native_units}" "--max-path-output-bytes;${path_encoded}")
    list(GET boundary 0 option)
    list(GET boundary 1 exact)
    math(EXPR below "${exact} - 1")
    math(EXPR above "${exact} + 1")
    foreach(value IN ITEMS "${below}" "${exact}" "${above}")
        execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" "${option}" "${value}"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(value EQUAL below)
            if(NOT code EQUAL 6 OR out MATCHES "complete-v9")
                message(FATAL_ERROR "L-1 not refused: ${option}=${value}: ${code}: ${err}")
            endif()
        elseif(NOT code EQUAL 0 OR NOT err STREQUAL "" OR NOT out MATCHES "complete-v9")
            message(FATAL_ERROR "Exact/excess finite bound: ${option}=${value}: ${code}: ${err}")
        endif()
    endforeach()
endforeach()
# Counters irrelevant to this NONE/source/type8 workload still retain their literal zero meaning.
foreach(option IN ITEMS --max-owned-field-time-records --source-gzip-members --source-zip-entries
    --source-header-bytes --source-total-header-bytes --source-inflate-calls --source-inflater-bytes
    --field-payload-encoded-bytes --field-payload-bytes --max-payload-encoded-bytes --max-payload-work-bytes
    --max-owned-payload-bytes --max-owned-field-units --max-field-units --max-field-bytes)
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" "${option}" 0
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code EQUAL 0 OR NOT err STREQUAL "" OR NOT out MATCHES "complete-v9")
        message(FATAL_ERROR "Inactive literal zero ${option}: ${code}: ${err}")
    endif()
endforeach()
foreach(option IN ITEMS --max-input --source-work-bytes --source-expansion-ratio)
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" --aggregate-reservation-copies 2
        "${option}" 18446744073709551615 RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
    if(NOT code EQUAL 6 OR NOT out STREQUAL "" OR NOT err MATCHES "unrepresentable")
        message(FATAL_ERROR "Checked aggregate product overflow ${option}: ${code}: ${err}")
    endif()
endforeach()
execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}" --output-format human
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT code EQUAL 0 OR NOT err STREQUAL "" OR NOT out MATCHES "Fields9" OR NOT out MATCHES "source-policy\tnone\tnone\t-")
    message(FATAL_ERROR "Owning HUMAN output")
endif()
if(DEFINED COMPRESSION AND NOT COMPRESSION)
    foreach(kind IN ITEMS gzip zip)
        execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input missing --container "${kind}"
            RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err)
        if(NOT code EQUAL 4 OR NOT out STREQUAL "" OR NOT err MATCHES "configuration\tcompression-disabled")
            message(FATAL_ERROR "OFF refusal before source work: ${kind}: ${code}: ${err}")
        endif()
    endforeach()
endif()
if(UNIX AND EXISTS /dev/full)
    execute_process(COMMAND "${PROGRAM}" --help OUTPUT_FILE /dev/full RESULT_VARIABLE code ERROR_VARIABLE err)
    if(NOT code EQUAL 3)
        message(FATAL_ERROR "Broken help output")
    endif()
    execute_process(COMMAND "${PROGRAM}" ${base} --time-policy none --input "${fixture}"
        OUTPUT_FILE /dev/full RESULT_VARIABLE code ERROR_VARIABLE err)
    if(NOT code EQUAL 3)
        message(FATAL_ERROR "Owning broken output")
    endif()
endif()
file(REMOVE "${RUN}/changed-record.tsv" "${RUN}/changed-resource.tsv" "${RUN}/--source-profile" "${fixture}" "${native}" "${RUN}/small-1.tsv" "${RUN}/small-4096.tsv")
message(STATUS "Actual source profile CLI contracts passed")
