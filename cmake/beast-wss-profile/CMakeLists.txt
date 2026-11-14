# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
cmake_minimum_required(VERSION 3.25)
project(qdcxx_beast_wss_sdk LANGUAGES NONE)

option(ORG_TTLDTOR_QDCXX_BEAST_WSS_DEPENDENCY "Build the opt-in private Beast WSS dependency package" OFF)
if(NOT ORG_TTLDTOR_QDCXX_BEAST_WSS_DEPENDENCY)
    message(STATUS "Beast WSS dependency OFF: no compiler, Boost or TLS SDK discovery")
    return()
endif()

enable_language(CXX)
set(ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE "" CACHE PATH "Explicit provisioned pinned Boost headers")
set(ORG_TTLDTOR_QDCXX_OPENSSL_ROOT "" CACHE PATH "Explicit qualified static OpenSSL installation")
if(NOT ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE OR NOT IS_DIRECTORY "${ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE}")
    message(FATAL_ERROR "Provide explicit ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE; see tools/deps/BEAST-WSS.md")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../VerifyTlsSdk.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/Profile.cmake")
qdcxxVerifyBeastWssSources("${ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE}")
qdcxxConfigureBeastWssTls("${CMAKE_CURRENT_LIST_DIR}/../tls-profile/openssl-3.5.9.json")

add_library(qdcxx_beast_wss INTERFACE)
set_target_properties(qdcxx_beast_wss PROPERTIES EXPORT_NAME beast_wss)
target_compile_features(qdcxx_beast_wss INTERFACE cxx_std_23)
target_compile_definitions(qdcxx_beast_wss INTERFACE BOOST_ALL_NO_LIB=1 BOOST_BEAST_HEADER_ONLY=1)
target_include_directories(qdcxx_beast_wss SYSTEM INTERFACE
    "$<BUILD_INTERFACE:${ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE}>" "$<INSTALL_INTERFACE:source>")
target_link_libraries(qdcxx_beast_wss INTERFACE qdcxx_tls_ssl Threads::Threads)
if(WIN32)
    target_compile_definitions(qdcxx_beast_wss INTERFACE _WIN32_WINNT=0x0A00 WINVER=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(qdcxx_beast_wss INTERFACE ws2_32 mswsock)
endif()

add_executable(qdcxx_beast_wss_probe "../../tests/conformance/beast-wss-dependency/probe.cpp")
set_target_properties(qdcxx_beast_wss_probe PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(qdcxx_beast_wss_probe PRIVATE qdcxx_beast_wss)
enable_testing()
add_test(NAME beast_wss_provision_cli COMMAND "${CMAKE_COMMAND}"
    "-DPROVISION=${CMAKE_CURRENT_LIST_DIR}/../../tools/deps/ProvisionBeastWss.cmake"
    "-DDESTINATION=${CMAKE_CURRENT_BINARY_DIR}/provision-cli-unused"
    -P "${CMAKE_CURRENT_LIST_DIR}/../../tests/conformance/beast-wss-dependency/provision_test.cmake")
set_tests_properties(beast_wss_provision_cli PROPERTIES TIMEOUT 25)

configure_file(SdkConfig.cmake.in qdcxx_beast_wss_sdkConfig.cmake @ONLY)

set(cmakeDestination lib/cmake/qdcxx_beast_wss_sdk)
install(TARGETS qdcxx_beast_wss EXPORT qdcxxBeastWssTargets)
install(EXPORT qdcxxBeastWssTargets NAMESPACE qdcxx_private:: DESTINATION "${cmakeDestination}")
install(DIRECTORY "${ORG_TTLDTOR_QDCXX_BEAST_WSS_SOURCE}/" DESTINATION source)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qdcxx_beast_wss_sdkConfig.cmake" "Profile.cmake"
    "boost-1.92.0.json" "boost-1.92.0-sources.sha256.tsv" "../VerifyTlsSdk.cmake"
    DESTINATION "${cmakeDestination}")
install(DIRECTORY ../tls-profile DESTINATION "${cmakeDestination}")
