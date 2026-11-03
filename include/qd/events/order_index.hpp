// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_mapping.hpp"
#include "qd/events/order_source.hpp"

#include <cstdint>
#include <expected>

namespace org::ttldtor::qdcxx::events {
/** Test the reserved source IDs used to wrap non-order events.
 * @param sourceId Signed source ID, without interpreting any exchange or endpoint.
 * @return True exactly for IDs 1 through 9.
 */
[[nodiscard]] bool isSpecialOrderSourceId(std::int32_t sourceId) noexcept;

/** Extract the source ID encoded in an order index.
 * @param index All signed 64 bits; extraction also accepts otherwise invalid negative event indices.
 * @return High signed 16 bits for special sources, otherwise high signed 32 bits.
 */
[[nodiscard]] std::int32_t orderIndexSourceId(std::int64_t index) noexcept;

/** Resolve the source encoded in an order index.
 * @param index All signed 64 bits, borrowed by value.
 * @return Independently owned source identity.
 * @throws std::invalid_argument Encoded generic ID has no valid source name.
 * @throws std::bad_alloc Source name allocation fails.
 */
[[nodiscard]] OrderSource orderIndexSource(std::int64_t index);

/** Replace source bits with the exact OrderBase.setSource mask and shift semantics.
 * A previous special source preserves its low 48 bits, including exchange bits. A previous generic
 * source preserves only its low 32 bits. The result can retain those bits even when source families change.
 * @param index Previous raw order index; negative values are interpreted as raw bits.
 * @param source Resolved new source, borrowed for this allocation-free call.
 * @return New raw index; no event publication, nonnegative-index or exchange validation is performed.
 */
[[nodiscard]] std::int64_t changeOrderIndexSource(std::int64_t index, const OrderSource &source) noexcept;

/** Immutable source and Index selection for an explicitly chosen local record.
 * This component projects only the order identity, not a complete typed Order event. It keeps no
 * symbol, exchange, endpoint, transport or caller-owned schema/value storage.
 */
class PreparedOrderSource {
    EventMapping mapping_;
    OrderSource source_;

    /** Adopt complete independently owned preparation results.
     * @param mapping Validated context, full shape and required Index position.
     * @param source Resolved exact record suffix, or DEFAULT when no separator is present.
     */
    PreparedOrderSource(EventMapping mapping, OrderSource source) noexcept;

    public:
    /** Resolve source suffix and required exact COMPACT_INT Index before projecting values.
     * The last '#' selects a one-to-four-unit OrderSource name; absent '#' selects DEFAULT.
     * Record names are not restricted to Order: the caller explicitly chooses the record and semantics.
     * @param schema Immutable validated source, borrowed only during this call.
     * @param localId Explicit local record position; no scan for a matching event name occurs.
     * @param limits Finite EventMapping bounds; record-name units additionally consume nameUnits,
     * totalNameUnits and comparisonWork before field preparation. The retained logical source name is at most
     * four UTF-16 units, or seven for DEFAULT; implementation-selected string capacity and allocator metadata are
     * separate.
     * @return Independently owned plan, or atomic input/type/resource/allocation error.
     */
    [[nodiscard]] static std::expected<PreparedOrderSource, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);

    /** @return Borrowed immutable provenance until assignment, move-from or destruction. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Borrowed source until assignment, move-from or destruction. */
    [[nodiscard]] const OrderSource &source() const noexcept;

    /** Validate complete local provenance/shape and pack its signed wire Index as unsigned low 32 bits.
     * @param values Complete owning-value view, borrowed only during this allocation-free call.
     * @return Raw source-ID high32 / wire-Index low32 identity, or original context/value error.
     */
    [[nodiscard]] std::expected<std::int64_t, MappingError> mapIndex(RecordValueView values) const noexcept;
    /** Validate complete borrowed provenance/shape and pack its signed wire Index as unsigned low32.
     * @param values Complete borrowed fields, all payloads alive throughout this call.
     * @return Raw independently stored index, or original context/value error; no payload is retained.
     */
    [[nodiscard]] std::expected<std::int64_t, MappingError> mapIndexView(BorrowedRecordValueView values) const noexcept;
};
} // namespace org::ttldtor::qdcxx::events
