# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.25)
project(qd_cxx LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, for example -B .work/build")
endif()

add_library(qd_encoding src/encoding/compact.cpp src/encoding/utf_string.cpp src/encoding/symbol_reader.cpp)
target_include_directories(qd_encoding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qd_encoding PUBLIC cxx_std_23)
set_target_properties(qd_encoding PROPERTIES CXX_EXTENSIONS OFF)

add_library(qd_core src/core/local_schema.cpp)
target_include_directories(qd_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qd_core PUBLIC cxx_std_23)
set_target_properties(qd_core PROPERTIES CXX_EXTENSIONS OFF)

add_library(qd_protocol src/protocol/frame_reader.cpp src/protocol/describe_records.cpp src/protocol/static_binding.cpp src/protocol/stream_data.cpp)
target_link_libraries(qd_protocol PUBLIC qd_encoding qd_core)
set_target_properties(qd_protocol PROPERTIES CXX_EXTENSIONS OFF)

option(QD_BUILD_QTP_INSPECT "Build the bounded offline QTP diagnostic CLI" OFF)
if(QD_BUILD_QTP_INSPECT)
    add_library(qd_inspect_tool STATIC tools/qtp-inspect/inspect.cpp)
    target_link_libraries(qd_inspect_tool PUBLIC qd_protocol)
    target_include_directories(qd_inspect_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/qtp-inspect")
    set_target_properties(qd_inspect_tool PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(qd-qtp-inspect tools/qtp-inspect/main.cpp)
    target_link_libraries(qd-qtp-inspect PRIVATE qd_inspect_tool)
    set_target_properties(qd-qtp-inspect PROPERTIES CXX_EXTENSIONS OFF)
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
    add_executable(compact_tests tests/conformance/compact_tests.cpp)
    target_link_libraries(compact_tests PRIVATE qd_encoding doctest::doctest)
    set_target_properties(compact_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_ENCODING_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/expected"
        CACHE PATH "Java TSV directory (override only for temporary negative checks)")
    add_test(NAME compact_java_vectors COMMAND compact_tests "${QD_ENCODING_FIXTURES}")
    add_executable(frame_tests tests/conformance/frame_tests.cpp)
    target_link_libraries(frame_tests PRIVATE qd_protocol doctest::doctest)
    set_target_properties(frame_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_PROTOCOL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/expected"
        CACHE PATH "Java protocol TSV directory (temporary overrides for negative checks)")
    set(QD_FRAME_PROJECTIONS "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/protocol/frames.tsv"
        CACHE FILEPATH "Reviewed frame-only expectations derived from Java inputs")
    add_test(NAME protocol_java_frames COMMAND frame_tests "${QD_PROTOCOL_FIXTURES}" "${QD_FRAME_PROJECTIONS}")
    add_executable(description_tests tests/conformance/description_tests.cpp)
    target_link_libraries(description_tests PRIVATE qd_protocol doctest::doctest)
    set_target_properties(description_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_DESCRIPTION_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/p08/expected/descriptions.tsv"
        CACHE FILEPATH "Java description observations (temporary override for negative checks)")
    add_test(NAME protocol_java_descriptions COMMAND description_tests "${QD_DESCRIPTION_FIXTURES}")
    add_executable(binding_tests tests/conformance/binding_tests.cpp)
    target_link_libraries(binding_tests PRIVATE qd_protocol doctest::doctest)
    set_target_properties(binding_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_BINDING_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/protocol/binding/plans.tsv"
        CACHE FILEPATH "Reviewed static binding projections (temporary override for negative checks)")
    add_test(NAME protocol_java_binding COMMAND binding_tests "${QD_BINDING_FIXTURES}")
    add_executable(symbol_tests tests/conformance/symbol_tests.cpp)
    target_link_libraries(symbol_tests PRIVATE qd_encoding doctest::doctest)
    set_target_properties(symbol_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_SYMBOL_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/symbols/expected/symbols.tsv"
        CACHE FILEPATH "Java symbol observations (temporary override for negative checks)")
    add_test(NAME encoding_java_symbols COMMAND symbol_tests "${QD_SYMBOL_FIXTURES}")
    add_executable(stream_data_tests tests/conformance/stream_data_tests.cpp)
    target_link_libraries(stream_data_tests PRIVATE qd_protocol doctest::doctest)
    set_target_properties(stream_data_tests PROPERTIES CXX_EXTENSIONS OFF)
    set(QD_STREAM_DATA_FIXTURES "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/stream-data/expected/stream-data.tsv"
        CACHE FILEPATH "Java STREAM_DATA observations (temporary override for negative checks)")
    add_test(NAME protocol_java_stream_data COMMAND stream_data_tests "${QD_STREAM_DATA_FIXTURES}")
    if(QD_BUILD_QTP_INSPECT)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME offline_inspection_cli COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/check_offline_inspection.py"
            "$<TARGET_FILE:qd-qtp-inspect>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection")
        add_executable(offline_io_tests tests/conformance/offline_io_tests.cpp)
        target_link_libraries(offline_io_tests PRIVATE qd_inspect_tool doctest::doctest)
        set_target_properties(offline_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME offline_inspection_injected_io COMMAND offline_io_tests)
    endif()
endif()
