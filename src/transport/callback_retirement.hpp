// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include <atomic>
#include <cstddef>

namespace org::ttldtor::qdcxx::transport::detail {
/** Pure high-level packet fence; no callable, runtime, native owner or progress dependency. */
struct CallbackRetirementFence {
    std::atomic<std::size_t> outstanding{0}; ///< Includes preparation, post, execution and final capture retirement.
    std::atomic<bool> cancelQueuedRequested{false}; ///< Owner cancels not-started work before waiting.
};
} // namespace org::ttldtor::qdcxx::transport::detail
