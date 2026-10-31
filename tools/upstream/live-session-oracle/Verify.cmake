# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Caller: cmake -DJAVA_HOME=JDK8 -DUPSTREAM=LOCKED -DRECEIPT=TRUSTED
# -DNATIVE=GENERATED_FIXTURES -DRUN=FRESH [-DMODE=Verify] -P Verify.cmake.
# Reads a complete native capture and trusted providers; creates only a fresh RUN.
# Errors preserve partial logs; no input, upstream, native build or golden is overwritten.
cmake_minimum_required(VERSION 3.25)
set(usage [=[Native control-session verification
Usage:
  cmake [-DHELP=ON] -P Verify.cmake
  cmake -DJAVA_HOME=JDK8 -DUPSTREAM=LOCKED_QD -DRECEIPT=PREPARED_RECEIPT
        -DNATIVE=GENERATED_DIRECTORY -DRUN=NEW_DIRECTORY
        [-DMODE=Capture|Verify] [-DEXPECTED=DIRECTORY] -P Verify.cmake
Requires CMake 3.25+, Git, JDK 8, unchanged prepared QD classes and native CTest outputs.
NATIVE contains native-controls.qtp and expected.tsv from the native test.
MODE defaults to Capture: always compares production Java callbacks against native input expectations.
Verify also compares all four artifact files against EXPECTED (default tests/fixtures/live-session/expected).
RUN must not exist. Creates private classes/logs/receipts and deterministic RUN/artifacts.
No network, Maven build, native build, fixture publication or global setting changes occur.
No arguments and -DHELP=ON print identical help without providers, inputs or work.
Exit 0 help/success; 1 invalid input, provider drift, Java failure or exact comparison mismatch.
Java uses a 256 MiB heap and 30-second parse deadline; these are not process RSS guarantees.
]=])
if(DEFINED HELP AND NOT (CMAKE_ARGC EQUAL 4 AND CMAKE_ARGV1 STREQUAL "-DHELP=ON"))
    message(FATAL_ERROR "HELP must be standalone -DHELP=ON")
endif()
if(CMAKE_ARGC EQUAL 3 OR DEFINED HELP)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${usage}" RESULT_VARIABLE result)
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Help output failed")
    endif()
    return()
endif()
math(EXPR last "${CMAKE_ARGC} - 1")
cmake_path(ABSOLUTE_PATH CMAKE_ARGV${last} NORMALIZE OUTPUT_VARIABLE script)
if(NOT script STREQUAL CMAKE_CURRENT_LIST_FILE)
    message(FATAL_ERROR "Unexpected positional arguments")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../schema-export/Environment.cmake")
schema_export_arguments(JAVA_HOME UPSTREAM RECEIPT NATIVE RUN MODE EXPECTED)
schema_export_require(JAVA_HOME UPSTREAM RECEIPT NATIVE RUN)
if(NOT DEFINED MODE)
    set(MODE Capture)
endif()
if(NOT MODE MATCHES "^(Capture|Verify)$")
    message(FATAL_ERROR "MODE must be Capture or Verify")
endif()
if(NOT DEFINED EXPECTED)
    set(EXPECTED "${schema_export_repo}/tests/fixtures/live-session/expected")
endif()
cmake_path(ABSOLUTE_PATH EXPECTED NORMALIZE)
# Resolve an absent RUN through its nearest existing parent before checking containment.
function(resolved_path path output)
    set(parent "${path}")
    set(suffix "")
    while(NOT EXISTS "${parent}" AND NOT IS_SYMLINK "${parent}")
        cmake_path(GET parent FILENAME name)
        cmake_path(GET parent PARENT_PATH next)
        if(next STREQUAL parent OR next STREQUAL "")
            message(FATAL_ERROR "Cannot resolve launcher path")
        endif()
        set(suffix "${name}/${suffix}")
        set(parent "${next}")
    endwhile()
    file(REAL_PATH "${parent}" resolved)
    cmake_path(APPEND resolved "${suffix}")
    cmake_path(NORMAL_PATH resolved)
    if(WIN32)
        string(TOLOWER "${resolved}" resolved)
    endif()
    set(${output} "${resolved}" PARENT_SCOPE)
