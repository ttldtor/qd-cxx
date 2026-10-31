# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Caller: cmake -DJAVA_HOME=... -DUPSTREAM=... -DRUN=NEW -DRECEIPT=... -P Capture.cmake.
# Inputs: locked clean Java source and a trusted prepared provider receipt.
# Outputs: fresh RUN/classes, deterministic RUN/artifacts, private raw/logs/receipts.
# Failure: nonzero; partial RUN remains available, no input or expected file is rewritten.
cmake_minimum_required(VERSION 3.25)
set(usage [=[Subscription production oracle
Usage:
  cmake [-DHELP=ON] -P Capture.cmake
  cmake -DJAVA_HOME=JDK8 -DUPSTREAM=LOCKED_QD -DRUN=NEW_DIRECTORY
        -DRECEIPT=PREPARED_RECEIPT [-DMODE=Capture|Verify] [-DEXPECTED=DIRECTORY]
        -P Capture.cmake
MODE defaults to Capture; Verify compares against the published subscription fixtures.
JAVA_HOME, UPSTREAM, RUN and RECEIPT are mandatory for work; RUN must not exist.
Requires CMake 3.25+, PowerShell 7.4+, JDK 8 and already prepared locked QD classes.
No network, Maven build, fixture publication or global configuration changes occur.
Creates RUN with bounded Java captures, logs and private clock-dependent raw bytes.
Exit 0: help/success. Exit 1: invalid input, provider drift, observation or comparison failure.
Help does not require providers and starts no capture.
]=])
if(DEFINED HELP AND NOT (CMAKE_ARGC EQUAL 4 AND CMAKE_ARGV1 STREQUAL "-DHELP=ON"))
    message(FATAL_ERROR "HELP must be standalone -DHELP=ON")
endif()
if(CMAKE_ARGC EQUAL 3 OR DEFINED HELP)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${usage}" RESULT_VARIABLE help_result)
    if(NOT "${help_result}" STREQUAL "0")
        message(FATAL_ERROR "Cannot write help")
    endif()
    return()
endif()
math(EXPR last_arg "${CMAKE_ARGC} - 1")
cmake_path(ABSOLUTE_PATH CMAKE_ARGV${last_arg} NORMALIZE OUTPUT_VARIABLE last_path)
if(NOT last_path STREQUAL CMAKE_CURRENT_LIST_FILE)
    message(FATAL_ERROR "Unexpected positional arguments; use standalone -DHELP=ON")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../schema-export/Environment.cmake")
schema_export_arguments(JAVA_HOME UPSTREAM RUN RECEIPT MODE EXPECTED)
schema_export_require(JAVA_HOME UPSTREAM RUN RECEIPT)
if(NOT DEFINED MODE)
    set(MODE Capture)
endif()
if(NOT MODE MATCHES "^(Capture|Verify)$")
    message(FATAL_ERROR "MODE must be Capture or Verify")
endif()
if(NOT DEFINED EXPECTED)
    set(EXPECTED "${schema_export_repo}/tests/fixtures/subscriptions/expected")
endif()
cmake_path(ABSOLUTE_PATH EXPECTED NORMALIZE)
cmake_path(IS_PREFIX EXPECTED "${RUN}" NORMALIZE under_expected)
if(under_expected)
    message(FATAL_ERROR "RUN must be separate from published expectations")
endif()
if(MODE STREQUAL Verify AND (NOT IS_DIRECTORY "${EXPECTED}" OR IS_SYMLINK "${EXPECTED}"))
    message(FATAL_ERROR "EXPECTED must be an existing ordinary directory")
endif()
schema_export_environment()
schema_export_classpath(before_receipt classpath)
file(READ "${RECEIPT}" prepared_receipt)
if(NOT before_receipt STREQUAL prepared_receipt)
    message(FATAL_ERROR "Prepared provider receipt differs from actual class bytes")
endif()
find_program(subscription_pwsh pwsh REQUIRED)
set(tool_files Capture.cmake Compare-Oracle.ps1 WireOracle.java CollectorOracle.java
    SessionOracle.java tests/HelpFailureProbe.java tests/Test-Comparison.ps1)
