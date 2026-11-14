# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
include("${CMAKE_CURRENT_LIST_DIR}/../Environment.cmake")
schema_export_require(JAVA_HOME UPSTREAM CLASSPATH_RECEIPT RUN NATIVE)
schema_export_environment()
schema_export_classpath(receipt classpath)
schema_export_fresh_run()
file(WRITE "${RUN}/help.json" "[]\n")
execute_process(COMMAND "${CMAKE_COMMAND}" "-DJAVA_HOME=${JAVA_HOME}" "-DUPSTREAM=${UPSTREAM}"
    "-DCLASSPATH_RECEIPT=${CLASSPATH_RECEIPT}" "-DARGS=${RUN}/help.json" "-DRUN=${RUN}/compiled"
    -P "${CMAKE_CURRENT_LIST_DIR}/../Export.cmake"
    COMMAND_ERROR_IS_FATAL ANY)
set(cp "${RUN}/compiled/classes${schema_export_separator}${classpath}")
function(run_case name executable)
    if(executable STREQUAL schema_export_java)
        execute_process(COMMAND "${executable}" -Xmx256m -Dfile.encoding=UTF-8
            -Duser.language=en -Duser.country=US -Duser.timezone=UTC
            "-Duser.home=${RUN}/compiled/home" "-Djava.io.tmpdir=${RUN}/compiled/tmp"
            -cp "${cp}" ${ARGN}
            OUTPUT_FILE "${RUN}/${name}.stdout" ERROR_FILE "${RUN}/${name}.stderr"
            RESULT_VARIABLE result TIMEOUT 60)
    else()
        execute_process(COMMAND "${executable}" ${ARGN}
            OUTPUT_FILE "${RUN}/${name}.stdout" ERROR_FILE "${RUN}/${name}.stderr"
            RESULT_VARIABLE result TIMEOUT 60)
    endif()
    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "${name} failed (${result}); inspect ${RUN}/${name}.stderr")
    endif()
endfunction()
foreach(profile IN ITEMS default dxlink)
    if(profile STREQUAL "default")
        set(source --source default)
    else()
        set(source --source xml --resource dxlink)
    endif()
    run_case("export-${profile}" "${schema_export_java}"
        qdcxx.schemaexport.SchemaExport --provenance "${RUN}/compiled/provenance.json"
        --output "${RUN}/${profile}" ${source} --profile binary-v2)
    foreach(scenario IN ITEMS default boundary empty content)
        set(name "${profile}-${scenario}")
        run_case("${name}-java-write" "${schema_export_java}"
            qdcxx.schemaexport.tests.FullCatalogWire "${profile}" "${scenario}")
        run_case("${name}-native" "${NATIVE}" "${RUN}/${profile}/schema.json"
            "${RUN}/${name}-java-write.stdout" "${scenario}" "${RUN}/${name}-native.tsv")
        run_case("${name}-java-read" "${schema_export_java}"
            qdcxx.schemaexport.tests.FullCatalogWire "${profile}" "${scenario}" "${RUN}/${name}-native.tsv")
    endforeach()
endforeach()
message(STATUS "Full QD catalog wire comparison passed for default and dxlink")
