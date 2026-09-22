// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/core/local_schema.hpp"
#include "qd/core/wide_decimal.hpp"
#include <array>
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

/** Owned provisional default-composite view; no event hierarchy or transport dependency. */
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
    std::int64_t timeMs;           ///< Maximum bid/ask millisecond time, without a subsecond component.
    std::int64_t eventTime = 0;    ///< Metadata is absent in the supported file profile.
    std::int32_t sequence = 0;     ///< Default schema has no Sequence field.
    std::int32_t timeNanoPart = 0; ///< Default schema has no TimeNanoPart field.
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
} // namespace org::ttldtor::qdcxx::events