endfunction()
foreach(name RUN NATIVE EXPECTED UPSTREAM)
    resolved_path("${${name}}" resolved_${name})
endforeach()
cmake_path(IS_PREFIX resolved_NATIVE "${resolved_RUN}" NORMALIZE under_native)
cmake_path(IS_PREFIX resolved_EXPECTED "${resolved_RUN}" NORMALIZE under_expected)
cmake_path(IS_PREFIX resolved_UPSTREAM "${resolved_RUN}" NORMALIZE under_upstream)
if(under_native OR under_expected OR under_upstream)
    message(FATAL_ERROR "RUN must be separate from native inputs, upstream and goldens")
endif()
set(native_files native-controls.qtp expected.tsv)
foreach(name IN LISTS native_files)
    if(NOT EXISTS "${NATIVE}/${name}" OR IS_DIRECTORY "${NATIVE}/${name}"
        OR IS_SYMLINK "${NATIVE}/${name}")
        message(FATAL_ERROR "Missing or symbolic native fixture: ${name}")
    endif()
    file(SIZE "${NATIVE}/${name}" size)
    if((name STREQUAL "native-controls.qtp" AND size GREATER 1048576)
        OR size GREATER 8388608)
        message(FATAL_ERROR "Native fixture exceeds its byte budget: ${name}")
    endif()
    file(SHA256 "${NATIVE}/${name}" native_before_${name})
endforeach()
schema_export_environment()
schema_export_classpath(before_receipt classpath)
file(READ "${RECEIPT}" trusted_receipt)
if(NOT trusted_receipt STREQUAL before_receipt)
    message(FATAL_ERROR "Actual provider bytes differ from trusted preparation")
endif()
set(tool_files
    tools/upstream/live-session-oracle/Verify.cmake
    tools/upstream/live-session-oracle/NativeSessionParser.java
    tools/schema-export/Environment.cmake
    upstream.lock
    CMakeLists.txt
    tests/conformance/live_session_tests.cpp
    include/qd/protocol/live_session.hpp
    src/protocol/live_session.cpp
    src/protocol/live_decode.hpp
    src/protocol/field_time_binding.hpp
    src/protocol/session_budget.hpp
    include/qd/protocol/subscription_writer.hpp
    src/protocol/subscription_writer.cpp
    include/qd/protocol/frame_reader.hpp
    src/protocol/frame_reader.cpp
    include/qd/protocol/describe_records.hpp
    src/protocol/describe_records.cpp
    include/qd/protocol/static_binding.hpp
    src/protocol/static_binding.cpp
    include/qd/protocol/stream_data.hpp
    src/protocol/stream_data.cpp
    include/qd/protocol/tape_reader.hpp
    include/qd/protocol/tape_writer.hpp
    include/qd/core/local_schema.hpp
    src/core/local_schema.cpp
    src/core/schema_validation.hpp
    include/qd/core/serial_field_type.hpp
    include/qd/core/subscription.hpp
    src/core/subscription.cpp
    include/qd/encoding/compact.hpp
    src/encoding/compact.cpp
    src/encoding/compact_limits.hpp
    include/qd/encoding/utf_string.hpp
    src/encoding/utf_string.cpp
    include/qd/encoding/utf_char.hpp
    src/encoding/utf_char.cpp
    include/qd/encoding/char_array.hpp
    src/encoding/char_array.cpp
    include/qd/encoding/symbol_reader.hpp
    src/encoding/symbol_reader.cpp
    include/qd/events/quote.hpp
    src/events/quote.cpp
    include/qd/events/event_mapping.hpp
    src/events/event_mapping.cpp
    src/events/decimal_projection.hpp
    include/qd/core/wide_decimal.hpp
    src/core/wide_decimal.cpp
    src/core/wide_decimal_representation.hpp)
