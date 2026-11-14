// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/mux/queue.hpp"

namespace org::ttldtor::qdcxx::mux::detail {
/** One participating queue and its complete replacement, borrowed for a serialized owner call. */
struct QueueReplacement {
    MuxQueue *queue;
    const MuxHandle *handle;
    std::span<const core::SubscriptionRequest> requests;
    QueueWork *work;
};

struct QueueReplaceAccess {
    /** Synchronous pre-publication observer; may stage a copy or refuse without publishing any candidate.
     * Candidate spans are valid only during the callback. The callback may not reenter any participating queue.
     */
    using PrepareAggregate = std::expected<void, QueueFailure> (*)(void *, std::size_t,
                                                                   std::span<const core::SubscriptionRequest>);

    /** Prepare every lane, then publish all before invoking callbacks or yielding to recovery. */
    [[nodiscard]] static std::expected<void, QueueFailure> replace(std::span<const QueueReplacement> replacements,
                                                                   std::size_t selectors, std::uint64_t revision,
                                                                   PrepareAggregate prepareAggregate = nullptr,
                                                                   void *argument = nullptr);
};
} // namespace org::ttldtor::qdcxx::mux::detail