set(provenance "kind\tpath\tdigest\npin\tupstream.lock\t${schema_export_commit}\n")
foreach(name IN LISTS tool_files)
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/${name}" hash)
    set(tool_before_${name} "${hash}")
    string(APPEND provenance "tool\ttools/upstream/subscription-oracle/${name}\t${hash}\n")
endforeach()
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/../../schema-export/Environment.cmake" environment_hash)
string(APPEND provenance "tool\ttools/schema-export/Environment.cmake\t${environment_hash}\n")
set(providers
    qd-core/src/main/java/com/devexperts/qd/QDFactory.java
    qd-core/src/main/java/com/devexperts/qd/QDCollector.java
    qd-core/src/main/java/com/devexperts/qd/QDAgent.java
    qd-core/src/main/java/com/devexperts/qd/QDDistributor.java
    qd-core/src/main/java/com/devexperts/qd/QDStream.java
    qd-core/src/main/java/com/devexperts/qd/QDTicker.java
    qd-core/src/main/java/com/devexperts/qd/QDHistory.java
    qd-core/src/main/java/com/devexperts/qd/kit/PentaCodec.java
    qd-core/src/main/java/com/devexperts/qd/qtp/AbstractQTPComposer.java
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPComposer.java
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryQTPParser.java
    qd-core/src/main/java/com/devexperts/qd/qtp/ConnectionQTPParser.java
    qd-core/src/main/java/com/devexperts/qd/qtp/BinaryRecordDesc.java
    qd-core/src/main/java/com/devexperts/qd/qtp/ProtocolDescriptor.java
    qd-core/src/main/java/com/devexperts/qd/qtp/HeartbeatPayload.java
    qd-core/src/main/java/com/devexperts/qd/qtp/MessageAdapter.java
    qd-core/src/main/java/com/devexperts/qd/qtp/MessageAdapterConnection.java
    qd-core/src/main/java/com/devexperts/qd/qtp/AgentAdapter.java
    qd-core/src/main/java/com/devexperts/qd/qtp/DistributorAdapter.java
    qd-core/src/main/java/com/devexperts/qd/qtp/AuthManager.java
    qd-core/src/main/java/com/devexperts/qd/qtp/LoginManager.java
    qd-core/src/main/java/com/devexperts/qd/qtp/auth/BasicChannelShaperFactory.java
    qd-core/src/main/java/com/devexperts/qd/ng/RecordMode.java
    qd-rmi/src/main/java/com/devexperts/rmi/impl/ComposedMessage.java
    qd-core/src/main/java/com/devexperts/qd/impl/matrix/Stream.java
    qd-core/src/main/java/com/devexperts/qd/impl/matrix/Ticker.java
    qd-core/src/main/java/com/devexperts/qd/impl/matrix/History.java)
foreach(name IN LISTS providers)
    file(SHA256 "${UPSTREAM}/${name}" hash)
    string(APPEND provenance "source\t${name}\t${hash}\n")
endforeach()
schema_export_fresh_run()
file(MAKE_DIRECTORY "${RUN}/classes" "${RUN}/artifacts")
file(WRITE "${RUN}/classpath-before.json" "${before_receipt}")
string(TIMESTAMP started "%Y-%m-%dT%H:%M:%SZ" UTC)
execute_process(COMMAND "${schema_export_javac}" -J-Xmx256m -encoding UTF-8 -source 8 -target 8
    -cp "${classpath}" -d "${RUN}/classes"
    "${CMAKE_CURRENT_LIST_DIR}/WireOracle.java"
    "${CMAKE_CURRENT_LIST_DIR}/CollectorOracle.java"
    "${CMAKE_CURRENT_LIST_DIR}/SessionOracle.java"
    "${CMAKE_CURRENT_LIST_DIR}/tests/HelpFailureProbe.java"
    TIMEOUT 60 RESULT_VARIABLE compile_result
    OUTPUT_FILE "${RUN}/javac.stdout" ERROR_FILE "${RUN}/javac.stderr")
