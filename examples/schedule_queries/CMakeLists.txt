# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
add_executable(qdcxx_schedule_queries_example main.cpp)
target_link_libraries(qdcxx_schedule_queries_example PRIVATE qdcxx_schedule)
target_compile_options(qdcxx_schedule_queries_example PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_schedule_queries_example PROPERTIES OUTPUT_NAME qdcxx-schedule-queries CXX_EXTENSIONS OFF)