set(provenance "kind\tpath\tdigest\npin\tupstream.lock\t${schema_export_commit}\n")
string(SHA256 receipt_hash "${before_receipt}")
string(APPEND provenance "providers\tclasspath-receipt\t${receipt_hash}\n")
foreach(name IN LISTS native_files)
    string(APPEND provenance "input\t${name}\t${native_before_${name}}\n")
endforeach()
foreach(name java javac)
    file(SHA256 "${schema_export_${name}}" hash)
    string(APPEND provenance "runtime\t${name}\t${hash}\n")
endforeach()
foreach(name IN LISTS tool_files)
    file(SHA256 "${schema_export_repo}/${name}" tool_before_${name})
    string(APPEND provenance "tool\t${name}\t${tool_before_${name}}\n")
endforeach()
set(sources
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java
    qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java
    qd-core/src/main/java/com/devexperts/qd/qtp/MessageDescriptor.java
    qd-core/src/main/java/com/devexperts/qd/qtp/HeartbeatPayload.java
    dxlib/src/main/java/com/devexperts/io/BufferedInput.java)
foreach(name IN LISTS sources)
    file(SHA256 "${UPSTREAM}/${name}" hash)
    string(APPEND provenance "source\t${name}\t${hash}\n")
endforeach()
schema_export_fresh_run()
string(TIMESTAMP started "%Y-%m-%dT%H:%M:%SZ" UTC)
file(MAKE_DIRECTORY "${RUN}/classes")
file(WRITE "${RUN}/classpath-before.json" "${before_receipt}")
execute_process(COMMAND "${schema_export_java}" -version TIMEOUT 15 RESULT_VARIABLE result
    OUTPUT_FILE "${RUN}/java-version.stdout" ERROR_FILE "${RUN}/java-version.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Cannot record Java version")
endif()
file(SHA256 "${RUN}/java-version.stderr" hash)
string(APPEND provenance "runtime\tjava-version\t${hash}\n")
# Internal help-only failure injection: stdout throws before QD initialization.
file(CONFIGURE OUTPUT "${RUN}/NativeHelpFailure.java" CONTENT [=[
package qdcxx.oracle;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
public final class NativeHelpFailure {
  public static void main(String[] args) throws Exception {
    System.setOut(new PrintStream(new OutputStream() {
      @Override public void write(int value) throws IOException {
        throw new IOException("synthetic-help-output");
      }
    }, true, "UTF-8"));
    System.err.println("PROBE_READY");
    NativeSessionParser.main(new String[] {"--help"});
    System.exit(99);
  }
}
]=] @ONLY NEWLINE_STYLE UNIX)
file(CONFIGURE OUTPUT "${RUN}/NativeRuntime.java" CONTENT [=[
package qdcxx.oracle;
public final class NativeRuntime {
  public static void main(String[] args) {
    System.out.println(com.devexperts.qd.QDFactory.getVersion());
  }
}
]=] @ONLY NEWLINE_STYLE UNIX)
execute_process(COMMAND "${schema_export_javac}" -J-Xmx256m -encoding UTF-8 -source 8 -target 8
    -cp "${classpath}" -d "${RUN}/classes"
    "${CMAKE_CURRENT_LIST_DIR}/NativeSessionParser.java" "${RUN}/NativeHelpFailure.java"
    "${RUN}/NativeRuntime.java"
    TIMEOUT 60 RESULT_VARIABLE result OUTPUT_FILE "${RUN}/javac.stdout" ERROR_FILE "${RUN}/javac.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Oracle compilation failed; see RUN/javac.stderr")
endif()
foreach(kind noargs help)
    set(args)
    if(kind STREQUAL help)
        set(args --help)
    endif()
    # Deliberately omit QD providers: help must not initialize them.
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -Dfile.encoding=UTF-8
        -cp "${RUN}/classes" qdcxx.oracle.NativeSessionParser ${args}
        TIMEOUT 15 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/${kind}.stdout" ERROR_FILE "${RUN}/${kind}.stderr")
    file(SIZE "${RUN}/${kind}.stderr" error_size)
    if(NOT "${result}" STREQUAL "0" OR NOT error_size EQUAL 0)
        message(FATAL_ERROR "Help contract failed: ${kind}")
    endif()