if(NOT "${compile_result}" STREQUAL "0")
    message(FATAL_ERROR "Oracle compilation failed; see RUN/javac.stderr")
endif()
set(driver_classpath "${RUN}/classes${schema_export_separator}${classpath}")
set(main_wire qdcxx.oracle.WireOracle)
set(main_collectors qdcxx.oracle.CollectorOracle)
set(main_session qdcxx.oracle.SessionOracle)
set(flag_wire --capture)
set(flag_collectors --output)
set(flag_session --out)
set(existing_wire 2)
set(existing_collectors 3)
set(existing_session 1)
foreach(driver wire collectors session)
    foreach(help_kind noargs help)
        set(help_args)
        if(help_kind STREQUAL help)
            set(help_args --help)
        endif()
        execute_process(COMMAND "${schema_export_java}" -Xmx256m -Dfile.encoding=UTF-8
            -Duser.language=en -Duser.country=US -Duser.timezone=UTC
            -cp "${driver_classpath}" "${main_${driver}}" ${help_args}
            TIMEOUT 15 RESULT_VARIABLE result
            OUTPUT_FILE "${RUN}/${driver}-${help_kind}.stdout"
            ERROR_FILE "${RUN}/${driver}-${help_kind}.stderr")
        file(SIZE "${RUN}/${driver}-${help_kind}.stderr" stderr_size)
        if(NOT "${result}" STREQUAL "0" OR NOT stderr_size EQUAL 0)
            message(FATAL_ERROR "Oracle help contract failed: ${driver}/${help_kind}")
        endif()
    endforeach()
    file(SHA256 "${RUN}/${driver}-noargs.stdout" noargs_hash)
    file(SHA256 "${RUN}/${driver}-help.stdout" help_hash)
    file(SIZE "${RUN}/${driver}-help.stdout" help_size)
    if(NOT noargs_hash STREQUAL help_hash OR help_size EQUAL 0)
        message(FATAL_ERROR "Noargs/help output differs: ${driver}")
    endif()
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
        qdcxx.oracle.HelpFailureProbe "${main_${driver}}"
        TIMEOUT 15 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/${driver}-help-failure.stdout"
        ERROR_FILE "${RUN}/${driver}-help-failure.stderr")
    set(help_failure_expected 1)
    if(driver STREQUAL wire)
        set(help_failure_expected 2)
    endif()
    file(READ "${RUN}/${driver}-help-failure.stderr" probe_stderr)
    file(SIZE "${RUN}/${driver}-help-failure.stdout" probe_stdout_size)
    if(NOT "${result}" STREQUAL "${help_failure_expected}" OR NOT probe_stdout_size EQUAL 0
        OR NOT probe_stderr MATCHES "^PROBE_READY[\r]?\n$")
        message(FATAL_ERROR "Help output failure was hidden: ${driver}")
    endif()
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
        "${main_${driver}}" --unknown-option
        TIMEOUT 15 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/${driver}-invalid.stdout"
        ERROR_FILE "${RUN}/${driver}-invalid.stderr")
    if(NOT "${result}" STREQUAL "3")
        message(FATAL_ERROR "Invalid arguments were accepted: ${driver}")
    endif()
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -Dfile.encoding=UTF-8
        -Duser.language=en -Duser.country=US -Duser.timezone=UTC
        -cp "${driver_classpath}" "${main_${driver}}"
        "${flag_${driver}}" "${RUN}/artifacts/${driver}"
        TIMEOUT 60 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/${driver}.stdout" ERROR_FILE "${RUN}/${driver}.stderr")
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Production observation failed: ${driver}; see RUN/${driver}.stderr")
    endif()
    foreach(required cases.tsv observations.tsv)
        if(NOT EXISTS "${RUN}/artifacts/${driver}/${required}")
            message(FATAL_ERROR "Missing production artifact: ${driver}/${required}")
        endif()
    endforeach()
    schema_export_hash_tree("${RUN}/artifacts/${driver}" capture_hash capture_count)
    execute_process(COMMAND "${schema_export_java}" -Xmx256m -cp "${driver_classpath}"
        "${main_${driver}}" "${flag_${driver}}" "${RUN}/artifacts/${driver}"
        TIMEOUT 15 RESULT_VARIABLE result
        OUTPUT_FILE "${RUN}/${driver}-existing.stdout"
        ERROR_FILE "${RUN}/${driver}-existing.stderr")
    schema_export_hash_tree("${RUN}/artifacts/${driver}" existing_hash existing_count)
    if(NOT "${result}" STREQUAL "${existing_${driver}}"
        OR NOT existing_hash STREQUAL capture_hash OR NOT existing_count EQUAL capture_count)
        message(FATAL_ERROR "Existing output was modified or accepted: ${driver}")
    endif()
