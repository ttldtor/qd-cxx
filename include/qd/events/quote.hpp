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
    std::int64_t bidTimeMs;        ///< Signed bid seconds expanded exactly to milliseconds.
    char16_t bidExchangeCode;      ///< Low UTF-16 unit of the preserved incoming code point.
    core::WideDecimal bidPrice;    ///< Exact raw decimal, with decimal/binary64 views.
    core::WideDecimal bidSize;     ///< Exact raw decimal size, preserving fractional values.
    std::int64_t askTimeMs;        ///< Signed ask seconds expanded exactly to milliseconds.
    char16_t askExchangeCode;      ///< Low UTF-16 unit of the preserved incoming code point.
    core::WideDecimal askPrice;    ///< Exact raw decimal, including NaN and infinities.
    core::WideDecimal askSize;     ///< Exact raw decimal size, including missing-field NaN.
    std::int64_t timeMs;           ///< Maximum bid/ask time plus the prepared mapping's packed milliseconds.
    std::int64_t eventTime = 0;    ///< Explicit caller metadata; the legacy mapper always leaves zero.
    std::int32_t sequence = 0;     ///< Low 22 Sequence bits; zero on the legacy mapping path.
    std::int32_t timeNanoPart = 0; ///< Signed TimeNanoPart value; zero on the legacy mapping path.
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

    /** Take an independently owned validated field plan.
     * @param mapping Complete immutable Quote field plan.
     */
    explicit PreparedQuote(EventMapping mapping);

    public:
    /** Resolve measured Quote keys and exact full types before event projection.
     * @param schema Explicit validated source schema; no storage is borrowed after return.
     * @param localId Selected ordinary local record identity.
     * @param limits Mandatory finite preparation budgets.
     * @return Immutable prepared mapping or atomic preparation error.
     */
    [[nodiscard]] static std::expected<PreparedQuote, MappingError> prepare(const core::LocalSchema &schema,
                                                                            std::int32_t localId, MappingLimits limits);

    /** Inspect the retained schema/record identity.
     * @return Borrowed context until this mapping is moved, assigned or destroyed; a copy owns its identity.
     */
    [[nodiscard]] const core::RecordContext &context() const noexcept;

    /** Validate and project without modifying or retaining generic values.
     * @param symbol Exact UTF-16 symbol, borrowed during this call.
     * @param values Context-tagged complete local values, including any extra fields.
     * @param symbolUnits Mandatory maximum UTF-16 units copied into the owned result.
     * @param eventTime Explicit metadata in milliseconds, independent of business fields.
     * @return Complete owned Quote or context/value/resource/allocation error.
     */
    [[nodiscard]] std::expected<Quote, MappingError> map(std::u16string_view symbol, RecordValueView values,
                                                         std::size_t symbolUnits, std::int64_t eventTime = 0) const;
};
} // namespace org::ttldtor::qdcxx::events
