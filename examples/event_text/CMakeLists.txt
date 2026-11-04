# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
add_executable(qdcxx_event_text_example main.cpp)
target_link_libraries(qdcxx_event_text_example PRIVATE qdcxx_event_text)
target_compile_options(qdcxx_event_text_example PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_event_text_example PROPERTIES OUTPUT_NAME qdcxx-event-text-example CXX_EXTENSIONS OFF)