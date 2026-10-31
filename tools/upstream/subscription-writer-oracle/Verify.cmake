# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Caller: cmake -DJAVA_HOME=JDK8 -DUPSTREAM=LOCKED -DRECEIPT=TRUSTED
# -DNATIVE=GENERATED_FIXTURES -DRUN=FRESH [-DMODE=Verify] -P Verify.cmake.
# Reads a complete native capture and trusted providers; creates only a fresh RUN.
# Errors preserve partial logs; no input, upstream, native build or golden is overwritten.
cmake_minimum_required(VERSION 3.25)
set(usage [=[Native subscription writer verification
Usage:
  cmake [-DHELP=ON] -P Verify.cmake
  cmake -DJAVA_HOME=JDK8 -DUPSTREAM=LOCKED_QD -DRECEIPT=PREPARED_RECEIPT
        -DNATIVE=GENERATED_DIRECTORY -DRUN=NEW_DIRECTORY
        [-DMODE=Capture|Verify] [-DEXPECTED=DIRECTORY] -P Verify.cmake
Requires CMake 3.25+, Git, JDK 8, unchanged prepared QD classes and native CTest outputs.
NATIVE contains native.qtp, expected.tsv and expected-time-reads.tsv from the native test.
MODE defaults to Capture: always compares production Java callbacks against native input expectations.
Verify also compares all five artifact files against EXPECTED (default committed writer fixtures).
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
    set(EXPECTED "${schema_export_repo}/tests/fixtures/subscriptions/writer/expected")
endif()
cmake_path(ABSOLUTE_PATH EXPECTED NORMALIZE)
cmake_path(IS_PREFIX NATIVE "${RUN}" NORMALIZE under_native)
cmake_path(IS_PREFIX EXPECTED "${RUN}" NORMALIZE under_expected)
cmake_path(IS_PREFIX UPSTREAM "${RUN}" NORMALIZE under_upstream)
if(under_native OR under_expected OR under_upstream)
    message(FATAL_ERROR "RUN must be separate from native inputs, upstream and goldens")
endif()
set(native_files native.qtp expected.tsv expected-time-reads.tsv)
foreach(name IN LISTS native_files)
    if(NOT EXISTS "${NATIVE}/${name}" OR IS_DIRECTORY "${NATIVE}/${name}"
        OR IS_SYMLINK "${NATIVE}/${name}")
        message(FATAL_ERROR "Missing or symbolic native fixture: ${name}")
    endif()
    file(SIZE "${NATIVE}/${name}" size)
    if(size GREATER 1048576)
        message(FATAL_ERROR "Native fixture exceeds 1 MiB: ${name}")
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
    tools/upstream/subscription-writer-oracle/Verify.cmake
    tools/upstream/subscription-writer-oracle/NativeSubscriptionParser.java
    tools/schema-export/Environment.cmake
    include/qd/protocol/subscription_writer.hpp
    src/protocol/subscription_writer.cpp
    tests/conformance/subscription_writer_tests.cpp
    include/qd/core/local_schema.hpp
    src/core/local_schema.cpp
    include/qd/encoding/compact.hpp
    src/encoding/compact.cpp
    include/qd/encoding/char_array.hpp
    src/encoding/char_array.cpp
    include/qd/encoding/utf_string.hpp
    src/encoding/utf_string.cpp)
set(provenance "kind\tpath\tdigest\npin\tupstream.lock\t${schema_export_commit}\n")
foreach(name IN LISTS tool_files)
    file(SHA256 "${schema_export_repo}/${name}" tool_before_${name})
    string(APPEND provenance "tool\t${name}\t${tool_before_${name}}\n")
endforeach()
set(sources
    qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPParser.java
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java
    qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java)
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
    NativeSubscriptionParser.main(new String[] {"--help"});
    System.exit(99);
  }
}
]=] @ONLY NEWLINE_STYLE UNIX)
execute_process(COMMAND "${schema_export_javac}" -J-Xmx256m -encoding UTF-8 -source 8 -target 8
    -cp "${classpath}" -d "${RUN}/classes"
    "${CMAKE_CURRENT_LIST_DIR}/NativeSubscriptionParser.java" "${RUN}/NativeHelpFailure.java"
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
        -cp "${RUN}/classes" qdcxx.oracle.NativeSubscriptionParser ${args}
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
set(driver_classpath "${RUN}/classes${schema_export_separator}${classpath}")
execute_process(COMMAND "${schema_export_java}" -Xmx256m -Dfile.encoding=UTF-8
    -Duser.language=en -Duser.country=US -Duser.timezone=UTC
    -cp "${driver_classpath}" qdcxx.oracle.NativeSubscriptionParser
    --input "${NATIVE}/native.qtp" --output "${RUN}/artifacts"
    TIMEOUT 30 RESULT_VARIABLE result OUTPUT_FILE "${RUN}/parse.stdout" ERROR_FILE "${RUN}/parse.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Production parse failed; see RUN/parse.stderr")
endif()
foreach(pair observations.tsv:expected.tsv time-reads.tsv:expected-time-reads.tsv)
    string(REPLACE ":" ";" parts "${pair}")
    list(GET parts 0 actual)
    list(GET parts 1 expected)
    file(SHA256 "${RUN}/artifacts/${actual}" actual_hash)
    file(SHA256 "${NATIVE}/${expected}" expected_hash)
    if(NOT actual_hash STREQUAL expected_hash)
        message(FATAL_ERROR "Production callback/input expectation mismatch: ${actual}")
    endif()
endforeach()
schema_export_hash_tree("${RUN}/artifacts" outputs_before output_count)
execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
    qdcxx.oracle.NativeSubscriptionParser --input "${NATIVE}/native.qtp" --output "${RUN}/artifacts"
    TIMEOUT 30 RESULT_VARIABLE result OUTPUT_FILE "${RUN}/existing.stdout" ERROR_FILE "${RUN}/existing.stderr")
schema_export_hash_tree("${RUN}/artifacts" outputs_after output_count_after)
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
file(COPY_FILE "${NATIVE}/native.qtp" "${RUN}/artifacts/native.qtp")
file(CONFIGURE OUTPUT "${RUN}/artifacts/provenance.tsv" CONTENT "${provenance}" @ONLY NEWLINE_STYLE UNIX)
set(files native.qtp observations.tsv provenance.tsv time-reads.tsv)
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
message(STATUS "${MODE} PASS: exact production callbacks and history-time reads; source/providers/native inputs unchanged")
