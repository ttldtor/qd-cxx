// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_payload.hpp"
#include "qd/collectors/stream.hpp"

namespace org::ttldtor::qdcxx::collectors::detail {
/** Borrowed homogeneous window; both spans expire when the collector offer is invalidated. */
struct StreamRangeDelivery {
    std::span<const protocol::LiveRecordView> records;
    std::span<const std::shared_ptr<const core::detail::SharedRecordPayload>> payloads;
    std::uint64_t revision;
    std::uint64_t bindingGeneration;
};

/** Retain the payloads of an already prepared prefix before its atomic acknowledgement. */
struct StreamRetentionAccess {
    /** Prepare an eligible homogeneous prefix; absence leaves the public fallback offer untouched. */
    static std::expected<std::optional<StreamRangeDelivery>, StreamError>
    prepareRange(StreamCollector &collector, std::size_t maximum, StreamWork &work);
    static void retainPrepared(const StreamCollector &collector, std::span<const StreamDelivery> offers,
                               std::span<std::shared_ptr<const core::detail::SharedRecordPayload>> storage) noexcept;
};
} // namespace org::ttldtor::qdcxx::collectors::detail