endforeach()
file(SHA256 "${RUN}/noargs.stdout" noargs)
file(SHA256 "${RUN}/help.stdout" help)
file(SIZE "${RUN}/help.stdout" help_size)
if(NOT noargs STREQUAL help OR help_size EQUAL 0)
    message(FATAL_ERROR "Help/noargs mismatch")
endif()
execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${RUN}/classes"
    qdcxx.oracle.NativeHelpFailure TIMEOUT 15 RESULT_VARIABLE result
    OUTPUT_FILE "${RUN}/help-failure.stdout" ERROR_FILE "${RUN}/help-failure.stderr")
file(READ "${RUN}/help-failure.stderr" probe_error)
file(SIZE "${RUN}/help-failure.stdout" probe_output)
if(NOT "${result}" STREQUAL "1" OR NOT probe_output EQUAL 0
    OR NOT probe_error MATCHES "^PROBE_READY[\r]?\n$")
    message(FATAL_ERROR "Help output failure was hidden")
endif()
foreach(kind unknown incomplete duplicate mixed_help missing_value)
    if(kind STREQUAL unknown)
        set(args --unknown synthetic-argument)
    elseif(kind STREQUAL incomplete)
        set(args --input)
    elseif(kind STREQUAL duplicate)
        set(args --input first --input second)
    elseif(kind STREQUAL mixed_help)
        set(args --help extra)
    else()
        set(args --input --output --output unused)
    endif()
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${RUN}/classes"
        qdcxx.oracle.NativeSessionParser ${args} TIMEOUT 15 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/invalid-${kind}.stdout" ERROR_FILE "${RUN}/invalid-${kind}.stderr")
    file(SIZE "${RUN}/invalid-${kind}.stdout" output_size)
    file(READ "${RUN}/invalid-${kind}.stderr" error)
    if(NOT "${result}" STREQUAL "3" OR NOT output_size EQUAL 0
        OR NOT error MATCHES "^NativeSessionParser: invalid arguments; use --help[\r]?\n$")
        message(FATAL_ERROR "Invalid argument contract failed: ${kind}")
    endif()
endforeach()
file(MAKE_DIRECTORY "${RUN}/artifacts")
set(driver_classpath "${RUN}/classes${schema_export_separator}${classpath}")
execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
    qdcxx.oracle.NativeRuntime TIMEOUT 15 RESULT_VARIABLE result
    OUTPUT_FILE "${RUN}/qd-version.stdout" ERROR_FILE "${RUN}/qd-version.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Cannot observe QD runtime advertisement")
endif()
file(READ "${RUN}/qd-version.stdout" advertisement)
string(STRIP "${advertisement}" advertisement)
if(advertisement STREQUAL "" OR advertisement MATCHES "[\t\r\n]")
    message(FATAL_ERROR "Invalid QD runtime advertisement")
endif()
string(APPEND provenance "advertisement\tQDFactory.getVersion\t${advertisement}\n")
execute_process(COMMAND "${schema_export_java}" -Xmx256m -Dfile.encoding=UTF-8
    -Duser.language=en -Duser.country=US -Duser.timezone=UTC
    -cp "${driver_classpath}" qdcxx.oracle.NativeSessionParser
    --input "${NATIVE}/native-controls.qtp" --output "${RUN}/artifacts/actual.tsv"
    TIMEOUT 30 RESULT_VARIABLE result OUTPUT_FILE "${RUN}/parse.stdout" ERROR_FILE "${RUN}/parse.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Production parse failed; see RUN/parse.stderr")
