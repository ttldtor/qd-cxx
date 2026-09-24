# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.25)
include("${CMAKE_CURRENT_LIST_DIR}/Environment.cmake")
schema_export_arguments(JAVA_HOME UPSTREAM MAVEN CACHE RUN)
schema_export_require(JAVA_HOME UPSTREAM MAVEN CACHE RUN)
schema_export_environment()
foreach(key MAVEN_OPTS MAVEN_ARGS MAVEN_CONFIG)
    if(DEFINED ENV{${key}} AND NOT "$ENV{${key}}" STREQUAL "")
        message(FATAL_ERROR "Unset ${key} for controlled upstream preparation")
    endif()
endforeach()
if(NOT EXISTS "${MAVEN}" OR IS_DIRECTORY "${MAVEN}")
    message(FATAL_ERROR "MAVEN must identify an explicit executable")
endif()
schema_export_fresh_run()
file(MAKE_DIRECTORY "${RUN}/build-work" "${RUN}/home" "${CACHE}")
# Both user/global settings and startup scripts are disabled; the locked reactor owns plugins.
file(WRITE "${RUN}/settings.xml" "<settings xmlns=\"http://maven.apache.org/SETTINGS/1.0.0\"/>\n")
set(ENV{JAVA_HOME} "${JAVA_HOME}")
set(ENV{MAVEN_SKIP_RC} "1")
set(ENV{MAVEN_OPTS} "-Duser.timezone=UTC -Duser.home=\"${RUN}/home\"")
set(ENV{TZ} "UTC")
execute_process(COMMAND "${MAVEN}" -B -f "${UPSTREAM}/pom.xml" -pl qds-file -am
    -s "${RUN}/settings.xml" -gs "${RUN}/settings.xml" "-Dmaven.repo.local=${CACHE}"
    -Dmaven.build.cache.enabled=false -Dmaven.test.skip=true clean compile
    WORKING_DIRECTORY "${RUN}/build-work" TIMEOUT 600 RESULT_VARIABLE result
    OUTPUT_FILE "${RUN}/build.stdout" ERROR_FILE "${RUN}/build.stderr")
if(NOT "${result}" STREQUAL "0")
    message(FATAL_ERROR "Locked reactor build failed (${result}); see RUN/build.stdout and RUN/build.stderr")
endif()
schema_export_environment()
schema_export_classpath(receipt classpath)
file(WRITE "${RUN}/classpath-receipt.json" "${receipt}")
message(STATUS "Prepared verified classpath receipt: ${RUN}/classpath-receipt.json")
