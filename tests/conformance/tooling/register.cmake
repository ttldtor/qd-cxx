set(ORG_TTLDTOR_QDCXX_UNCRUSTIFY "" CACHE FILEPATH "Exact native Uncrustify for optional real-tool tests")
set(ORG_TTLDTOR_QDCXX_CLANG_FORMAT "" CACHE FILEPATH "Exact native clang-format for optional real-tool tests")
if(WIN32)
    set(logger_acp_target qdcxx_run_utc_acp1252)
else()
    set(logger_acp_target qdcxx_run_utc)
endif()
add_test(NAME tooling_logger_contract COMMAND "${CMAKE_COMMAND}"
    "-DQDCXX_LOGGER=$<TARGET_FILE:qdcxx_run_utc>" "-DQDCXX_PROBE=$<TARGET_FILE:qdcxx_process_probe>"
    "-DQDCXX_LOGGER_TESTS=$<TARGET_FILE:qdcxx_logger_tests>" "-DWORK=${PROJECT_BINARY_DIR}/tooling-logger"
    "-DACP_LOGGER=$<TARGET_FILE:${logger_acp_target}>"
    "-DMUTANT_ARGS=$<TARGET_FILE:qdcxx_run_utc_mutant_args>"
    "-DMUTANT_DECODING=$<TARGET_FILE:qdcxx_run_utc_mutant_decoding>"
    "-DMUTANT_EXIT=$<TARGET_FILE:qdcxx_run_utc_mutant_exit>"
    "-DFAULT_LOGGER=$<TARGET_FILE:qdcxx_run_utc_fault>"
    -P "${CMAKE_CURRENT_LIST_DIR}/logger.cmake")
add_test(NAME tooling_format_contract COMMAND "${CMAKE_COMMAND}"
    "-DDRIVER=${PROJECT_SOURCE_DIR}/tools/format/Format-Cpp.cmake" "-DPROBE=$<TARGET_FILE:qdcxx_format_probe>"
    "-DROOT=${PROJECT_SOURCE_DIR}" "-DRUN_DIR=${PROJECT_BINARY_DIR}/tooling-format"
    "-DREAL_UNCRUSTIFY=${ORG_TTLDTOR_QDCXX_UNCRUSTIFY}" "-DREAL_CLANG_FORMAT=${ORG_TTLDTOR_QDCXX_CLANG_FORMAT}"
    -P "${CMAKE_CURRENT_LIST_DIR}/format.cmake")
add_test(NAME tooling_coverage_contract COMMAND "${CMAKE_COMMAND}"
    "-DRUN=${PROJECT_BINARY_DIR}/tooling-coverage" -P "${CMAKE_CURRENT_LIST_DIR}/coverage.cmake")
add_test(NAME tooling_audit_contract COMMAND "${CMAKE_COMMAND}"
    "-DRUN=${PROJECT_BINARY_DIR}/tooling-audit" -P "${CMAKE_CURRENT_LIST_DIR}/audit_tests.cmake")
set_tests_properties(tooling_logger_contract tooling_format_contract tooling_coverage_contract tooling_audit_contract
    PROPERTIES TIMEOUT 600)
