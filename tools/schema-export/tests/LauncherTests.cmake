# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.25)
include("${CMAKE_CURRENT_LIST_DIR}/../Environment.cmake")
schema_export_arguments(JAVA_HOME UPSTREAM CLASSPATH_RECEIPT RUN)
schema_export_require(JAVA_HOME UPSTREAM CLASSPATH_RECEIPT RUN)
schema_export_environment()
schema_export_fresh_run()
set(launcher "${schema_export_directory}/Export.cmake")
file(WRITE "${RUN}/empty.json" "[]\n")

function(expect_failure name diagnostic)
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJAVA_HOME=${JAVA_HOME}" "-DUPSTREAM=${UPSTREAM}"
        "-DCLASSPATH_RECEIPT=${CLASSPATH_RECEIPT}" "-DARGS=${RUN}/empty.json" "-DRUN=${RUN}/${name}"
        ${ARGN} -P "${launcher}" TIMEOUT 40 RESULT_VARIABLE result
        OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if("${result}" STREQUAL "0" OR NOT error MATCHES "${diagnostic}")
        message(FATAL_ERROR "Launcher case ${name} did not reject as expected: ${result}\n${output}\n${error}")
    endif()
    if(EXISTS "${RUN}/${name}")
        message(FATAL_ERROR "Launcher case ${name} performed work before boundary rejection")
    endif()
    message(STATUS "Launcher rejection passed: ${name}")
endfunction()

expect_failure(unknown "Unknown launcher setting" -DUNRECOGNIZED=true)
expect_failure(missing "Required launcher setting" -DARGS=)
foreach(key JAVA_TOOL_OPTIONS _JAVA_OPTIONS JDK_JAVA_OPTIONS)
    set(ENV{${key}} "-Dqdcxx.unexpected=true")
    expect_failure("environment-${key}" "Unset ${key}")
    unset(ENV{${key}})
endforeach()
expect_failure(upstream "Cannot verify locked upstream checkout" "-DUPSTREAM=${RUN}/missing-checkout")
expect_failure(jdk "Explicit JAVA_HOME must provide JDK 8" "-DJAVA_HOME=${RUN}/missing-jdk")
file(WRITE "${RUN}/bad-receipt.json" "{}\n")
expect_failure(receipt "Classpath receipt mismatch" "-DCLASSPATH_RECEIPT=${RUN}/bad-receipt.json")
file(WRITE "${RUN}/not-array.json" "{}\n")
expect_failure(not-array "ARGS must contain a JSON array" "-DARGS=${RUN}/not-array.json")
file(WRITE "${RUN}/not-string.json" "[1]\n")
expect_failure(not-string "ARGS must contain only strings" "-DARGS=${RUN}/not-string.json")
file(WRITE "${RUN}/reserved.json" "[\"--provenance\",\"elsewhere\"]\n")
expect_failure(reserved "--provenance is reserved" "-DARGS=${RUN}/reserved.json")
# An existing scratch directory must remain intact, just like an existing export destination.
file(MAKE_DIRECTORY "${RUN}/existing")
file(WRITE "${RUN}/existing/sentinel" "unchanged\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DJAVA_HOME=${JAVA_HOME}" "-DUPSTREAM=${UPSTREAM}"
    "-DCLASSPATH_RECEIPT=${CLASSPATH_RECEIPT}" "-DARGS=${RUN}/empty.json" "-DRUN=${RUN}/existing"
    -P "${launcher}" TIMEOUT 40 RESULT_VARIABLE result ERROR_VARIABLE error OUTPUT_VARIABLE output)
file(READ "${RUN}/existing/sentinel" sentinel)
if("${result}" STREQUAL "0" OR NOT error MATCHES "RUN must be a new scratch directory"
        OR NOT sentinel STREQUAL "unchanged\n")
    message(FATAL_ERROR "Existing scratch directory was not preserved")
endif()
function(run_command name json)
    file(WRITE "${RUN}/${name}.json" "${json}\n")
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DJAVA_HOME=${JAVA_HOME}" "-DUPSTREAM=${UPSTREAM}"
        "-DCLASSPATH_RECEIPT=${CLASSPATH_RECEIPT}" "-DARGS=${RUN}/${name}.json" "-DRUN=${RUN}/${name}"
        -P "${launcher}" TIMEOUT 100 RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    set(${name}_result "${result}" PARENT_SCOPE)
    set(${name}_output "${output}" PARENT_SCOPE)
    set(${name}_error "${error}" PARENT_SCOPE)
endfunction()

run_command(no_arguments "[]")
run_command(help "[\"--help\"]")
if(NOT "${no_arguments_result}" STREQUAL "0" OR NOT "${help_result}" STREQUAL "0"
        OR NOT no_arguments_output STREQUAL help_output OR help_output STREQUAL ""
        OR NOT no_arguments_error STREQUAL "" OR NOT help_error STREQUAL "")
    message(FATAL_ERROR "No arguments and standalone help must produce identical stdout and empty stderr")
endif()
run_command(empty_argument "[\"\"]")
if("${empty_argument_result}" STREQUAL "0" OR NOT empty_argument_error MATCHES "schema-export-error.*USAGE")
    message(FATAL_ERROR "The launcher lost an empty argument instead of preserving its usage error")
endif()
message(STATUS "Launcher tests passed (12 boundary rejections, help equivalence, empty argument preservation)")
