// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/protocol/live_session.hpp"

namespace org::ttldtor::qdcxx::protocol::detail {

/** Borrowed local STREAM row for one bounded event owner. */
struct LocalStreamRow {
    std::int32_t localId;
    core::RecordContext context;
    std::u16string_view symbol;
    std::span<const core::FieldValueView> fields;
};

/** One private synthetic source epoch with the usual counted event backing. */
class LocalEventOwner {
    LiveSnapshot epoch_;
    std::size_t eventBytes_;

    LocalEventOwner(LiveSnapshot epoch, std::size_t eventBytes) noexcept;

    template <typename Field>
    static void appendRecord(LiveEvent::State &event, std::int32_t localId, const core::RecordContext &context,
                             std::u16string_view symbol, std::span<const Field> fields, std::int32_t flags,
                             std::optional<std::int64_t> removalTime, std::optional<FieldEventTime> eventTime,
                             LiveWork &work);

    /** Adopt validated values into an ordinary local owner.
     * @tparam Field Borrowed or owned tagged representation.
     * @param type Data family carried by the row.
     * @param localId Validated local record ID.
     * @param context Owning provenance of that local record.
     * @param symbol Exact borrowed symbol.
     * @param fields Validated tagged fields in local order.
     * @param flags Physical indexed control flags.
     * @param removalTime Signed physical removal key, if present.
     * @param eventTime Inherited local event time, if present.
     * @param work Mutable work allowance.
     * @return Owned local row or storage/work failure.
     */
    template <typename Field>
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    copyRecord(LiveDataMessage type, std::int32_t localId, const core::RecordContext &context,
               std::u16string_view symbol, std::span<const Field> fields, std::int32_t flags,
               std::optional<std::int64_t> removalTime, std::optional<FieldEventTime> eventTime, LiveWork &work) const;

    public:
    [[nodiscard]] static std::expected<LocalEventOwner, LiveError> create(LiveLimits limits, LiveWork &work);

    /** Copy an ordered batch into one ordinary STREAM owner. */
    [[nodiscard]] std::expected<LiveEvent, LiveError> stream(std::span<const LocalStreamRow> rows, LiveWork &work) const;

    /** Own one local TICKER record and optional explicit event-time metadata. */
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    quote(const core::RecordContext &context, std::u16string_view symbol, std::span<const core::FieldValue> fields,
          std::optional<FieldEventTime> eventTime, LiveWork &work) const;

    /** Build an owning row from a source view, copying nullable logical field payloads into its bounded arena.
     * @param type Data family carried by the row.
     * @param localId Validated local record ID.
     * @param context Owning provenance of that local record.
     * @param symbol Exact borrowed symbol.
     * @param fields Validated tagged fields in local order, borrowed through the call.
     * @param flags Validated indexed control flags.
     * @param removalTime Exact signed indexed removal key, if present.
     * @param work Mutable work allowance.
     * @return Owned local row or storage/work failure.
     */
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    record(LiveDataMessage type, std::int32_t localId, const core::RecordContext &context, std::u16string_view symbol,
           std::span<const core::FieldValueView> fields, std::int32_t flags, std::optional<std::int64_t> removalTime,
           LiveWork &work) const;

    /** Own one local record with its selected contract and physical indexed controls. */
    [[nodiscard]] std::expected<LiveEvent, LiveError>
    record(LiveDataMessage type, std::int32_t localId, const core::RecordContext &context, std::u16string_view symbol,
           std::span<const core::FieldValue> fields, std::int32_t flags, std::optional<std::int64_t> removalTime,
           std::optional<FieldEventTime> eventTime, LiveWork &work) const;
};

} // namespace org::ttldtor::qdcxx::protocol::detail
