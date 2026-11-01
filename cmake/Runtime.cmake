# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

# Included only for the explicit runtime option; OFF never discovers these SDKs or enables C.
set(ORG_TTLDTOR_QDCXX_ASIO_INCLUDE "" CACHE PATH "Explicit verified standalone Asio 1.38.2 include directory")
set(ORG_TTLDTOR_QDCXX_CARES_SOURCE "" CACHE PATH "Explicit verified c-ares 1.34.9+qdcxx2 source directory")
if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The runtime profile supports only Windows x64 and Linux x86-64")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The runtime profile requires a qualified 64-bit target")
endif()
if(WIN32 AND NOT MSVC)
    message(FATAL_ERROR "The qualified Windows runtime profile requires MSVC")
endif()
if(WIN32 AND NOT CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[xX]64$")
    message(FATAL_ERROR "The qualified Windows runtime profile requires an x64 compiler target")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    message(FATAL_ERROR "The qualified Linux runtime profile requires an x86-64 target")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "The qualified Linux runtime profile requires GCC")
endif()
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("\
#if defined(_WIN32)\n\
# if !defined(_M_X64)\n\
#  error Unqualified Windows target architecture\n\
# endif\n\
#elif !defined(__x86_64__)\n\
# error Unqualified Linux target architecture\n\
#endif\n\
int main() { return 0; }\n" ORG_TTLDTOR_QDCXX_RUNTIME_X64_TARGET)
if(NOT ORG_TTLDTOR_QDCXX_RUNTIME_X64_TARGET)
    message(FATAL_ERROR "The runtime compiler target must be Windows x64 or Linux x86-64")
endif()
if(NOT EXISTS "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}/asio/version.hpp"
    OR NOT EXISTS "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}/../LICENSE_1_0.txt")
    message(FATAL_ERROR "Provide the explicit verified Asio include directory with its adjacent license")
endif()
file(SHA256 "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}/asio/version.hpp" qdcxxAsioVersionHash)
if(NOT qdcxxAsioVersionHash STREQUAL "89d0c5c620e6626d3bee2f33522c7a9d54e00231325edb69f5c25ff65c54adb0")
    message(FATAL_ERROR "The runtime requires the pinned Asio 1.38.2 version header")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/VerifyRuntimeSdk.cmake")
qdcxxVerifyRuntimeSdk("${CMAKE_CURRENT_LIST_DIR}/runtime-profile/asio-1.38.2.sources.json"
    "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}/.." "include")
if(NOT EXISTS "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/include/ares_version.h"
    OR NOT EXISTS "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/src/lib/ares_init.c"
    OR NOT EXISTS "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/LICENSE.md")
    message(FATAL_ERROR "Provide the explicit verified qualified c-ares source directory and license")
endif()
qdcxxVerifyRuntimeSdk("${CMAKE_CURRENT_LIST_DIR}/runtime-profile/c-ares-1.34.9-qdcxx2.sources.json"
    "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}" "")

enable_language(C)
find_package(Threads REQUIRED)
set(CARES_STATIC ON CACHE BOOL "" FORCE)
set(CARES_SHARED OFF CACHE BOOL "" FORCE)
set(CARES_STATIC_PIC ON CACHE BOOL "" FORCE)
set(CARES_INSTALL OFF CACHE BOOL "" FORCE)
set(CARES_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(CARES_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CARES_THREADS OFF CACHE BOOL "" FORCE)
add_subdirectory("${ORG_TTLDTOR_QDCXX_CARES_SOURCE}" qdcxx-runtime-cares EXCLUDE_FROM_ALL)

add_library(qdcxx_transport src/transport/runtime_bootstrap.cpp src/transport/dns_resolver.cpp
    src/transport/runtime_core.cpp src/transport/tcp_runtime.cpp)
target_compile_definitions(qdcxx_transport PRIVATE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_include_directories(qdcxx_transport SYSTEM PRIVATE "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}")
target_link_libraries(qdcxx_transport PUBLIC qdcxx_protocol PRIVATE Threads::Threads
    "$<BUILD_INTERFACE:c-ares::cares>" "$<INSTALL_INTERFACE:qdcxx_runtime_cares>")
set_target_properties(qdcxx_transport PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
    target_compile_definitions(qdcxx_transport PRIVATE _WIN32_WINNT=0x0A00 WINVER=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(qdcxx_transport PRIVATE ws2_32 mswsock)
endif()

include(GNUInstallDirs)
get_target_property(qdcxxCaresTarget c-ares::cares ALIASED_TARGET)
get_target_property(qdcxxCaresNativeLibraries "${qdcxxCaresTarget}" INTERFACE_LINK_LIBRARIES)
if(NOT qdcxxCaresNativeLibraries)
    set(qdcxxCaresNativeLibraries "")
endif()
set(qdcxxCaresInstalledName "${CMAKE_STATIC_LIBRARY_PREFIX}qdcxx_runtime_cares${CMAKE_STATIC_LIBRARY_SUFFIX}")
install(TARGETS qdcxx_transport EXPORT qdcxxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES "$<TARGET_FILE:${qdcxxCaresTarget}>" DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RENAME "${qdcxxCaresInstalledName}")
install(DIRECTORY include/qd/transport DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "tls_runtime.hpp" EXCLUDE PATTERN "tls_resources.hpp" EXCLUDE)
install(FILES "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/LICENSE.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qdcxx/licenses" RENAME c-ares-MIT.txt)
install(FILES "${ORG_TTLDTOR_QDCXX_ASIO_INCLUDE}/../LICENSE_1_0.txt"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/qdcxx/licenses" RENAME Asio-BSL-1.0.txt)
