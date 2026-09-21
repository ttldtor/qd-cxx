cmake_minimum_required(VERSION 3.25)
project(qd_cxx LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, for example -B .work/build")
endif()

add_library(qd_encoding src/encoding/compact.cpp src/encoding/utf_string.cpp)
target_include_directories(qd_encoding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qd_encoding PUBLIC cxx_std_23)
set_target_properties(qd_encoding PROPERTIES CXX_EXTENSIONS OFF)

add_library(qd_protocol src/protocol/frame_reader.cpp)
target_link_libraries(qd_protocol PUBLIC qd_encoding)
set_target_properties(qd_protocol PROPERTIES CXX_EXTENSIONS OFF)

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
endif()