endif()
foreach(pair actual.tsv:expected.tsv)
    string(REPLACE ":" ";" parts "${pair}")
    list(GET parts 0 actual)
    list(GET parts 1 expected)
    file(SHA256 "${RUN}/artifacts/${actual}" actual_hash)
    file(SHA256 "${NATIVE}/${expected}" expected_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Production callback/input expectation mismatch: ${actual}")
    endif()
endforeach()
file(SHA256 "${RUN}/artifacts/actual.tsv" outputs_before)
execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
    qdcxx.oracle.NativeSessionParser --input "${NATIVE}/native-controls.qtp" --output "${RUN}/artifacts/actual.tsv"
    TIMEOUT 30 RESULT_VARIABLE result OUTPUT_FILE "${RUN}/existing.stdout" ERROR_FILE "${RUN}/existing.stderr")
file(SHA256 "${RUN}/artifacts/actual.tsv" outputs_after)
if(NOT "${result}" STREQUAL "1" OR NOT outputs_before STREQUAL outputs_after)
    message(FATAL_ERROR "Existing output was accepted or changed")
endif()
foreach(name IN LISTS native_files)
    file(SHA256 "${NATIVE}/${name}" hash)
    if(NOT hash STREQUAL native_before_${name})
        message(FATAL_ERROR "Native input changed: ${name}")
    endif()
endforeach()
foreach(name IN LISTS tool_files)
    file(SHA256 "${schema_export_repo}/${name}" hash)
    if(NOT hash STREQUAL tool_before_${name})
        message(FATAL_ERROR "Tool source changed: ${name}")
    endif()
endforeach()
schema_export_environment()
schema_export_classpath(after_receipt unused)
if(NOT before_receipt STREQUAL after_receipt)
    message(FATAL_ERROR "Provider bytes changed")
endif()
file(WRITE "${RUN}/classpath-after.json" "${after_receipt}")
file(COPY_FILE "${NATIVE}/native-controls.qtp" "${RUN}/artifacts/native-controls.qtp")
file(CONFIGURE OUTPUT "${RUN}/artifacts/provenance.tsv" CONTENT "${provenance}" @ONLY NEWLINE_STYLE UNIX)
set(files native-controls.qtp actual.tsv provenance.tsv)
set(manifest "path\tbytes\tsha256\n")
foreach(name IN LISTS files)
    file(SIZE "${RUN}/artifacts/${name}" size)
    file(SHA256 "${RUN}/artifacts/${name}" hash)
    string(APPEND manifest "${name}\t${size}\t${hash}\n")
endforeach()
file(CONFIGURE OUTPUT "${RUN}/artifacts/manifest.tsv" CONTENT "${manifest}" @ONLY NEWLINE_STYLE UNIX)
list(APPEND files manifest.tsv)
if(MODE STREQUAL Verify)
    if(NOT IS_DIRECTORY "${EXPECTED}" OR IS_SYMLINK "${EXPECTED}")
        message(FATAL_ERROR "Golden root must be an ordinary directory")
    endif()
    file(GLOB golden_files LIST_DIRECTORIES true RELATIVE "${EXPECTED}" "${EXPECTED}/*")
    list(SORT golden_files)
    list(SORT files)
    if(NOT golden_files STREQUAL files)
        message(FATAL_ERROR "Golden artifact set differs")
    endif()
    foreach(name IN LISTS files)
        if(IS_SYMLINK "${EXPECTED}/${name}" OR IS_DIRECTORY "${EXPECTED}/${name}")
            message(FATAL_ERROR "Symbolic golden artifact")
        endif()
        file(SHA256 "${EXPECTED}/${name}" expected_hash)
        file(SHA256 "${RUN}/artifacts/${name}" actual_hash)
        if(NOT expected_hash STREQUAL actual_hash)
            message(FATAL_ERROR "Golden bytes differ: ${name}")
        endif()
    endforeach()
endif()
string(TIMESTAMP finished "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${RUN}/run.tsv" "mode\t${MODE}\nstartedUtc\t${started}\nfinishedUtc\t${finished}\nheapMiB\t256\nparseTimeoutSeconds\t30\n")
message(STATUS "${MODE} PASS: exact production control callbacks; source/providers/native inputs unchanged")
