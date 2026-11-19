// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "../core/shared_record_block.hpp"
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

    /** Retain pure physical accounting, without retaining the serialized event or session.
     * @param event Existing admitted owner.
     * @return Atomic byte-domain state, or an empty pointer for an empty event.
     */
    [[nodiscard]] static std::shared_ptr<core::detail::RecordByteState> physicalState(const LiveEvent &event) noexcept;
};
} // namespace org::ttldtor::qdcxx::protocol::detail
