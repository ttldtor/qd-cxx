// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/events/message.hpp"

namespace org::ttldtor::qdcxx::events {
/** Owning Configuration value; its signed event version never chooses or executes a codec. */
struct Configuration {
    std::u16string symbol;           ///< Exact application symbol without market/regional rewriting.
    std::int64_t eventTime = 0;      ///< Independent FIELD metadata milliseconds, zero if unspecified.
    std::int32_t version = 0;        ///< First physical signed int slot; absent Version means zero.
    SerializedAttachment attachment; ///< Exact owning nullable SERIAL_OBJECT body.
    /** @return Borrowed immutable serialization, valid while this event is alive and unchanged. */
    [[nodiscard]] const SerializedAttachment &getAttachment() const noexcept;
    /** @return Signed event-version metadata; unrelated to codec format versions. */
    [[nodiscard]] std::int32_t getVersion() const noexcept;
    /** @return Exact independent event timestamp milliseconds. */
    [[nodiscard]] std::int64_t getEventTime() const noexcept;
};

/** Construct the explicit standard Configuration schema with Version enabled.
 * @param limits Finite complete schema limits, including zero.
 * @return Owning record0 with Version8 and Configuration249, or original typed schema failure.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultConfigurationSchema(core::SchemaLimits limits);

/** Immutable exact-schema projection, independent of receiving version-state and codec execution. */
class PreparedConfiguration {
    EventMapping mapping_;
    explicit PreparedConfiguration(EventMapping mapping);

    public:
    /** Bind required Configuration249 and optional Version scalar first-int-slot metadata.
     * @param schema Complete immutable local schema, borrowed only during preparation.
     * @param localId Explicit ordinary record position; aliases remain caller-defined schema lookups.
     * @param limits Finite shape/key/type/name/comparison allowances.
     * @return Independently owning mapper or typed preparation failure.
     */
    [[nodiscard]] static std::expected<PreparedConfiguration, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Exact owned origin/record context, borrowed while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Optional exact Version logical index for explicit TICKER policy; absence disables filtering. */
    [[nodiscard]] std::optional<std::size_t> versionField() const noexcept;
    /** Validate complete values, then produce independent bytes and signed event metadata.
     * @param symbol Exact application symbol, borrowed during this call.
     * @param values Complete context-tagged local values, including validated unselected extras.
     * @param limits Finite copied symbol/body/combined byte bounds.
     * @param eventTime Independent FIELD metadata milliseconds, default zero.
     * @return Complete owning Configuration or typed shape/resource/allocation failure.
     */
    [[nodiscard]] std::expected<Configuration, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                                 AttachmentEventLimits limits,
                                                                 std::int64_t eventTime = 0) const;
    /** Project borrowed fields directly; this operation does not apply version filtering.
     * @param symbol Exact application symbol, borrowed during this call.
     * @param values Complete borrowed field views/context, valid through return.
     * @param limits Finite copied symbol/body/combined byte bounds.
     * @param eventTime Independent FIELD metadata milliseconds, default zero.
     * @return Complete owning Configuration or typed shape/resource/allocation failure.
     */
    [[nodiscard]] std::expected<Configuration, MappingError> mapView(std::u16string_view symbol,
                                                                     BorrowedRecordValueView values,
                                                                     AttachmentEventLimits limits,
                                                                     std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
