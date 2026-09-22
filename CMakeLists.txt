# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.25)
project(qdcxx LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, for example -B .work/build")
endif()

add_library(qdcxx_encoding src/encoding/compact.cpp src/encoding/utf_string.cpp src/encoding/symbol_reader.cpp)
target_include_directories(qdcxx_encoding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qdcxx_encoding PUBLIC cxx_std_23)
set_target_properties(qdcxx_encoding PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_core src/core/local_schema.cpp)
target_include_directories(qdcxx_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qdcxx_core PUBLIC cxx_std_23)
set_target_properties(qdcxx_core PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_protocol src/protocol/frame_reader.cpp src/protocol/describe_records.cpp src/protocol/static_binding.cpp src/protocol/stream_data.cpp)
target_link_libraries(qdcxx_protocol PUBLIC qdcxx_encoding qdcxx_core)
set_target_properties(qdcxx_protocol PROPERTIES CXX_EXTENSIONS OFF)

option(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT "Build the bounded offline QTP diagnostic CLI" OFF)
if(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT)
    add_library(qdcxx_inspect_tool STATIC tools/qtp-inspect/inspect.cpp tools/qtp-inspect/formatter.cpp)
    target_link_libraries(qdcxx_inspect_tool PUBLIC qdcxx_protocol)
    target_include_directories(qdcxx_inspect_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/qtp-inspect")
    set_target_properties(qdcxx_inspect_tool PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(qdcxx_qtp_inspect tools/qtp-inspect/main.cpp)
    target_link_libraries(qdcxx_qtp_inspect PRIVATE qdcxx_inspect_tool)
    set_target_properties(qdcxx_qtp_inspect PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "qdcxx-qtp-inspect")
endif()

include(CTest)
if(BUILD_TESTING)
    include(FetchContent)
    # Test-only dependency, pinned to the v2.4.12 release archive and its SHA-256.
    FetchContent_Declare(doctest
        URL https://codeload.github.com/doctest/doctest/tar.gz/refs/tags/v2.4.12
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(doctest)
    add_executable(qdcxx_compact_tests tests/conformance/compact_tests.cpp)
    target_link_libraries(qdcxx_compact_tests PRIVATE qdcxx_encoding doctest::doctest)
    set_target_properties(qdcxx_compact_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/expected"
        CACHE PATH "Java TSV directory (override only for temporary negative checks)")
    add_test(NAME compact_java_vectors COMMAND qdcxx_compact_tests "${ORG_TTLDTOR_QDCXX_ENCODING_FIXTURES}")
    add_executable(qdcxx_frame_tests tests/conformance/frame_tests.cpp)
    target_link_libraries(qdcxx_frame_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_frame_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/expected"
        CACHE PATH "Java protocol TSV directory (temporary overrides for negative checks)")
    set(ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/protocol/frames.tsv"
        CACHE FILEPATH "Reviewed frame-only expectations derived from Java inputs")
    add_test(NAME protocol_java_frames COMMAND qdcxx_frame_tests "${ORG_TTLDTOR_QDCXX_PROTOCOL_FIXTURES}" "${ORG_TTLDTOR_QDCXX_FRAME_PROJECTIONS}")
    add_executable(qdcxx_description_tests tests/conformance/description_tests.cpp)
    target_link_libraries(qdcxx_description_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_description_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/p08/expected/descriptions.tsv"
        CACHE FILEPATH "Java description observations (temporary override for negative checks)")
    add_test(NAME protocol_java_descriptions COMMAND qdcxx_description_tests "${ORG_TTLDTOR_QDCXX_DESCRIPTION_FIXTURES}")
    add_executable(qdcxx_binding_tests tests/conformance/binding_tests.cpp)
    target_link_libraries(qdcxx_binding_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_binding_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_BINDING_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/protocol/binding/plans.tsv"
        CACHE FILEPATH "Reviewed static binding projections (temporary override for negative checks)")
    add_test(NAME protocol_java_binding COMMAND qdcxx_binding_tests "${ORG_TTLDTOR_QDCXX_BINDING_FIXTURES}")
    add_executable(qdcxx_symbol_tests tests/conformance/symbol_tests.cpp)
    target_link_libraries(qdcxx_symbol_tests PRIVATE qdcxx_encoding doctest::doctest)
    set_target_properties(qdcxx_symbol_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/symbols/expected/symbols.tsv"
        CACHE FILEPATH "Java symbol observations (temporary override for negative checks)")
    add_test(NAME encoding_java_symbols COMMAND qdcxx_symbol_tests "${ORG_TTLDTOR_QDCXX_SYMBOL_FIXTURES}")
    add_executable(qdcxx_stream_data_tests tests/conformance/stream_data_tests.cpp)
    target_link_libraries(qdcxx_stream_data_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_stream_data_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/stream-data/expected/stream-data.tsv"
        CACHE FILEPATH "Java STREAM_DATA observations (temporary override for negative checks)")
    add_test(NAME protocol_java_stream_data COMMAND qdcxx_stream_data_tests "${ORG_TTLDTOR_QDCXX_STREAM_DATA_FIXTURES}")
    if(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME offline_inspection_cli COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/check_offline_inspection.py"
            "$<TARGET_FILE:qdcxx_qtp_inspect>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection")
        add_executable(qdcxx_offline_io_tests tests/conformance/offline_io_tests.cpp)
        target_link_libraries(qdcxx_offline_io_tests PRIVATE qdcxx_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_offline_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME offline_inspection_injected_io COMMAND qdcxx_offline_io_tests)
        add_test(NAME human_inspection_cli COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/check_human_inspection.py"
            "$<TARGET_FILE:qdcxx_qtp_inspect>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/human-inspection")
        add_executable(qdcxx_human_io_tests tests/conformance/human_io_tests.cpp)
        target_link_libraries(qdcxx_human_io_tests PRIVATE qdcxx_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_human_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME human_inspection_injected_io COMMAND qdcxx_human_io_tests)
    endif()
endif()
