cmake_minimum_required(VERSION 3.25)
project(qd_cxx LANGUAGES CXX)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, for example -B .work/build")
endif()

add_library(qd_encoding src/encoding/compact.cpp)
target_include_directories(qd_encoding PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(qd_encoding PUBLIC cxx_std_23)
set_target_properties(qd_encoding PROPERTIES CXX_EXTENSIONS OFF)

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
endif()
