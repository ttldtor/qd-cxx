# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

if(NOT ORG_TTLDTOR_QDCXX_BUILD_CLIENT OR NOT ORG_TTLDTOR_QDCXX_BUILD_RUNTIME)
    message(FATAL_ERROR "BUILD_RECOVERY requires explicit BUILD_CLIENT=ON and BUILD_RUNTIME=ON")
endif()

add_library(qdcxx_recovery src/transport/recovery_state.cpp src/transport/recovery_copy_profile.cpp
    src/transport/recovery_configuration.cpp src/transport/recovery_core.cpp
    src/transport/recovery_core_replay.cpp src/transport/tcp_recovery.cpp)
target_link_libraries(qdcxx_recovery PUBLIC qdcxx_transport qdcxx_client qdcxx_mux_queue)
target_include_directories(qdcxx_recovery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/transport")
set_target_properties(qdcxx_recovery PROPERTIES CXX_EXTENSIONS OFF)
install(TARGETS qdcxx_recovery EXPORT qdcxxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES include/qd/transport/recovery.hpp include/qd/transport/tcp_recovery.hpp
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd/transport")

if(ORG_TTLDTOR_QDCXX_BUILD_TLS)
    add_library(qdcxx_tls_recovery src/transport/tls_recovery.cpp)
    target_link_libraries(qdcxx_tls_recovery PUBLIC qdcxx_recovery qdcxx_tls)
    target_include_directories(qdcxx_tls_recovery PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/transport")
    set_target_properties(qdcxx_tls_recovery PROPERTIES CXX_EXTENSIONS OFF)
    install(TARGETS qdcxx_tls_recovery EXPORT qdcxxTargets ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES include/qd/transport/tls_recovery.hpp DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd/transport")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/RecoveryCopyProfile.cmake")
qdcxx_configure_recovery_copy_profile(qdcxx_recovery)
