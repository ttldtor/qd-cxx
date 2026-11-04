# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
if(NOT ORG_TTLDTOR_QDCXX_BUILD_IPF OR NOT ORG_TTLDTOR_QDCXX_BUILD_TLS)
    message(FATAL_ERROR "BUILD_IPF_TRANSFER requires explicit BUILD_IPF=ON and BUILD_TLS=ON with its prerequisites")
endif()
set(ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK "" CACHE PATH "Explicit sealed static IPF transfer SDK")
if(NOT IS_ABSOLUTE "${ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK}" OR
   NOT EXISTS "${ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK}/lib/cmake/qdcxx_ipf_transfer_sdk/qdcxx_ipf_transfer_sdkConfig.cmake")
    message(FATAL_ERROR "Provide ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK: explicit qualified sealed SDK; no download/discovery fallback")
endif()
find_package(qdcxx_ipf_transfer_sdk CONFIG REQUIRED NO_DEFAULT_PATH
    PATHS "${ORG_TTLDTOR_QDCXX_IPF_TRANSFER_SDK}/lib/cmake/qdcxx_ipf_transfer_sdk")
add_library(qdcxx_ipf_transfer src/ipf/transfer_source.cpp src/ipf/transfer_response.cpp src/ipf/url_reader.cpp)
target_link_libraries(qdcxx_ipf_transfer PUBLIC qdcxx_ipf qdcxx_tls PRIVATE qdcxx_private::url qdcxx_tls_ssl qdcxx_tls_crypto
    "$<BUILD_INTERFACE:c-ares::cares>")
target_include_directories(qdcxx_ipf_transfer SYSTEM PRIVATE "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}")
target_compile_definitions(qdcxx_ipf_transfer PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
set_target_properties(qdcxx_ipf_transfer PROPERTIES CXX_EXTENSIONS OFF)
install(TARGETS qdcxx_ipf_transfer EXPORT qdcxxTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES include/qd/ipf/instrument_profile_url_reader.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd/ipf")
