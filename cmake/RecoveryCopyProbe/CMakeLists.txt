# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

# Internal try_compile project called by RecoveryCopyProfile.cmake. Inputs are the
# generated source, candidate profile, dependency output and admitted environment file. Produces
# one static library and compiler include provenance; compile failure is an unsupported profile.
cmake_minimum_required(VERSION 3.25)
project(qdcxx_recovery_copy_probe LANGUAGES CXX)
include("${QDCXX_PROFILE_ENVIRONMENT}")
add_library(copy_probe STATIC "${QDCXX_PROFILE_SOURCE}")
target_compile_features(copy_probe PRIVATE cxx_std_23)
set_target_properties(copy_probe PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(copy_probe PRIVATE ${QDCXX_PROFILE_INCLUDES})
target_compile_definitions(copy_probe PRIVATE "QDCXX_PROFILE_KIND=${QDCXX_PROFILE_KIND}" ${QDCXX_PROFILE_DEFINITIONS})
if(MSVC)
    target_compile_options(copy_probe PRIVATE /sourceDependencies "${QDCXX_PROFILE_DEPENDENCIES}")
else()
    target_compile_options(copy_probe PRIVATE -H)
endif()
