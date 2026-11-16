// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_payload.hpp"
#include "qd/protocol/live_session.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {
/** Retain only immutable local bytes; the event and its snapshot remain serialized. */
struct SharedRecordAccess {
    [[nodiscard]] static bool hasSharedRecords(const LiveEvent &event) noexcept;
    [[nodiscard]] static std::span<const std::shared_ptr<const core::detail::SharedRecordPayload>>
    payloads(const LiveEvent &event, std::span<const LiveRecordView> records) noexcept;
    [[nodiscard]] static std::shared_ptr<const core::detail::SharedRecordPayload>
    retain(const LiveEvent &event, const LiveRecordView &record) noexcept;
};
} // namespace org::ttldtor::qdcxx::protocol::detail
