// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_payload.hpp"
#include "qd/collectors/stream.hpp"

namespace org::ttldtor::qdcxx::collectors::detail {
/** Retain the payloads of an already prepared prefix before its atomic acknowledgement. */
struct StreamRetentionAccess {
    static void retainPrepared(const StreamCollector &collector, std::span<const StreamDelivery> offers,
                               std::span<std::shared_ptr<const core::detail::SharedRecordPayload>> storage) noexcept;
};
} // namespace org::ttldtor::qdcxx::collectors::detail
