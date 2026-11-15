// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include <cstddef>

namespace org::ttldtor::qdcxx::mux::detail {
/** Number of fair visits to each configured client slot before source admission can reopen. */
inline constexpr std::size_t QUEUE_PHASES = 5;
} // namespace org::ttldtor::qdcxx::mux::detail
