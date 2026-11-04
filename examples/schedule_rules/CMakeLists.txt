# Copyright (c) 2026 ttldtor.
# SPDX-License-Identifier: BSL-1.0
add_executable(qdcxx_schedule_example main.cpp)
target_link_libraries(qdcxx_schedule_example PRIVATE qdcxx_schedule)
target_compile_options(qdcxx_schedule_example PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
set_target_properties(qdcxx_schedule_example PROPERTIES OUTPUT_NAME qdcxx-schedule-example CXX_EXTENSIONS OFF)
