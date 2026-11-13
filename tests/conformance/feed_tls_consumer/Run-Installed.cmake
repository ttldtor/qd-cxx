# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
# Runs the already-built independent Feed client against the existing owned TLS fixture peer.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED CONSUMER OR NOT IS_ABSOLUTE "${CONSUMER}" OR NOT EXISTS "${CONSUMER}")
    message(FATAL_ERROR "Set CONSUMER to the absolute path of the installed Feed test client executable.")
endif()

if(NOT DEFINED PEER)
    execute_process(COMMAND "${CONSUMER}" unavailable TIMEOUT 15 RESULT_VARIABLE result)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "TLS-OFF Feed consumer failed: ${result}")
    endif()
    message(STATUS "TLS-OFF Feed consumer passed")
    return()
endif()

if(NOT IS_ABSOLUTE "${PEER}" OR NOT EXISTS "${PEER}" OR
   NOT DEFINED FIXTURES OR NOT IS_ABSOLUTE "${FIXTURES}" OR
   NOT EXISTS "${FIXTURES}/ca.pem" OR NOT EXISTS "${FIXTURES}/other-ca.pem")
    message(FATAL_ERROR "Set PEER and FIXTURES to the absolute TLS fixture driver and certificate directory.")
endif()

foreach(mode authenticated wrong-ca wrong-san)
    set(trust "${FIXTURES}/ca.pem")
    if(mode STREQUAL "wrong-ca")
        set(trust "${FIXTURES}/other-ca.pem")
    endif()
    execute_process(
        COMMAND "${PEER}" --installed-peer "${mode}"
        COMMAND "${CONSUMER}" "${mode}" "${trust}"
        TIMEOUT 28
        RESULTS_VARIABLE results
    )
    if(NOT results STREQUAL "0;0")
        message(FATAL_ERROR "Installed Feed TLS ${mode} failed: peer and consumer status ${results}")
    endif()
    message(STATUS "Installed Feed TLS ${mode} passed")
endforeach()
