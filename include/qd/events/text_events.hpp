// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_mapping.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** Owning text message, independent of the decoded record and its lifetime. */
struct TextMessage {
    std::u16string symbol;              ///< Exact logical symbol.
    std::int64_t eventTime = 0;         ///< FIELD metadata milliseconds, independent of business time.
    std::uint64_t timeSequence = 0;     ///< Signed seconds in the high word and packed sequence in the low word.
    std::optional<std::u16string> text; ///< Nullable exact UTF-16 payload; empty and null differ.

    /** @return Business time in milliseconds, including packed millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Low 22 sequence bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
};

/** Owning lasting text configuration. */
struct TextConfiguration {
    std::u16string symbol;              ///< Exact logical symbol.
    std::int64_t eventTime = 0;         ///< FIELD metadata milliseconds, independent of business time.
    std::uint64_t timeSequence = 0;     ///< Signed seconds in the high word and packed sequence in the low word.
    std::int32_t version = 0;           ///< Signed source version; zero when the field is absent.
    std::optional<std::u16string> text; ///< Nullable exact UTF-16 payload; empty and null differ.

    /** @return Business time in milliseconds, including packed millisecond bits. */
    [[nodiscard]] std::int64_t getTime() const noexcept;
    /** @return Low 22 sequence bits. */
    [[nodiscard]] std::int32_t getSequence() const noexcept;
};

/** Construct the standard TextMessage record.
 * @param limits Finite complete schema limits.
 * @return Owned schema or original schema error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultTextMessageSchema(core::SchemaLimits limits);
/** Construct the standard TextConfiguration record with enabled optional fields.
 * @param limits Finite complete schema limits.
 * @return Owned schema or original schema error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError>
defaultTextConfigurationSchema(core::SchemaLimits limits);

/** Immutable complete-shape TextMessage mapper. */
class PreparedTextMessage {
    EventMapping mapping_;
    explicit PreparedTextMessage(EventMapping mapping);

    public:
    /** Bind required Time, Sequence and Text fields without retaining the schema.
     * @param schema Complete validated declarations.
     * @param localId Explicit record identity.
     * @param limits Finite mapping preparation limits.
     * @return Owning mapper or original mapping error.
     */
    [[nodiscard]] static std::expected<PreparedTextMessage, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed exact context while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** Validate complete owned input and copy an independent event.
     * @param symbol Borrowed logical symbol.
     * @param values Complete record values, including unselected extras.
     * @param limits Finite symbol, string and aggregate UTF-16 ownership limits.
     * @param eventTime Independent FIELD metadata milliseconds.
     * @return Complete owning event or original mapping/resource error.
     */
    [[nodiscard]] std::expected<TextMessage, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0) const;
    /** Validate complete borrowed input and copy an independent event.
     * @param symbol Borrowed logical symbol.
     * @param values Complete record values, including unselected extras.
     * @param limits Finite symbol, string and aggregate UTF-16 ownership limits.
     * @param eventTime Independent FIELD metadata milliseconds.
     * @return Complete owning event or original mapping/resource error.
     */
    [[nodiscard]] std::expected<TextMessage, MappingError> mapView(std::u16string_view symbol,
                                                                   BorrowedRecordValueView values,
                                                                   EventValueLimits limits,
                                                                   std::int64_t eventTime = 0) const;
};

/** Immutable complete-shape TextConfiguration mapper. */
class PreparedTextConfiguration {
    EventMapping mapping_;
    explicit PreparedTextConfiguration(EventMapping mapping);

    public:
    /** Bind required Text and optional Time, Sequence and Version fields.
     * @param schema Complete validated declarations.
     * @param localId Explicit record identity.
     * @param limits Finite mapping preparation limits.
     * @return Owning mapper or original mapping error.
     */
    [[nodiscard]] static std::expected<PreparedTextConfiguration, MappingError>
    prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits);
    /** @return Borrowed exact context while this mapper remains alive and unmoved. */
    [[nodiscard]] const core::RecordContext &context() const noexcept;
    /** @return Optional selected Version field index for the shared TICKER policy. */
    [[nodiscard]] std::optional<std::size_t> versionField() const noexcept;
    /** Validate complete owned input and copy an independent event.
     * @param symbol Borrowed logical symbol.
     * @param values Complete record values, including unselected extras.
     * @param limits Finite symbol, string and aggregate UTF-16 ownership limits.
     * @param eventTime Independent FIELD metadata milliseconds.
     * @return Complete owning event or original mapping/resource error.
     */
    [[nodiscard]] std::expected<TextConfiguration, MappingError>
    map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits, std::int64_t eventTime = 0) const;
    /** Validate complete borrowed input and copy an independent event.
     * @param symbol Borrowed logical symbol.
     * @param values Complete record values, including unselected extras.
     * @param limits Finite symbol, string and aggregate UTF-16 ownership limits.
     * @param eventTime Independent FIELD metadata milliseconds.
     * @return Complete owning event or original mapping/resource error.
     */
    [[nodiscard]] std::expected<TextConfiguration, MappingError> mapView(std::u16string_view symbol,
                                                                         BorrowedRecordValueView values,
                                                                         EventValueLimits limits,
                                                                         std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
