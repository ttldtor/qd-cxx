// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/core/wide_decimal.hpp"
#include "qd/events/event_mapping.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace org::ttldtor::qdcxx::events {
/** ASCII local field names, shared by the fixed schema adapter and presentation. */
inline constexpr std::array<std::string_view, 8> QUOTE_FIELD_NAMES{"Bid.Time", "Bid.Exchange", "Bid.Price", "Bid.Size",
                                                                   "Ask.Time", "Ask.Exchange", "Ask.Price", "Ask.Size"};
/** Full identities in the default composite Quote logical field order. */
inline constexpr std::array<core::SerialFieldType, 8> QUOTE_FIELD_TYPES{
    core::SerialFieldType::TIME_SECONDS, core::SerialFieldType::UTF_CHAR,     core::SerialFieldType::WIDE_DECIMAL,
    core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::TIME_SECONDS, core::SerialFieldType::UTF_CHAR,
    core::SerialFieldType::WIDE_DECIMAL, core::SerialFieldType::WIDE_DECIMAL};

/** Owned Quote projection; no event hierarchy or transport dependency. */
struct Quote {
    std::u16string symbol;         ///< Exact owned UTF-16 symbol, without normalization.
    std::int64_t bidTimeMs;        ///< Signed milliseconds; TIME_SECONDS expands by 1000, TIME_MILLIS is unchanged.
    char16_t bidExchangeCode;      ///< Low UTF-16 unit of the preserved incoming code point.
    core::WideDecimal bidPrice{0}; ///< Generated WideDecimal accessor projection; incoming wide bits stay unchanged.
    core::WideDecimal bidSize{0};  ///< Decimal size projection, preserving supported fractional values.
    std::int64_t askTimeMs;        ///< Signed milliseconds; absent optional local time projects zero.
    char16_t askExchangeCode;      ///< Low UTF-16 unit of the preserved incoming code point.
    core::WideDecimal askPrice{0}; ///< Decimal price projection, including NaN and infinities.
    core::WideDecimal askSize{0};  ///< Decimal size projection; the local field remains required.
    std::int64_t timeMs;           ///< Maximum bid/ask floored to seconds plus unsigned packed milliseconds, wrapping.
    std::int64_t eventTime = 0;    ///< Explicit caller metadata; the legacy mapper always leaves zero.
    std::int32_t sequence = 0;     ///< Low 22 Sequence bits; zero on the legacy mapping path.
    std::int32_t timeNanoPart =
        0; ///< Raw signed nanoseconds within a millisecond; absent means zero, without clamping.
    std::optional<std::array<detail::BusinessNumericSnapshot, 4>> businessNumericSnapshot{};
    ///< Inline projection state; leave absent for manual aggregates. Raw edits invalidate each cached value separately.

    /** Read the Java business bid price without a LONG-to-wide rounding step.
     * @return Captured direct value while bidPrice.raw is unchanged; otherwise its current raw decimal view.
     */
    [[nodiscard]] double getBidPrice() const noexcept;
    /** Read the Java business bid size as binary64.
     * @return Captured direct value while bidSize.raw is unchanged; otherwise its current raw decimal view.
     */
    [[nodiscard]] double getBidSizeAsDouble() const noexcept;
    /** Read the Java business ask price without a LONG-to-wide rounding step.
     * @return Captured direct value while askPrice.raw is unchanged; otherwise its current raw decimal view.
     */
    [[nodiscard]] double getAskPrice() const noexcept;
    /** Read the Java business ask size as binary64.
     * @return Captured direct value while askSize.raw is unchanged; otherwise its current raw decimal view.
     */
    [[nodiscard]] double getAskSizeAsDouble() const noexcept;
};

/** Mapping failure, separate from wire parsing and transport. */
enum class QuoteError {
    INVALID_SHAPE,     ///< Count, exact tag or raw range violates the default Quote contract.
    RESOURCE_LIMIT,    ///< Explicit owned symbol-unit budget is exceeded.
    ALLOCATION_FAILURE ///< Symbol copy cannot be allocated; no partial event escapes.
};

/** Construct the smallest explicit default-composite local schema.
 * @param limits Mandatory finite schema bounds, including zero.
 * @return Owned validated local ID 0/Quote with eight fields, or schema/resource/allocation error.
 */
[[nodiscard]] std::expected<core::LocalSchema, core::SchemaError> defaultQuoteSchema(core::SchemaLimits limits);

/** Map already decoded exact values without retaining decoder/schema storage.
 * @param symbol Borrowed exact symbol, unchanged during the call.
 * @param fields Eight local-order exact values, unchanged during the call.
 * @param symbolUnits Mandatory maximum units copied into the result.
 * @return Owned Quote or first shape/resource/allocation error; decimal raw bits are preserved.
 */
[[nodiscard]] std::expected<Quote, QuoteError>
mapQuote(std::u16string_view symbol, std::span<const core::FieldValue> fields, std::size_t symbolUnits);

/** Immutable Quote field selections tied to one validated local record context. */
class PreparedQuote {
    EventMapping mapping_;
    char16_t recordExchange_; ///< Immutable syntactic record exchange; absent business exchange fields use this code.

    /** Take an independently owned validated field plan.
     * @param mapping Complete immutable Quote field plan.
     * @param recordExchange Regional record suffix, or zero for a composite/custom record.
     */
    explicit PreparedQuote(EventMapping mapping, char16_t recordExchange);

    public:
    /** Resolve Quote keys and exact numeric/raw/time types declared in FIELD-CONVERSIONS.
     * @param schema Explicit validated source schema; no storage is borrowed after return.
     * @param localId Selected ordinary local record identity.
     * @param limits Finite preparation budgets; record name consumes name units and three conservative scans.
     * @return Immutable prepared mapping or atomic preparation error.
     */
    [[nodiscard]] static std::expected<PreparedQuote, MappingError> prepare(const core::LocalSchema &schema,
                                                                            std::int32_t localId, MappingLimits limits);

    /** Inspect the retained schema/record identity.
     * @return Borrowed context until this mapping is moved, assigned or destroyed; a copy owns its identity.
     */
    [[nodiscard]] const core::RecordContext &context() const noexcept;

    /** Validate and project without modifying or retaining generic values.
     * @param symbol Physical QD UTF-16 symbol; a regional record restores its suffix on the owned result.
     * @param values Context-tagged complete local values, including any extra fields.
     * @param symbolUnits Mandatory maximum UTF-16 units copied into the owned result.
     * @param eventTime Explicit metadata in milliseconds, independent of business fields.
     * @return Complete owned Quote or context/value/resource/allocation error.
     */
    [[nodiscard]] std::expected<Quote, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                         std::size_t symbolUnits, std::int64_t eventTime = 0) const;

    /** Project complete borrowed fields without constructing intermediate owning field values.
     * @param symbol Physical QD UTF-16 symbol; a regional record restores its suffix on the owned result.
     * @param values Context-tagged complete field views; all source storage remains valid during this call.
     * @param symbolUnits Maximum UTF-16 symbol units copied into the owned result.
     * @param eventTime Explicit metadata milliseconds, independent of indexed identity and business fields.
     * @return Complete independently owned Quote or context/value/resource/allocation error; inputs are not retained.
     */
    [[nodiscard]] std::expected<Quote, MappingError> mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                             std::size_t symbolUnits, std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
