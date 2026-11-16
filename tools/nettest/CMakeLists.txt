# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0

if(TARGET qdcxx_transport AND TARGET qdcxx_schema_json)
    add_executable(qdcxx_nettest main.cpp session.cpp)
    target_link_libraries(qdcxx_nettest PRIVATE qdcxx_transport qdcxx_schema_json)
    target_compile_options(qdcxx_nettest PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    if(WIN32)
        target_compile_definitions(qdcxx_nettest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
    set_target_properties(qdcxx_nettest PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-nettest)
    install(TARGETS qdcxx_nettest RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    if(BUILD_TESTING)
        add_test(NAME nettest_help COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_nettest>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/HelpTest.cmake")
        set_tests_properties(nettest_help PROPERTIES TIMEOUT 20)
        add_test(NAME nettest_startup COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_nettest>"
            "-DRUN=${CMAKE_CURRENT_BINARY_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/StartupTest.cmake")
        set_tests_properties(nettest_startup PROPERTIES TIMEOUT 15)
    endif()
endif()

if(TARGET qdcxx_feed)
    add_executable(qdcxx_quote_stream_nettest quote_stream_main.cpp)
    target_link_libraries(qdcxx_quote_stream_nettest PRIVATE qdcxx_feed)
    target_compile_options(qdcxx_quote_stream_nettest PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set_target_properties(qdcxx_quote_stream_nettest PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-quote-stream-nettest)

    add_executable(qdcxx_event_nettest event_main.cpp)
    target_link_libraries(qdcxx_event_nettest PRIVATE qdcxx_feed)
    target_compile_options(qdcxx_event_nettest PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set_target_properties(qdcxx_event_nettest PROPERTIES CXX_EXTENSIONS OFF OUTPUT_NAME qdcxx-event-nettest)
    install(TARGETS qdcxx_event_nettest RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

    if(BUILD_TESTING)
        add_test(NAME quote_stream_nettest_help COMMAND qdcxx_quote_stream_nettest --help)
        add_test(NAME quote_stream_nettest_help_ru COMMAND qdcxx_quote_stream_nettest --help-ru)
        set_tests_properties(quote_stream_nettest_help quote_stream_nettest_help_ru PROPERTIES TIMEOUT 20)
        add_test(NAME event_nettest_help COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:qdcxx_event_nettest>"
            "-DEVENT_MODE=ON" -P "${CMAKE_CURRENT_SOURCE_DIR}/HelpTest.cmake")
        set_tests_properties(event_nettest_help PROPERTIES TIMEOUT 20)
    endif()
endif()
