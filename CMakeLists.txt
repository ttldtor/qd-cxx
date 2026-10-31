# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

cmake_minimum_required(VERSION 3.25)
project(qdcxx LANGUAGES CXX)

include(GNUInstallDirs)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build, for example -B .work/build")
endif()

add_library(qdcxx_encoding src/encoding/compact.cpp src/encoding/utf_string.cpp src/encoding/symbol_reader.cpp src/encoding/utf_char.cpp src/encoding/char_array.cpp)
target_include_directories(qdcxx_encoding PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(qdcxx_encoding PUBLIC cxx_std_23)
set_target_properties(qdcxx_encoding PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_core src/core/local_schema.cpp src/core/wide_decimal.cpp src/core/subscription.cpp)
target_include_directories(qdcxx_core PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(qdcxx_core PUBLIC cxx_std_23)
set_target_properties(qdcxx_core PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_mux src/mux/interest.cpp)
target_link_libraries(qdcxx_mux PUBLIC qdcxx_core)
set_target_properties(qdcxx_mux PROPERTIES CXX_EXTENSIONS OFF)

option(ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON "Build the bounded native JSON schema loader" ON)
if(ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON)
    include(FetchContent)
    # Build only the pinned amalgamation: no upstream tools, tests or interpreter discovery.
    FetchContent_Declare(simdjson
        URL https://codeload.github.com/simdjson/simdjson/tar.gz/refs/tags/v4.6.4
        URL_HASH SHA256=b091107844fe928158c5c2265c20360fff312889ddf7ebc4528a0f0f8f2ff9cd
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR qdcxx-unused
    )
    FetchContent_MakeAvailable(simdjson)
    # The binary loader's parser-request model is qualified against these complete amalgamated sources.
    file(SHA256 "${simdjson_SOURCE_DIR}/singleheader/simdjson.h" qdcxxSimdjsonHeaderHash)
    file(SHA256 "${simdjson_SOURCE_DIR}/singleheader/simdjson.cpp" qdcxxSimdjsonSourceHash)
    if(NOT qdcxxSimdjsonHeaderHash STREQUAL "fec0cb4559e25456789efb153b7e5eaa27b50ce2bc2c9d3b531ea2000928380a"
        OR NOT qdcxxSimdjsonSourceHash STREQUAL "826660ed60cf467641ab2f93a0e1a2dacf603c444f76d835358c814bc2caba3a")
        message(FATAL_ERROR "The native binary JSON parser model requires the pinned simdjson 4.6.4 amalgamation")
    endif()
    add_library(qdcxx_simdjson STATIC "${simdjson_SOURCE_DIR}/singleheader/simdjson.cpp")
    target_include_directories(qdcxx_simdjson SYSTEM PUBLIC "$<BUILD_INTERFACE:${simdjson_SOURCE_DIR}/singleheader>")
    target_compile_features(qdcxx_simdjson PRIVATE cxx_std_17)
    set_target_properties(qdcxx_simdjson PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
    add_library(qdcxx_schema_json src/schema/json_schema.cpp)
    target_link_libraries(qdcxx_schema_json PUBLIC qdcxx_core PRIVATE qdcxx_simdjson)
    set_target_properties(qdcxx_schema_json PROPERTIES CXX_EXTENSIONS OFF)
    install(TARGETS qdcxx_schema_json qdcxx_simdjson EXPORT qdcxxTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY include/qd/schema DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd"
        FILES_MATCHING PATTERN "*.hpp")
    install(FILES licenses/simdjson-4.6.4/LICENSE licenses/simdjson-4.6.4/LICENSE-MIT
        DESTINATION "${CMAKE_INSTALL_DATADIR}/qdcxx/licenses/simdjson-4.6.4")
endif()

add_library(qdcxx_event_mapping src/events/event_mapping.cpp)
target_link_libraries(qdcxx_event_mapping PUBLIC qdcxx_core)
set_target_properties(qdcxx_event_mapping PROPERTIES CXX_EXTENSIONS OFF)
add_library(qdcxx_profile src/events/profile.cpp)
target_link_libraries(qdcxx_profile PUBLIC qdcxx_event_mapping)
set_target_properties(qdcxx_profile PROPERTIES CXX_EXTENSIONS OFF)
add_library(qdcxx_quote src/events/quote.cpp)
target_link_libraries(qdcxx_quote PUBLIC qdcxx_event_mapping)
set_target_properties(qdcxx_quote PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_protocol src/protocol/frame_reader.cpp src/protocol/describe_records.cpp src/protocol/static_binding.cpp src/protocol/stream_data.cpp src/protocol/tape_reader.cpp src/protocol/tape_writer.cpp src/protocol/tape_admission.cpp src/protocol/subscription_writer.cpp src/protocol/live_session.cpp)
target_link_libraries(qdcxx_protocol PUBLIC qdcxx_encoding qdcxx_core)
set_target_properties(qdcxx_protocol PROPERTIES CXX_EXTENSIONS OFF)

add_library(qdcxx_collectors src/collectors/stream.cpp src/collectors/ticker.cpp src/collectors/history.cpp)
target_link_libraries(qdcxx_collectors PUBLIC qdcxx_protocol)
set_target_properties(qdcxx_collectors PROPERTIES CXX_EXTENSIONS OFF)

option(ORG_TTLDTOR_QDCXX_BUILD_CLIENT "Build the bounded event-driven STREAM client dispatcher" OFF)
if(ORG_TTLDTOR_QDCXX_BUILD_CLIENT)
    find_package(Threads REQUIRED)
    add_library(qdcxx_client src/client/stream_dispatcher.cpp src/client/endpoint_options.cpp)
    target_link_libraries(qdcxx_client PUBLIC qdcxx_collectors Threads::Threads)
    set_target_properties(qdcxx_client PROPERTIES CXX_EXTENSIONS OFF)
    install(TARGETS qdcxx_client EXPORT qdcxxTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(DIRECTORY include/qd/client
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd" FILES_MATCHING PATTERN "*.hpp")
endif()

option(ORG_TTLDTOR_QDCXX_BUILD_RUNTIME "Build the explicit credential-free caller-driven TCP runtime" OFF)
if(ORG_TTLDTOR_QDCXX_BUILD_RUNTIME)
    include(cmake/Runtime.cmake)
endif()

include(CMakePackageConfigHelpers)
install(TARGETS qdcxx_encoding qdcxx_core qdcxx_protocol qdcxx_collectors qdcxx_mux
    qdcxx_event_mapping qdcxx_quote qdcxx_profile EXPORT qdcxxTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(DIRECTORY include/qd/core include/qd/encoding include/qd/protocol include/qd/collectors include/qd/events include/qd/mux
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/qd" FILES_MATCHING PATTERN "*.hpp")
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATADIR}/qdcxx/licenses" RENAME qdcxx-LICENSE.txt)
configure_package_config_file(cmake/qdcxxConfig.cmake.in "${CMAKE_CURRENT_BINARY_DIR}/qdcxxConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/qdcxx"
    PATH_VARS CMAKE_INSTALL_LIBDIR)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qdcxxConfig.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/qdcxx")
install(EXPORT qdcxxTargets FILE qdcxxTargets.cmake DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/qdcxx")

option(ORG_TTLDTOR_QDCXX_BUILD_REPLAY "Build the bounded synchronous offline replay library" OFF)
if(ORG_TTLDTOR_QDCXX_BUILD_REPLAY)
    add_library(qdcxx_replay src/replay/tape_replay.cpp)
    target_link_libraries(qdcxx_replay PUBLIC qdcxx_protocol)
    set_target_properties(qdcxx_replay PROPERTIES CXX_EXTENSIONS OFF)
endif()

option(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT "Build the bounded binary tape diagnostic CLI" OFF)
option(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT "Build the bounded offline QTP diagnostic CLI" OFF)
option(ORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE "Build the strict bounded offline tape writer CLI" OFF)
option(ORG_TTLDTOR_QDCXX_BUILD_CONNECT "Build the controlled credential-free STREAM subscription CLI" OFF)
if(ORG_TTLDTOR_QDCXX_BUILD_CONNECT)
    if(NOT ORG_TTLDTOR_QDCXX_BUILD_CLIENT OR NOT ORG_TTLDTOR_QDCXX_BUILD_RUNTIME)
        message(FATAL_ERROR "BUILD_CONNECT requires explicit ORG_TTLDTOR_QDCXX_BUILD_CLIENT=ON and ORG_TTLDTOR_QDCXX_BUILD_RUNTIME=ON")
    endif()
    add_library(qdcxx_stream_application STATIC examples/subscriptions/stream_client.cpp examples/subscriptions/example.cpp)
    target_include_directories(qdcxx_stream_application PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/examples/subscriptions")
    target_link_libraries(qdcxx_stream_application PUBLIC qdcxx_client qdcxx_transport)
    set_target_properties(qdcxx_stream_application PROPERTIES CXX_EXTENSIONS OFF)
    add_library(qdcxx_connect_tool STATIC tools/subscribe/configuration.cpp tools/subscribe/output.cpp)
    target_include_directories(qdcxx_connect_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/subscribe")
    target_link_libraries(qdcxx_connect_tool PUBLIC qdcxx_stream_application qdcxx_quote qdcxx_profile)
    set_target_properties(qdcxx_connect_tool PROPERTIES CXX_EXTENSIONS OFF)
    if(WIN32)
        target_compile_definitions(qdcxx_connect_tool PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(qdcxx_connect_tool PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
        target_link_libraries(qdcxx_connect_tool PRIVATE ws2_32)
    endif()
    add_executable(qdcxx_connect tools/subscribe/main.cpp)
    target_link_libraries(qdcxx_connect PRIVATE qdcxx_connect_tool)
    set_target_properties(qdcxx_connect PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "qdcxx-connect")
endif()
if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE)
    add_executable(qdcxx_tape_write tools/tape-write/main.cpp tools/tape-write/configuration.cpp tools/tape-write/file_boundary.cpp)
    target_link_libraries(qdcxx_tape_write PRIVATE qdcxx_protocol qdcxx_quote qdcxx_profile)
    if(ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON)
        target_link_libraries(qdcxx_tape_write PRIVATE qdcxx_schema_json)
        target_compile_definitions(qdcxx_tape_write PRIVATE ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON=1)
    endif()
    set_target_properties(qdcxx_tape_write PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "qdcxx-tape-write")
endif()
if(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT OR ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
    add_library(qdcxx_inspect_tool STATIC tools/qtp-inspect/inspect.cpp tools/qtp-inspect/formatter.cpp
        tools/tape-inspect/source_format.cpp)
    target_link_libraries(qdcxx_inspect_tool PUBLIC qdcxx_protocol qdcxx_quote qdcxx_profile)
    target_include_directories(qdcxx_inspect_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/qtp-inspect")
    set_target_properties(qdcxx_inspect_tool PROPERTIES CXX_EXTENSIONS OFF)
endif()
if(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT)
    add_executable(qdcxx_qtp_inspect tools/qtp-inspect/main.cpp)
    target_link_libraries(qdcxx_qtp_inspect PRIVATE qdcxx_inspect_tool)
    set_target_properties(qdcxx_qtp_inspect PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "qdcxx-qtp-inspect")
endif()

if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
    add_library(qdcxx_tape_inspect_tool STATIC tools/tape-inspect/inspect.cpp tools/tape-inspect/diagnostics.cpp
        tools/tape-inspect/schema_input.cpp tools/tape-inspect/tape_sequence.cpp
        tools/tape-inspect/catalog_segment.cpp tools/tape-inspect/sequence_owner.cpp)
    target_include_directories(qdcxx_tape_inspect_tool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/tape-inspect")
    target_link_libraries(qdcxx_tape_inspect_tool PUBLIC qdcxx_inspect_tool qdcxx_tape_sidecar qdcxx_tape_compression)
    set_target_properties(qdcxx_tape_inspect_tool PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(qdcxx_tape_inspect tools/tape-inspect/main.cpp)
    target_sources(qdcxx_tape_inspect PRIVATE tools/tape-inspect/source_cli.cpp)
    target_link_libraries(qdcxx_tape_inspect PRIVATE qdcxx_tape_inspect_tool)
    if(ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON)
        target_link_libraries(qdcxx_tape_inspect PRIVATE qdcxx_schema_json)
        target_compile_definitions(qdcxx_tape_inspect PRIVATE ORG_TTLDTOR_QDCXX_HAS_SCHEMA_JSON=1)
    endif()
    set_target_properties(qdcxx_tape_inspect PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME "qdcxx-tape-inspect")
endif()
include(CTest)
if(BUILD_TESTING OR ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
    add_library(qdcxx_tape_sidecar STATIC tools/tape-inspect/sidecar_time.cpp)
    target_compile_features(qdcxx_tape_sidecar PUBLIC cxx_std_23)
    target_include_directories(qdcxx_tape_sidecar PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/tools/tape-inspect")
    set_target_properties(qdcxx_tape_sidecar PROPERTIES CXX_EXTENSIONS OFF)
endif()
option(ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION "Build optional closed GZIP/ZIP tool input" OFF)
set(ORG_TTLDTOR_QDCXX_ZLIB_ARCHIVE "" CACHE FILEPATH "Optional exact zlib 1.3.2 archive, checked only when needed")
if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT OR BUILD_TESTING)
    add_library(qdcxx_tape_compression STATIC tools/tape-inspect/compressed_input.cpp)
    target_compile_features(qdcxx_tape_compression PUBLIC cxx_std_23)
    set_target_properties(qdcxx_tape_compression PROPERTIES CXX_EXTENSIONS OFF)
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION)
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
        target_compile_definitions(qdcxx_tape_compression PRIVATE ORG_TTLDTOR_QDCXX_HAS_ZLIB)
        target_link_libraries(qdcxx_tape_compression PRIVATE ZLIB::ZLIBSTATIC)
    endif()
endif()
if(BUILD_TESTING)
    add_executable(qdcxx_field_time_tape_tests tests/conformance/field_time_tape_tests.cpp)
    target_link_libraries(qdcxx_field_time_tape_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_field_time_tape_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_field_time COMMAND qdcxx_field_time_tape_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/field-time"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/current-field-time-expected")
    include(FetchContent)
    # Test-only dependency, pinned to the v2.4.12 release archive and its SHA-256.
    FetchContent_Declare(doctest
        URL https://codeload.github.com/doctest/doctest/tar.gz/refs/tags/v2.4.12
        URL_HASH SHA256=73381c7aa4dee704bd935609668cf41880ea7f19fa0504a200e13b74999c2d70
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(doctest)
    if(ORG_TTLDTOR_QDCXX_BUILD_CONNECT)
        add_executable(qdcxx_connect_tests tests/conformance/connect_tests.cpp tests/conformance/connect_peer.cpp)
        target_link_libraries(qdcxx_connect_tests PRIVATE qdcxx_connect_tool doctest::doctest)
        set_target_properties(qdcxx_connect_tests PROPERTIES CXX_EXTENSIONS OFF)
        if(WIN32)
            target_compile_definitions(qdcxx_connect_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            target_link_libraries(qdcxx_connect_tests PRIVATE ws2_32)
        endif()
        add_test(NAME connect_stream COMMAND qdcxx_connect_tests --no-colors --no-path-filenames)
        set_tests_properties(connect_stream PROPERTIES TIMEOUT 45)
        add_test(NAME endpoint_credit_lifecycle COMMAND qdcxx_connect_tests --no-colors --no-path-filenames "--test-case=snapshot work limits retain credit caps through controlled retirement")
        set_tests_properties(endpoint_credit_lifecycle PROPERTIES TIMEOUT 8)
        add_test(NAME connect_cli_contract COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_connect>" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/connect-cli"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/connect_cli.cmake")
        add_executable(qdcxx_connect_process_tests tests/conformance/connect_process_tests.cpp tests/conformance/connect_peer.cpp)
        target_link_libraries(qdcxx_connect_process_tests PRIVATE qdcxx_transport qdcxx_quote qdcxx_profile)
        set_target_properties(qdcxx_connect_process_tests PROPERTIES CXX_EXTENSIONS OFF)
        if(WIN32)
            target_compile_definitions(qdcxx_connect_process_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            target_link_libraries(qdcxx_connect_process_tests PRIVATE ws2_32)
        endif()
        add_test(NAME connect_process COMMAND qdcxx_connect_process_tests "$<TARGET_FILE:qdcxx_connect>"
            "${CMAKE_CURRENT_BINARY_DIR}/connect-process")
        set_tests_properties(connect_process PROPERTIES TIMEOUT 45)
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_RUNTIME)
        add_executable(qdcxx_tcp_tests tests/conformance/transport/tcp_tests.cpp)
        target_link_libraries(qdcxx_tcp_tests PRIVATE qdcxx_transport qdcxx_quote doctest::doctest c-ares::cares)
        target_include_directories(qdcxx_tcp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/transport")
        target_compile_definitions(qdcxx_tcp_tests PRIVATE
            "ORG_TTLDTOR_QDCXX_TCP_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
        set_target_properties(qdcxx_tcp_tests PROPERTIES CXX_EXTENSIONS OFF)
        if(WIN32)
            target_compile_definitions(qdcxx_tcp_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            target_link_libraries(qdcxx_tcp_tests PRIVATE ws2_32)
        endif()
        add_test(NAME transport_tcp COMMAND qdcxx_tcp_tests)
        set_tests_properties(transport_tcp PROPERTIES TIMEOUT 45)
        add_executable(qdcxx_cares_ownership_tests tests/conformance/transport/cares_ownership_tests.c)
        target_compile_features(qdcxx_cares_ownership_tests PRIVATE c_std_99)
        target_compile_definitions(qdcxx_cares_ownership_tests PRIVATE HAVE_CONFIG_H=1 CARES_BUILDING_LIBRARY)
        target_include_directories(qdcxx_cares_ownership_tests PRIVATE
            "${CMAKE_CURRENT_BINARY_DIR}/qdcxx-runtime-cares" "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}"
            "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/src/lib" "${ORG_TTLDTOR_QDCXX_CARES_SOURCE}/src/lib/include")
        if(WIN32)
            target_compile_definitions(qdcxx_cares_ownership_tests PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0602)
        else()
            target_compile_definitions(qdcxx_cares_ownership_tests PRIVATE
                _GNU_SOURCE _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700)
        endif()
        target_link_libraries(qdcxx_cares_ownership_tests PRIVATE c-ares::cares)
        add_test(NAME transport_cares_ownership COMMAND qdcxx_cares_ownership_tests)
        set_tests_properties(transport_cares_ownership PROPERTIES TIMEOUT 20)
    endif()
    add_executable(qdcxx_mux_interest_tests tests/conformance/mux_interest_tests.cpp)
    target_link_libraries(qdcxx_mux_interest_tests PRIVATE qdcxx_mux qdcxx_protocol qdcxx_quote doctest::doctest)
    set_target_properties(qdcxx_mux_interest_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME mux_interest COMMAND qdcxx_mux_interest_tests)
    set_tests_properties(mux_interest PROPERTIES TIMEOUT 60)

    add_executable(qdcxx_subscription_state_tests tests/conformance/subscription_state_tests.cpp)
    target_link_libraries(qdcxx_subscription_state_tests PRIVATE qdcxx_core doctest::doctest)
    set_target_properties(qdcxx_subscription_state_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME core_subscription_state COMMAND qdcxx_subscription_state_tests)
    set_tests_properties(core_subscription_state PROPERTIES TIMEOUT 60)
    add_executable(qdcxx_subscription_writer_tests tests/conformance/subscription_writer_tests.cpp)
    target_link_libraries(qdcxx_subscription_writer_tests PRIVATE qdcxx_protocol doctest::doctest)
    target_compile_definitions(qdcxx_subscription_writer_tests PRIVATE
        QDCXX_SUBSCRIPTION_CAPTURE_DIR="${CMAKE_CURRENT_BINARY_DIR}/subscription-native-captures")
    set_target_properties(qdcxx_subscription_writer_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_subscription_writer COMMAND qdcxx_subscription_writer_tests)
    set_tests_properties(protocol_subscription_writer PROPERTIES TIMEOUT 60)
    add_executable(qdcxx_live_session_tests tests/conformance/live_session_tests.cpp)
    target_link_libraries(qdcxx_live_session_tests PRIVATE qdcxx_protocol qdcxx_quote doctest::doctest)
    target_compile_definitions(qdcxx_live_session_tests PRIVATE
        "ORG_TTLDTOR_QDCXX_LIVE_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        "ORG_TTLDTOR_QDCXX_LIVE_CAPTURE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/live-session-native\"")
    set_target_properties(qdcxx_live_session_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_live_session COMMAND qdcxx_live_session_tests)
    set_tests_properties(protocol_live_session PROPERTIES TIMEOUT 120)
    add_executable(qdcxx_stream_collector_tests tests/conformance/stream_collector_tests.cpp)
    target_link_libraries(qdcxx_stream_collector_tests PRIVATE qdcxx_collectors qdcxx_quote doctest::doctest)
    target_compile_definitions(qdcxx_stream_collector_tests PRIVATE
        "ORG_TTLDTOR_QDCXX_STREAM_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        "ORG_TTLDTOR_QDCXX_STREAM_CAPTURE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/stream-native\"")
    set_target_properties(qdcxx_stream_collector_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME collectors_stream COMMAND qdcxx_stream_collector_tests)
    set_tests_properties(collectors_stream PROPERTIES TIMEOUT 120)
    add_executable(qdcxx_ticker_collector_tests tests/conformance/ticker_collector_tests.cpp)
    if(NOT TARGET Threads::Threads)
        find_package(Threads REQUIRED)
    endif()
    target_link_libraries(qdcxx_ticker_collector_tests PRIVATE qdcxx_collectors qdcxx_quote qdcxx_profile doctest::doctest Threads::Threads)
    target_compile_definitions(qdcxx_ticker_collector_tests PRIVATE
        "ORG_TTLDTOR_QDCXX_TICKER_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        "ORG_TTLDTOR_QDCXX_TICKER_CAPTURE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/ticker-native\"")
    set_target_properties(qdcxx_ticker_collector_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME collectors_ticker COMMAND qdcxx_ticker_collector_tests)
    set_tests_properties(collectors_ticker PROPERTIES TIMEOUT 120)
    add_executable(qdcxx_history_collector_tests tests/conformance/history_collector_tests.cpp)
    target_link_libraries(qdcxx_history_collector_tests PRIVATE qdcxx_collectors qdcxx_quote qdcxx_profile doctest::doctest Threads::Threads)
    target_compile_definitions(qdcxx_history_collector_tests PRIVATE
        "ORG_TTLDTOR_QDCXX_HISTORY_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\""
        "ORG_TTLDTOR_QDCXX_HISTORY_CAPTURE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/history-native\"")
    set_target_properties(qdcxx_history_collector_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME collectors_history COMMAND qdcxx_history_collector_tests)
    set_tests_properties(collectors_history PROPERTIES TIMEOUT 120)
    if(ORG_TTLDTOR_QDCXX_BUILD_CLIENT)
        add_executable(qdcxx_endpoint_options_tests tests/conformance/endpoint_options_tests.cpp)
        target_link_libraries(qdcxx_endpoint_options_tests PRIVATE qdcxx_client qdcxx_quote doctest::doctest)
        add_test(NAME endpoint_options COMMAND qdcxx_endpoint_options_tests --no-colors --no-path-filenames)
        add_executable(qdcxx_client_dispatch_tests tests/conformance/client_dispatch_tests.cpp)
        target_link_libraries(qdcxx_client_dispatch_tests PRIVATE qdcxx_client qdcxx_quote doctest::doctest)
        target_compile_definitions(qdcxx_client_dispatch_tests PRIVATE
            "ORG_TTLDTOR_QDCXX_CLIENT_SOURCE_ROOT=\"${CMAKE_CURRENT_SOURCE_DIR}\"")
        set_target_properties(qdcxx_client_dispatch_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME client_dispatch COMMAND qdcxx_client_dispatch_tests)
        set_tests_properties(client_dispatch PROPERTIES TIMEOUT 120)
    endif()
    if(TARGET qdcxx_inspect_tool)
        add_executable(qdcxx_source_format_tests tests/conformance/source_format_tests.cpp)
        target_link_libraries(qdcxx_source_format_tests PRIVATE qdcxx_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_source_format_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME source_format COMMAND qdcxx_source_format_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/field-time"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/current-field-time-expected")
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
        add_executable(qdcxx_tape_sequence_tests tests/conformance/tape_sequence_tests.cpp)
        target_link_libraries(qdcxx_tape_sequence_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_tape_sequence_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME tape_sequence_admission COMMAND qdcxx_tape_sequence_tests)
        add_executable(qdcxx_tape_sequence_owner_tests tests/conformance/tape_sequence_owner_tests.cpp)
        target_link_libraries(qdcxx_tape_sequence_owner_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_tape_sequence_owner_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME source_sequence_io COMMAND qdcxx_tape_sequence_owner_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-containers"
            "${CMAKE_CURRENT_BINARY_DIR}/sequence-owner-fixtures")
        add_executable(qdcxx_large_tape_tests tests/conformance/large_tape_tests.cpp)
        target_link_libraries(qdcxx_large_tape_tests PRIVATE qdcxx_tape_inspect_tool)
        set_target_properties(qdcxx_large_tape_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME source_sequence_large COMMAND qdcxx_large_tape_tests
            "${CMAKE_CURRENT_BINARY_DIR}/large-tape-streaming")
        add_test(NAME source_sequence_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>"
            "-DCOMPARATOR=$<TARGET_FILE:qdcxx_large_tape_tests>"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/source-sequence-cli"
            "-DCOMPRESSION=${ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/source_sequence_cli.cmake")
    endif()
    add_executable(qdcxx_plain_input_tests tests/conformance/plain_input_tests.cpp)
    target_link_libraries(qdcxx_plain_input_tests PRIVATE qdcxx_tape_compression doctest::doctest)
    target_compile_definitions(qdcxx_plain_input_tests PRIVATE
        ORG_TTLDTOR_QDCXX_TEST_COMPRESSION=$<BOOL:${ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION}>)
    set_target_properties(qdcxx_plain_input_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME plain_input COMMAND qdcxx_plain_input_tests)
    add_executable(qdcxx_sidecar_tests tests/conformance/sidecar_time_tests.cpp)
    target_link_libraries(qdcxx_sidecar_tests PRIVATE qdcxx_tape_sidecar doctest::doctest)
    add_test(NAME sidecar_scanner COMMAND qdcxx_sidecar_tests)
    add_executable(qdcxx_sidecar_policy_tests tests/conformance/sidecar_policy_tests.cpp)
    target_link_libraries(qdcxx_sidecar_policy_tests PRIVATE qdcxx_protocol doctest::doctest)
    add_test(NAME sidecar_policy COMMAND qdcxx_sidecar_policy_tests)
    add_executable(qdcxx_sidecar_include_tests tests/conformance/sidecar_windows_include_test.cpp)
    target_link_libraries(qdcxx_sidecar_include_tests PRIVATE qdcxx_tape_sidecar)
    add_test(NAME sidecar_windows_include COMMAND qdcxx_sidecar_include_tests)
    add_executable(qdcxx_sidecar_allocation_tests tests/conformance/sidecar_allocation_test.cpp)
    target_link_libraries(qdcxx_sidecar_allocation_tests PRIVATE qdcxx_tape_sidecar)
    add_test(NAME sidecar_allocation COMMAND qdcxx_sidecar_allocation_tests)
    set_target_properties(qdcxx_sidecar_tests qdcxx_sidecar_policy_tests qdcxx_sidecar_include_tests
        qdcxx_sidecar_allocation_tests PROPERTIES CXX_EXTENSIONS OFF)
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION)
        add_executable(qdcxx_sidecar_owner_tests tests/conformance/sidecar_owner_tests.cpp)
        target_link_libraries(qdcxx_sidecar_owner_tests PRIVATE qdcxx_tape_sidecar qdcxx_tape_compression
            qdcxx_protocol ZLIB::ZLIBSTATIC doctest::doctest)
        set_target_properties(qdcxx_sidecar_owner_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME sidecar_paired_owner COMMAND qdcxx_sidecar_owner_tests)
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_COMPRESSION)
        add_executable(qdcxx_compressed_input_tests tests/conformance/compressed_input_tests.cpp)
        target_link_libraries(qdcxx_compressed_input_tests PRIVATE qdcxx_tape_compression ZLIB::ZLIBSTATIC doctest::doctest)
        target_compile_definitions(qdcxx_compressed_input_tests PRIVATE
            "ORG_TTLDTOR_QDCXX_CONTAINER_FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-containers\"")
        set_target_properties(qdcxx_compressed_input_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME compressed_input COMMAND qdcxx_compressed_input_tests)
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
        add_executable(qdcxx_tape_message_family_tests tests/conformance/tape_message_family_tests.cpp)
        target_link_libraries(qdcxx_tape_message_family_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_tape_message_family_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME protocol_tape_message_families COMMAND qdcxx_tape_message_family_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/dispatch")
        add_test(NAME binary_catalog_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/binary_catalog_cli.cmake")
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_REPLAY)
        add_executable(qdcxx_tape_replay_tests tests/conformance/tape_replay_tests.cpp)
        target_link_libraries(qdcxx_tape_replay_tests PRIVATE qdcxx_replay doctest::doctest)
        set_target_properties(qdcxx_tape_replay_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME offline_tape_replay COMMAND qdcxx_tape_replay_tests "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    add_executable(qdcxx_tape_admission_tests tests/conformance/tape_admission_tests.cpp)
    target_link_libraries(qdcxx_tape_admission_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_tape_admission_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_tape_admission COMMAND qdcxx_tape_admission_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-write/contracts")
    add_executable(qdcxx_tape_write_file_tests tests/conformance/tape_write_file_tests.cpp tools/tape-write/file_boundary.cpp)
    target_link_libraries(qdcxx_tape_write_file_tests PRIVATE qdcxx_core doctest::doctest)
    target_include_directories(qdcxx_tape_write_file_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tools/tape-write")
    set_target_properties(qdcxx_tape_write_file_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tape_write_file_boundary COMMAND qdcxx_tape_write_file_tests)
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_WRITE)
        add_test(NAME tape_write_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_write>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DADMISSION_PRODUCER=$<TARGET_FILE:qdcxx_tape_admission_tests>"
            "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/tape-write-cli"
            "-DJSON_ENABLED=${ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/tape_write_cli.cmake")
    endif()
    add_executable(qdcxx_tape_writer_tests tests/conformance/tape_writer_tests.cpp)
    target_link_libraries(qdcxx_tape_writer_tests PRIVATE qdcxx_protocol qdcxx_quote qdcxx_profile doctest::doctest)
    set_target_properties(qdcxx_tape_writer_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_tape_writer COMMAND qdcxx_tape_writer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-write/contracts")
    add_executable(qdcxx_char_array_writer_tests tests/conformance/char_array_writer_tests.cpp)
    target_link_libraries(qdcxx_char_array_writer_tests PRIVATE qdcxx_encoding doctest::doctest)
    set_target_properties(qdcxx_char_array_writer_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME encoding_char_array_writer COMMAND qdcxx_char_array_writer_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-write/contracts")
    if(ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON)
        add_executable(qdcxx_schema_json_tests tests/conformance/schema_json_tests.cpp)
        target_link_libraries(qdcxx_schema_json_tests PRIVATE qdcxx_schema_json qdcxx_simdjson doctest::doctest)
        set_target_properties(qdcxx_schema_json_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME schema_json_contract COMMAND qdcxx_schema_json_tests)
        if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
            add_executable(qdcxx_schema_json_integration_tests tests/conformance/schema_json_integration_tests.cpp)
            target_link_libraries(qdcxx_schema_json_integration_tests PRIVATE qdcxx_schema_json qdcxx_tape_inspect_tool doctest::doctest)
            set_target_properties(qdcxx_schema_json_integration_tests PROPERTIES CXX_EXTENSIONS OFF)
            add_test(NAME schema_json_integration COMMAND qdcxx_schema_json_integration_tests "${CMAKE_CURRENT_SOURCE_DIR}")
            add_executable(qdcxx_schema_export_tests tests/conformance/schema_export_tests.cpp)
            target_link_libraries(qdcxx_schema_export_tests PRIVATE qdcxx_schema_json qdcxx_tape_inspect_tool doctest::doctest)
            set_target_properties(qdcxx_schema_export_tests PROPERTIES CXX_EXTENSIONS OFF)
            add_test(NAME schema_export_consumption COMMAND qdcxx_schema_export_tests "${CMAKE_CURRENT_SOURCE_DIR}")
            add_test(NAME schema_export_cli COMMAND "${CMAKE_COMMAND}"
                "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/schema_export_cli.cmake")
        endif()
    endif()
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
        add_test(NAME schema_json_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DENABLED=${ORG_TTLDTOR_QDCXX_BUILD_SCHEMA_JSON}" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/schema-json-cli"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/schema_json_cli.cmake")
    endif()
    # Verification-only image tracing is implemented for the Windows/Linux acceptance hosts.
    if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
        add_executable(qdcxx_no_python_guard tests/conformance/no_python_guard.cpp)
        target_compile_features(qdcxx_no_python_guard PRIVATE cxx_std_23)
        set_target_properties(qdcxx_no_python_guard PROPERTIES CXX_EXTENSIONS OFF)
    endif()
    add_subdirectory(tools/dev)
    include(tests/conformance/tooling/register.cmake)
    add_executable(qdcxx_event_mapping_tests tests/conformance/event_mapping_tests.cpp)
    target_link_libraries(qdcxx_event_mapping_tests PRIVATE qdcxx_event_mapping qdcxx_protocol doctest::doctest)
    add_test(NAME event_mapping_contract COMMAND qdcxx_event_mapping_tests)
    add_executable(qdcxx_profile_tests tests/conformance/profile_tests.cpp)
    target_link_libraries(qdcxx_profile_tests PRIVATE qdcxx_profile doctest::doctest)
    add_test(NAME profile_mapping_contract COMMAND qdcxx_profile_tests)
    add_executable(qdcxx_event_oracle_tests tests/conformance/event_oracle_tests.cpp)
    target_link_libraries(qdcxx_event_oracle_tests PRIVATE qdcxx_quote qdcxx_profile qdcxx_protocol doctest::doctest)
    add_test(NAME configured_java_events COMMAND qdcxx_event_oracle_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/events/mapping/expected")
    add_test(NAME configured_event_fixture_negatives COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_event_oracle_tests>" "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/events/mapping/expected" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/event-negatives" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/event_oracle_negatives.cmake")
    set_target_properties(qdcxx_event_mapping_tests qdcxx_profile_tests qdcxx_event_oracle_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(qdcxx_field_type_value_tests tests/conformance/field_type_value_tests.cpp)
    target_link_libraries(qdcxx_field_type_value_tests PRIVATE qdcxx_quote qdcxx_encoding doctest::doctest)
    set_target_properties(qdcxx_field_type_value_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME field_type_java_values COMMAND qdcxx_field_type_value_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/primitives.tsv")
    add_test(NAME field_type_fixture_negatives COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_field_type_value_tests>" "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/primitives.tsv" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/field-type-negatives" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/field_type_negatives.cmake")
    add_executable(qdcxx_field_type_tape_tests tests/conformance/field_type_tape_tests.cpp)
    target_link_libraries(qdcxx_field_type_tape_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_field_type_tape_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME field_type_java_tape COMMAND qdcxx_field_type_tape_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected")
    add_executable(qdcxx_object_field_value_tests tests/conformance/object_field_value_tests.cpp)
    target_link_libraries(qdcxx_object_field_value_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_object_field_value_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME object_field_owned_values COMMAND qdcxx_object_field_value_tests)
    add_executable(qdcxx_indexed_schema_tests tests/conformance/indexed_schema_tests.cpp)
    target_link_libraries(qdcxx_indexed_schema_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_indexed_schema_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME indexed_schema_layout COMMAND qdcxx_indexed_schema_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/indexed"
        "--test-case-exclude=production*")
    add_test(NAME indexed_schema_java COMMAND qdcxx_indexed_schema_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/indexed"
        "--test-case=production*")
    add_test(NAME indexed_schema_java_current COMMAND qdcxx_indexed_schema_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/current-indexed-expected"
        "--test-case=production*")
    add_executable(qdcxx_tape_session_tests tests/conformance/tape_session_tests.cpp)
    target_link_libraries(qdcxx_tape_session_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_tape_session_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME tape_session_generations COMMAND qdcxx_tape_session_tests)
    add_executable(qdcxx_raw_tape_tests tests/conformance/raw_tape_tests.cpp)
    target_link_libraries(qdcxx_raw_tape_tests PRIVATE qdcxx_protocol qdcxx_quote doctest::doctest)
    set_target_properties(qdcxx_raw_tape_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME raw_tape_java_conformance COMMAND qdcxx_raw_tape_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/raw-tape/expected")
    add_executable(qdcxx_quote_value_tests tests/conformance/quote_value_tests.cpp)
    target_link_libraries(qdcxx_quote_value_tests PRIVATE qdcxx_core qdcxx_encoding doctest::doctest)
    set_target_properties(qdcxx_quote_value_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME quote_java_values COMMAND qdcxx_quote_value_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/quote/expected/primitives.tsv")
    add_executable(qdcxx_quote_tests tests/conformance/quote_tests.cpp)
    target_link_libraries(qdcxx_quote_tests PRIVATE qdcxx_quote qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_quote_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME quote_java_tape COMMAND qdcxx_quote_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/quote/expected")
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
    add_executable(qdcxx_event_flags_tests tests/conformance/event_flags_tests.cpp)
    target_link_libraries(qdcxx_event_flags_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_event_flags_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_event_flags COMMAND qdcxx_event_flags_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected/flags")
    if(ORG_TTLDTOR_QDCXX_BUILD_TAPE_INSPECT)
        add_executable(qdcxx_profile_cli_tests tests/conformance/profile_cli_tests.cpp)
        target_link_libraries(qdcxx_profile_cli_tests PRIVATE qdcxx_encoding qdcxx_profile qdcxx_quote)
        add_executable(qdcxx_profile_io_tests tests/conformance/profile_io_tests.cpp)
        target_link_libraries(qdcxx_profile_io_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_profile_cli_tests qdcxx_profile_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME profile_inspection_io COMMAND qdcxx_profile_io_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/events/mapping/expected/default")
        add_test(NAME profile_inspection_cli COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DCOMPARATOR=$<TARGET_FILE:qdcxx_profile_cli_tests>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/profile-cli" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/profile_cli.cmake")
        add_executable(qdcxx_field_type_cli_tests tests/conformance/field_type_cli_tests.cpp)
        add_executable(qdcxx_field_type_io_tests tests/conformance/field_type_io_tests.cpp)
        target_link_libraries(qdcxx_field_type_io_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_field_type_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME field_type_inspection_io COMMAND qdcxx_field_type_io_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/encoding/field-types/expected")
        target_link_libraries(qdcxx_field_type_cli_tests PRIVATE qdcxx_encoding)
        set_target_properties(qdcxx_field_type_cli_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME field_type_inspection_cli COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DCOMPARATOR=$<TARGET_FILE:qdcxx_field_type_cli_tests>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/field-type-cli" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/field_type_cli.cmake")
        add_executable(qdcxx_raw_tape_cli_tests tests/conformance/raw_tape_cli_tests.cpp)
        target_link_libraries(qdcxx_raw_tape_cli_tests PRIVATE qdcxx_encoding)
        set_target_properties(qdcxx_raw_tape_cli_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME raw_tape_inspection_cli COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DCOMPARATOR=$<TARGET_FILE:qdcxx_raw_tape_cli_tests>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/raw-tape-cli" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/raw_tape_cli.cmake")
        add_executable(qdcxx_raw_tape_io_tests tests/conformance/raw_tape_io_tests.cpp)
        target_link_libraries(qdcxx_raw_tape_io_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_raw_tape_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME raw_tape_inspection_io COMMAND qdcxx_raw_tape_io_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/raw-tape/expected")
        add_executable(qdcxx_quote_io_tests tests/conformance/quote_io_tests.cpp)
        target_link_libraries(qdcxx_quote_io_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_quote_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME quote_inspection_io COMMAND qdcxx_quote_io_tests)
        add_test(NAME quote_inspection_cli COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/quote_cli.cmake")
        add_executable(qdcxx_tape_io_tests tests/conformance/tape_io_tests.cpp)
        target_link_libraries(qdcxx_tape_io_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_tape_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME tape_inspection_io COMMAND qdcxx_tape_io_tests)
        add_executable(qdcxx_wire_inventory_schema_tests tests/conformance/wire_inventory_schema_tests.cpp)
        target_link_libraries(qdcxx_wire_inventory_schema_tests PRIVATE qdcxx_tape_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_wire_inventory_schema_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME wire_inventory_schema COMMAND qdcxx_wire_inventory_schema_tests "${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME wire_inventory_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/wire_inventory_cli.cmake")
        add_test(NAME tape_inspection_cli COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>" "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/tape_cli.cmake")
        add_test(NAME tape_argument_contract COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_tape_inspect>"
            "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/tape-argument-contract"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape/expected/constrained.qtp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/refactor/cli_arguments.cmake")
    endif()
    add_executable(qdcxx_tape_tests tests/conformance/tape_reader_tests.cpp)
    target_link_libraries(qdcxx_tape_tests PRIVATE qdcxx_protocol doctest::doctest)
    set_target_properties(qdcxx_tape_tests PROPERTIES CXX_EXTENSIONS OFF)
    add_test(NAME protocol_binary_tape COMMAND qdcxx_tape_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/tape-reader/expected")
    if(ORG_TTLDTOR_QDCXX_BUILD_QTP_INSPECT)
        add_executable(qdcxx_inspection_cli_tests tests/conformance/inspection_cli_tests.cpp)
        target_compile_features(qdcxx_inspection_cli_tests PRIVATE cxx_std_23)
        set_target_properties(qdcxx_inspection_cli_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME inspection_checker_contract COMMAND qdcxx_inspection_cli_tests selftest)
        add_test(NAME inspection_cli_negatives COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_qtp_inspect>" "-DCHECKER=$<TARGET_FILE:qdcxx_inspection_cli_tests>"
            "-DPROCESS_HELPER=$<TARGET_FILE:qdcxx_inspection_process_tests>"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection"
            "-DRUN_DIR=${CMAKE_CURRENT_BINARY_DIR}/inspection-cli-negatives"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/inspection_cli_negatives.cmake")
        add_executable(qdcxx_inspection_process_tests tests/conformance/inspection_process_tests.cpp)
        target_compile_features(qdcxx_inspection_process_tests PRIVATE cxx_std_23)
        set_target_properties(qdcxx_inspection_process_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME inspection_process_contract COMMAND qdcxx_inspection_process_tests
            "$<TARGET_FILE:qdcxx_qtp_inspect>"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection/expected/composed.qtp"
            "${CMAKE_CURRENT_BINARY_DIR}/inspection-process")
        add_test(NAME qtp_argument_contract COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_qtp_inspect>"
            "-DRUN=${CMAKE_CURRENT_BINARY_DIR}/qtp-argument-contract"
            "-DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection/expected/composed.qtp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/refactor/cli_arguments.cmake")
        add_test(NAME offline_inspection_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_qtp_inspect>" "-DCHECKER=$<TARGET_FILE:qdcxx_inspection_cli_tests>"
            "-DPROCESS_HELPER=$<TARGET_FILE:qdcxx_inspection_process_tests>"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/offline-inspection"
            "-DRUN_DIR=${CMAKE_CURRENT_BINARY_DIR}/offline-cli"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/offline_cli.cmake")
        add_executable(qdcxx_offline_io_tests tests/conformance/offline_io_tests.cpp)
        target_link_libraries(qdcxx_offline_io_tests PRIVATE qdcxx_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_offline_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME offline_inspection_injected_io COMMAND qdcxx_offline_io_tests)
        add_test(NAME human_inspection_cli COMMAND "${CMAKE_COMMAND}"
            "-DPROGRAM=$<TARGET_FILE:qdcxx_qtp_inspect>" "-DCHECKER=$<TARGET_FILE:qdcxx_inspection_cli_tests>"
            "-DFIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/protocol/human-inspection"
            "-DRUN_DIR=${CMAKE_CURRENT_BINARY_DIR}/human-cli"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/human_cli.cmake")
        add_executable(qdcxx_human_io_tests tests/conformance/human_io_tests.cpp)
        target_link_libraries(qdcxx_human_io_tests PRIVATE qdcxx_inspect_tool doctest::doctest)
        set_target_properties(qdcxx_human_io_tests PROPERTIES CXX_EXTENSIONS OFF)
        add_test(NAME human_inspection_injected_io COMMAND qdcxx_human_io_tests)
    endif()
endif()
