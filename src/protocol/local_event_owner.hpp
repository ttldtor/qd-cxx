// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {

/** One private synthetic source epoch with the usual counted event backing. */
class LocalEventOwner {
    LiveSnapshot epoch_;
    std::size_t eventBytes_;

    LocalEventOwner(LiveSnapshot epoch, std::size_t eventBytes) noexcept;

    public:
    [[nodiscard]] static std::expected<LocalEventOwner, LiveError> create(LiveLimits limits, LiveWork &work);

    /** Own one local TICKER record and optional explicit event-time metadata. */
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    quote(const core::RecordContext &context, std::u16string_view symbol, std::span<const core::FieldValue> fields,
          std::optional<FieldEventTime> eventTime, LiveWork &work) const;

    /** Own one local record with its selected contract and physical indexed controls. */
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    record(LiveDataMessage type, std::int32_t localId, const core::RecordContext &context, std::u16string_view symbol,
           std::span<const core::FieldValue> fields, std::int32_t flags, std::optional<std::int64_t> removalTime,
           std::optional<FieldEventTime> eventTime, LiveWork &work) const;
};

} // namespace org::ttldtor::qdcxx::protocol::detail
