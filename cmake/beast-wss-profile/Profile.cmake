# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

# Internal callers are standalone dependency configure, installed consumers and offline provisioning.
# Paths in the profile resolve beside this file. Source verification performs no writes.
function(qdcxxReadBeastWssProfile metadata manifest)
    file(READ "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/boost-1.92.0.json" profile)
    string(JSON revision GET "${profile}" schemaVersion)
    string(JSON version GET "${profile}" version)
    string(JSON filename GET "${profile}" sourceManifest file)
    if(NOT revision EQUAL 1 OR NOT version STREQUAL "1.92.0" OR NOT filename STREQUAL "boost-1.92.0-sources.sha256.tsv")
        message(FATAL_ERROR "Unsupported Beast WSS source manifest")
    endif()
    set("${metadata}" "${profile}" PARENT_SCOPE)
    set("${manifest}" "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${filename}" PARENT_SCOPE)
endfunction()

function(qdcxxVerifyBeastWssSources sourceRoot)
    qdcxxReadBeastWssProfile(profile manifest)
    string(JSON hash GET "${profile}" sourceManifest sha256)
    string(JSON count GET "${profile}" sourceManifest regularFileCount)
    # Use the existing exact-file verifier for the Boost manifest as well as the OpenSSL manifest.
    qdcxxVerifyTlsFiles("${manifest}" "${hash}" "${count}" "${sourceRoot}")
endfunction()

# Inputs are a compiled project and its explicit, separately provisioned OpenSSL root.
# Imports the same qualified targets as the QTP TLS profile. A conflicting prior import fails.
function(qdcxxConfigureBeastWssTls tlsProfile)
    if(CMAKE_CONFIGURATION_TYPES)
        message(FATAL_ERROR "Beast WSS requires a single-configuration generator")
    endif()
    if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
        set(platform windows-x64)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(platform linux-x64)
        if(NOT CMAKE_CXX_FLAGS MATCHES "(^| )-fsanitize=address,undefined( |$)")
            message(FATAL_ERROR "Beast WSS Linux profile requires -fsanitize=address,undefined")
        endif()
    else()
        message(FATAL_ERROR "Beast WSS requires Windows Release or Linux Debug")
    endif()
    if(NOT ORG_TTLDTOR_QDCXX_OPENSSL_ROOT OR NOT IS_DIRECTORY "${ORG_TTLDTOR_QDCXX_OPENSSL_ROOT}")
        message(FATAL_ERROR "Beast WSS requires explicit ORG_TTLDTOR_QDCXX_OPENSSL_ROOT")
    endif()
    qdcxxCheckTlsHost("${platform}")
    qdcxxVerifyTlsProfile("${tlsProfile}" "${platform}" "${ORG_TTLDTOR_QDCXX_OPENSSL_ROOT}")
    find_package(Threads REQUIRED)
    qdcxxImportTlsSdk("${ORG_TTLDTOR_QDCXX_OPENSSL_ROOT}" "${platform}")
endfunction()
