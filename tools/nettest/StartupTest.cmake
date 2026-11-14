# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED PROGRAM)
    message(FATAL_ERROR "PROGRAM is required")
endif()

if(NOT DEFINED RUN)
    message(FATAL_ERROR "RUN is required")
endif()
set(bundle "${RUN}/nettest-help-bundle")
file(MAKE_DIRECTORY "${bundle}")
file(WRITE "${bundle}/schema.json" [=[
{"formatVersion":1,"profile":{"id":"qdcxx-native-binary","version":2},"records":[
   {"id":0,"name":"Selected","hasTime":false,"fields":[{"serialTypeId":8}],
    "lookups":[{"name":"Value","fieldId":0}],"indexedTimeLayout":null}
 ]}
]=])
file(WRITE "${bundle}/provenance.json" [=[
{"format":"qdcxx-java-export-v2","projection":[{"sourceId":0,"exportedId":0,
"slots":[{"fieldId":0,"names":{"propertyName":"Value"}}]}]}
]=])
execute_process(COMMAND "${PROGRAM}" p --bundle "${bundle}" --port 0 --record Selected --startup 1
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 4)
if(NOT "${code}" STREQUAL "1" OR NOT out MATCHES "^PORT [0-9]+" OR NOT err MATCHES "matching demand")
    message(FATAL_ERROR "No-demand producer did not complete its startup deadline: ${code}; ${out}; ${err}")
endif()

execute_process(COMMAND "${PROGRAM}" c --bundle "${bundle}" --port 1 --record Selected --contract history
    RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 4)
if(NOT "${code}" STREQUAL "2" OR NOT out STREQUAL "" OR NOT err MATCHES "HISTORY requires an indexed record")
    message(FATAL_ERROR "HISTORY accepted a record without indexed positions: ${code}; ${out}; ${err}")
endif()
