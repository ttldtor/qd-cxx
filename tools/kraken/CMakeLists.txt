# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

if(NOT TARGET qdcxx_kraken_source OR NOT TARGET qdcxx_feed OR ORG_TTLDTOR_QDCXX_BUILD_TLS)
    message(FATAL_ERROR "Kraken gateway requires the Kraken source and plain Feed profiles; disable QTP TLS")
endif()

add_executable(qdcxx_kraken_gateway gateway.cpp)
set_target_properties(qdcxx_kraken_gateway PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-kraken-gateway)
target_link_libraries(qdcxx_kraken_gateway PRIVATE qdcxx_kraken_source qdcxx_feed)
target_compile_options(qdcxx_kraken_gateway PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(BUILD_TESTING)
    add_test(NAME kraken_gateway_cli COMMAND "${CMAKE_COMMAND}"
        "-DPROGRAM=$<TARGET_FILE:qdcxx_kraken_gateway>"
        "-DTRUST=${CMAKE_CURRENT_SOURCE_DIR}/../../tests/fixtures/transport/tls/ca.pem"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check.cmake")
    set_tests_properties(kraken_gateway_cli PROPERTIES TIMEOUT 20)
endif()
