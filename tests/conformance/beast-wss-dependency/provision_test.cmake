# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED PROVISION OR NOT DEFINED DESTINATION)
    message(FATAL_ERROR "Provide the provisioning script and an unused destination")
endif()

function(expectResult expected fragment)
    execute_process(COMMAND "${CMAKE_COMMAND}" ${ARGN} -P "${PROVISION}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error ENCODING UTF-8 TIMEOUT 5)
    if(NOT result STREQUAL expected OR NOT "${output}${error}" MATCHES "${fragment}")
        message(FATAL_ERROR "Unexpected provisioning result ${result}: ${output}${error}")
    endif()
endfunction()

expectResult("0" "Usage:" -DHELP=ON)
expectResult("0" "Запуск:" -DHELP=ON -DLANGUAGE=ru)
expectResult("0" "^$" -DENABLED=OFF "-DDESTINATION=${DESTINATION}")
expectResult("1" "Supply DESTINATION")
foreach(path IN ITEMS "${DESTINATION}" "${DESTINATION}.partial" "${DESTINATION}.lock")
    if(EXISTS "${path}" OR IS_SYMLINK "${path}")
        message(FATAL_ERROR "Offline help or OFF wrote to ${path}")
    endif()
endforeach()
