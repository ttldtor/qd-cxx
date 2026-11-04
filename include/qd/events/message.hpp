// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/event_mapping.hpp"
#include "qd/events/serialized_attachment.hpp"

namespace org::ttldtor::qdcxx::events {
/** Finite ownership limits for one complete serialized-attachment event. */
struct AttachmentEventLimits {
    std::size_t symbolUnits;  ///< Maximum copied UTF-16 symbol units.
    std::size_t payloadBytes; ///< Maximum copied nullable attachment body bytes.
    std::size_t totalBytes;   ///< Combined symbol bytes and body bytes, excluding wrapper/allocator bookkeeping.
};

/** Owning Message event. Serialized bytes never claim decoded Java Object semantics. */
struct Message {
    std::u16string symbol;           ///< Exact application symbol, with no market/regional rewriting.
    std::int64_t eventTime = 0;      ///< Independent FIELD metadata milliseconds; zero if unspecified.
    SerializedAttachment attachment; ///< Exact owning nullable SERIAL_OBJECT body.
    /** @return Borrowed immutable serialization, valid while this event is alive and unchanged. */
    [[nodiscard]] const SerializedAttachment &getAttachment() const noexcept;
    /** @return Exact independent event timestamp milliseconds. */
    [[nodiscard]] std::int64_t getEventTime() const noexcept;
};

/** Construct the explicit standard Message schema.
 * @param limits Finite complete schema limits, including zero.
 * @return Owning record0 with required full249 Message field, or original typed schema failure.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultMessageSchema(core::SchemaLimits limits);

/** Immutable exact-schema Message projection; no source declaration or field storage is borrowed. */
class PreparedMessage {
    EventMapping mapping_;
    explicit PreparedMessage(EventMapping mapping);

    public:
    /** Bind required Message lookup to exact SERIAL_OBJECT249.
     * @param schema Complete immutable local schema, borrowed only during preparation.
     * @param localId Explicit ordinary record position; names never select a type automatically.
     * @param limits Finite shape/key/type/name/comparison allowances.
     * @return Independently owning mapper or typed preparation failure.
     */
    [[nodiscard]] static std::expected<PreparedMessage, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Exact owned origin/record context, borrowed while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate the complete shape, then copy one independent event.
     * @param symbol Exact application symbol, borrowed only during this call.
     * @param values Complete local field values with exact source context; extras are validated.
     * @param limits Finite copied symbol/body/combined byte bounds.
     * @param eventTime Independent FIELD metadata milliseconds, default zero.
     * @return Complete owning Message or typed shape/resource/allocation failure; no partial result escapes.
     */
    [[nodiscard]] std::expected<Message, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                           AttachmentEventLimits limits,
                                                           std::int64_t eventTime = 0) const;
    /** Project borrowed source fields directly into the final owning result.
     * @param symbol Exact application symbol, borrowed only during this call.
     * @param values Complete borrowed field views/context, valid through return.
     * @param limits Finite copied symbol/body/combined byte bounds.
     * @param eventTime Independent FIELD metadata milliseconds, default zero.
     * @return Complete owning Message or typed shape/resource/allocation failure.
     */
    [[nodiscard]] std::expected<Message, MappingError> mapView(std::u16string_view symbol,
                                                               BorrowedRecordValueView values,
                                                               AttachmentEventLimits limits,
                                                               std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