endforeach()
# Fresh RUN owns both paths. Check resolved roots before moving the private raw directory.
file(REAL_PATH "${RUN}" run_real)
file(REAL_PATH "${RUN}/artifacts/session/raw" raw_real)
cmake_path(IS_PREFIX run_real "${raw_real}" NORMALIZE raw_inside)
set(raw_target "${run_real}/session-raw")
cmake_path(IS_PREFIX run_real "${raw_target}" NORMALIZE target_inside)
if(NOT raw_inside OR NOT target_inside OR IS_SYMLINK "${RUN}/artifacts/session/raw"
    OR EXISTS "${raw_target}")
    message(FATAL_ERROR "Unsafe private raw capture relocation")
endif()
file(RENAME "${RUN}/artifacts/session/raw" "${raw_target}")
schema_export_environment()
schema_export_classpath(after_receipt unused)
if(NOT before_receipt STREQUAL after_receipt)
    message(FATAL_ERROR "Production providers changed during capture")
endif()
foreach(name IN LISTS tool_files)
    file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/${name}" hash)
    if(NOT hash STREQUAL tool_before_${name})
        message(FATAL_ERROR "Oracle source changed during capture: ${name}")
    endif()
endforeach()
file(SHA256 "${CMAKE_CURRENT_LIST_DIR}/../../schema-export/Environment.cmake" environment_after)
if(NOT environment_hash STREQUAL environment_after)
    message(FATAL_ERROR "Shared launcher changed during capture")
endif()
file(WRITE "${RUN}/classpath-after.json" "${after_receipt}")
file(CONFIGURE OUTPUT "${RUN}/artifacts/provenance.tsv" CONTENT "${provenance}"
    @ONLY NEWLINE_STYLE UNIX)
file(GLOB_RECURSE artifact_files LIST_DIRECTORIES false RELATIVE "${RUN}/artifacts"
    "${RUN}/artifacts/*")
list(SORT artifact_files COMPARE STRING CASE SENSITIVE ORDER ASCENDING)
set(manifest "path\tbytes\tsha256\n")
foreach(name IN LISTS artifact_files)
    file(SIZE "${RUN}/artifacts/${name}" size)
    file(SHA256 "${RUN}/artifacts/${name}" hash)
    string(APPEND manifest "${name}\t${size}\t${hash}\n")
endforeach()
file(CONFIGURE OUTPUT "${RUN}/artifacts/manifest.tsv" CONTENT "${manifest}"
    @ONLY NEWLINE_STYLE UNIX)
set(compare_args -Actual "${RUN}/artifacts")
if(MODE STREQUAL Verify)
    list(APPEND compare_args -Expected "${EXPECTED}")
endif()
execute_process(COMMAND "${subscription_pwsh}" -NoProfile -File
    "${CMAKE_CURRENT_LIST_DIR}/Compare-Oracle.ps1" ${compare_args}
    TIMEOUT 30 RESULT_VARIABLE result
    OUTPUT_FILE "${RUN}/compare.stdout" ERROR_FILE "${RUN}/compare.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Strict artifact validation failed; see RUN/compare.stderr")
endif()
string(TIMESTAMP finished "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${RUN}/run.tsv" "mode\t${MODE}\nstartedUtc\t${started}\nfinishedUtc\t${finished}\nheapMiB\t256\nprocessTimeoutSeconds\t60\n")
message(STATUS "${MODE} PASS: unchanged production oracles; deterministic artifacts validated")
