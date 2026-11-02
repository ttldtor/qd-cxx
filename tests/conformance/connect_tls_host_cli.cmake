cmake_minimum_required(VERSION 3.25)

# Internal CTest helper: PROGRAM is the closed-host runner. Usage must refuse before SDK work.
foreach(args "" "--case;DO_NOT_ECHO_ARGUMENT")
    execute_process(COMMAND "${PROGRAM}" ${args}
        RESULT_VARIABLE code OUTPUT_VARIABLE out ERROR_VARIABLE err TIMEOUT 10)
    if(NOT code EQUAL 2 OR NOT err MATCHES "Invalid invocation" OR out MATCHES "RUN"
        OR "${out}${err}" MATCHES "DO_NOT_ECHO_ARGUMENT")
        message(FATAL_ERROR "Expected redacted usage refusal with exit 2")
    endif()
endforeach()
