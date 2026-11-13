// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/subscription_state_access.hpp"
#include "qd/collectors/stream.hpp"

namespace org::ttldtor::qdcxx::collectors::detail {
/** Stage and publish the union for a serialized STREAM collector. */
struct StreamReplaceAccess {
    using Candidate = core::detail::SubscriptionStateAccess::PreparedReplace;

    [[nodiscard]] static std::expected<Candidate, core::SubscriptionError>
    prepare(StreamCollector &owner, std::span<const core::SubscriptionRequest> requests, core::SubscriptionWork &work);

    [[nodiscard]] static std::expected<core::SubscriptionChange, core::SubscriptionError>
    commit(StreamCollector &owner, Candidate candidate) noexcept;
};
} // namespace org::ttldtor::qdcxx::collectors::detail
