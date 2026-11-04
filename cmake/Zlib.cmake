# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
include_guard(GLOBAL)
set(ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE "" CACHE FILEPATH "Optional exact zlib 1.3.2 archive, checked only when needed")
# A source-directory override would bypass archive verification; accept only an exact archive.
if(FETCHCONTENT_SOURCE_DIR_QDCXX_ZLIB)
    message(FATAL_ERROR "Use the hash-verified archive override, not an arbitrary source tree")
endif()
set(qdcxxZlibArchive "https://zlib.net/zlib-1.3.2.tar.gz")
if(ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE)
    file(SHA256 "${ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE}" qdcxxZlibHash)
    if(NOT qdcxxZlibHash STREQUAL "bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16")
        message(FATAL_ERROR "zlib archive hash mismatch")
    endif()
    set(qdcxxZlibArchive "${ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE}")
endif()
# Keep the default/unused optional profile CXX-only and independent of compression acquisition.
enable_language(C)
include(FetchContent)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(qdcxx_zlib URL "${qdcxxZlibArchive}"
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(qdcxx_zlib)
file(SHA256 "${qdcxx_zlib_SOURCE_DIR}/LICENSE" qdcxxZlibLicenseHash)
file(STRINGS "${qdcxx_zlib_SOURCE_DIR}/zlib.h" qdcxxZlibVersion REGEX "^#define ZLIB_VERSION ")
if(NOT qdcxxZlibLicenseHash STREQUAL "e32ff4e00d9d94930537635291da39e7e612703334bf6fde8c7f1686fe8a45a2" OR
   NOT qdcxxZlibVersion STREQUAL "#define ZLIB_VERSION \"1.3.2\"")
    message(FATAL_ERROR "zlib version/license differs from the accepted pin")
endif()
