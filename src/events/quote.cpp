// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/quote.hpp"

#include "decimal_projection.hpp"
#include "qd/events/market_event_symbols.hpp"

#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/// Conversion from signed wire seconds to event milliseconds.
constexpr std::int64_t MILLISECONDS_PER_SECOND = 1000;
/// Exchange-code projection retains the low UTF-16 unit.
constexpr std::uint64_t UTF16_UNIT_MASK = 0xffff;
} // namespace

double Quote::getBidPrice() const noexcept {
    return detail::businessValue(bidPrice, businessNumericSnapshot, 0);
}

double Quote::getBidSizeAsDouble() const noexcept {
    return detail::businessValue(bidSize, businessNumericSnapshot, 1);
}

double Quote::getAskPrice() const noexcept {
    return detail::businessValue(askPrice, businessNumericSnapshot, 2);
}

double Quote::getAskSizeAsDouble() const noexcept {
    return detail::businessValue(askSize, businessNumericSnapshot, 3);
}

std::expected<core::LocalSchema, core::SchemaError> defaultQuoteSchema(core::SchemaLimits limits) {
    constexpr std::array<std::u16string_view, 8> PROPERTY_NAMES{u"BidTime",  u"BidExchangeCode", u"BidPrice",
                                                                u"BidSize",  u"AskTime",         u"AskExchangeCode",
                                                                u"AskPrice", u"AskSize"};
    try {
        core::LocalRecord record{0, u"Quote", false, {}, {}};

        for (std::size_t i = 0; i < QUOTE_FIELD_TYPES.size(); ++i) {
            const std::u16string localName(QUOTE_FIELD_NAMES[i].begin(), QUOTE_FIELD_NAMES[i].end());
            record.fields.push_back({std::to_underlying(QUOTE_FIELD_TYPES[i])});
            record.lookups.push_back({localName, i});
            record.lookups.push_back({u"Quote." + localName, i});
            record.lookups.push_back({std::u16string(PROPERTY_NAMES[i]), i});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::QUOTE_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

std::expected<Quote, QuoteError> mapQuote(std::u16string_view symbol, std::span<const core::FieldValue> fields,
                                          std::size_t symbolUnits) {
    if (fields.size() != QUOTE_FIELD_TYPES.size()) {
        return std::unexpected(QuoteError::INVALID_SHAPE);
    }

    for (std::size_t i = 0; i < fields.size(); ++i) {
        const auto &value = fields[i];

        if (value.type != QUOTE_FIELD_TYPES[i] || !core::validFieldValue(value)) {
            return std::unexpected(QuoteError::INVALID_SHAPE);
        }
    }

    if (symbol.size() > symbolUnits) {
        return std::unexpected(QuoteError::RESOURCE_LIMIT);
    }

    try {
        const auto bid = fields[0].raw * MILLISECONDS_PER_SECOND;
        const auto ask = fields[4].raw * MILLISECONDS_PER_SECOND;

        return Quote{std::u16string(symbol),
                     bid,
                     static_cast<char16_t>(static_cast<std::uint64_t>(fields[1].raw) & UTF16_UNIT_MASK),
                     {fields[2].raw},
                     {fields[3].raw},
                     ask,
                     static_cast<char16_t>(static_cast<std::uint64_t>(fields[5].raw) & UTF16_UNIT_MASK),
                     {fields[6].raw},
                     {fields[7].raw},
                     std::max(bid, ask)};
    } catch (const std::bad_alloc &) {
        return std::unexpected(QuoteError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(QuoteError::ALLOCATION_FAILURE);
    }
}

PreparedQuote::PreparedQuote(EventMapping mapping, char16_t recordExchange)
    : mapping_(std::move(mapping)), recordExchange_(recordExchange) {
}

std::expected<PreparedQuote, MappingError> PreparedQuote::prepare(const core::LocalSchema &schema, std::int32_t localId,
                                                                  MappingLimits limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    // The existing suffix helper can inspect the name three times when attributes are present.
    constexpr std::size_t RECORD_NAME_SCANS = 3;

    if (record.name.size() > limits.nameUnits || record.name.size() > limits.totalNameUnits ||
        record.name.size() > limits.comparisonWork / RECORD_NAME_SCANS) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= record.name.size();
    limits.comparisonWork -= record.name.size() * RECORD_NAME_SCANS;

    using Type = core::SerialFieldType;
    constexpr std::array timeTypes{Type::TIME_SECONDS, Type::TIME_MILLIS};
    constexpr auto exchangeTypes = detail::RAW_INT_TYPES;
    constexpr auto numericTypes = detail::NUMERIC_TYPES;
    constexpr auto sequenceTypes = detail::RAW_INT_TYPES;
    constexpr auto nanoTypes = detail::RAW_INT_TYPES;
    constexpr std::array<std::u16string_view, 2> bidTime{u"Bid.Time", u"BidTime"};
    constexpr std::array<std::u16string_view, 2> bidExchange{u"Bid.Exchange", u"BidExchangeCode"};
    constexpr std::array<std::u16string_view, 2> bidPrice{u"Bid.Price", u"BidPrice"};
    constexpr std::array<std::u16string_view, 2> bidSize{u"Bid.Size", u"BidSize"};
    constexpr std::array<std::u16string_view, 2> askTime{u"Ask.Time", u"AskTime"};
    constexpr std::array<std::u16string_view, 2> askExchange{u"Ask.Exchange", u"AskExchangeCode"};
    constexpr std::array<std::u16string_view, 2> askPrice{u"Ask.Price", u"AskPrice"};
    constexpr std::array<std::u16string_view, 2> askSize{u"Ask.Size", u"AskSize"};
    constexpr std::array<std::u16string_view, 1> sequence{u"Sequence"};
    constexpr std::array<std::u16string_view, 1> nano{u"TimeNanoPart"};
    const std::array<FieldRequest, 10> requests{{{bidTime, timeTypes, false},
                                                 {bidExchange, exchangeTypes, false},
                                                 {bidPrice, numericTypes, true},
                                                 {bidSize, numericTypes, true},
                                                 {askTime, timeTypes, false},
                                                 {askExchange, exchangeTypes, false},
                                                 {askPrice, numericTypes, true},
                                                 {askSize, numericTypes, true},
                                                 {sequence, sequenceTypes, false},
                                                 {nano, nanoTypes, false}}};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedQuote(std::move(*mapping), market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedQuote::context() const noexcept {
    return mapping_.context();
}

namespace {
/** Floor to whole seconds and add packed milliseconds with Java long wrapping, including signed64 boundaries.
 * @param time Signed business milliseconds before the packed Sequence override.
 * @param millis Unsigned high ten Sequence bits, preserved even above 999.
 * @return Signed two's-complement result; no signed C++ arithmetic overflows.
 */
std::int64_t quoteTime(std::int64_t time, std::uint32_t millis) noexcept {
    const auto remainder = time % MILLISECONDS_PER_SECOND;
    auto rounded = static_cast<std::uint64_t>(time) - static_cast<std::uint64_t>(remainder);

    if (remainder < 0) {
        rounded -= MILLISECONDS_PER_SECOND;
    }

    return std::bit_cast<std::int64_t>(rounded + millis);
}

/** Project validated Quote fields without owning an intermediate field representation.
 * @tparam Field Canonical core FieldValue or FieldValueView.
 * @param symbol Borrowed exact UTF-16 symbol.
 * @param fields Complete validated local fields in schema order.
 * @param selections Immutable prepared Quote property positions.
 * @param symbolUnits Maximum symbol units copied into the result.
 * @param eventTime Explicit caller metadata milliseconds, independent of business fields.
 * @param recordExchange Syntactic regional record suffix, used for output identity and absent exchange fields.
 * @return Complete owned Quote or resource/allocation error.
 */
template <typename Field>
std::expected<Quote, MappingError> projectQuoteFields(std::u16string_view symbol, std::span<const Field> fields,
                                                      std::span<const std::optional<std::size_t>> selections,
                                                      std::size_t symbolUnits, std::int64_t eventTime,
                                                      char16_t recordExchange) {
    if (symbol.size() > symbolUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    constexpr std::size_t REGIONAL_SUFFIX_UNITS = 2;

    if (recordExchange && !market_symbols::hasExchangeCode(symbol) &&
        symbolUnits - symbol.size() < REGIONAL_SUFFIX_UNITS) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    constexpr unsigned SEQUENCE_BITS = 22;
    constexpr std::uint32_t SEQUENCE_MASK = (std::uint32_t{1} << SEQUENCE_BITS) - 1;
    const auto raw = [&](std::size_t selection) -> std::int64_t {
        if (!selections[selection]) {
            return 0;
        }

        const auto &field = fields[*selections[selection]];

        return detail::projectPhysicalInt(field.type, field.raw);
    };
    std::array<detail::BusinessNumericSnapshot, 4> businessValues{};
    const auto decimal = [&](std::size_t selection, std::size_t numericIndex) -> core::WideDecimal {
        const auto &field = fields[*selections[selection]];
        const auto raw = field.raw;
        const auto projected = detail::projectPresentDecimal(field.type, raw);
        businessValues[numericIndex] = {projected.raw, detail::projectPresentBusinessDouble(field.type, raw)};

        return projected;
    };
    const auto time = [&](std::size_t selection) -> std::int64_t {
        if (!selections[selection]) {
            return 0;
        }

        const auto &field = fields[*selections[selection]];

        return field.type == core::SerialFieldType::TIME_MILLIS ? field.raw : field.raw * MILLISECONDS_PER_SECOND;
    };
    const auto exchange = [&](std::size_t selection) -> char16_t {
        return selections[selection]
                   ? static_cast<char16_t>(static_cast<std::uint64_t>(raw(selection)) & UTF16_UNIT_MASK)
                   : recordExchange;
    };
    // Selection numbers follow the request order above, independently of schema field positions.
    const auto bidTime = time(0);
    const auto askTime = time(4);
    const auto packedSequence = static_cast<std::uint32_t>(raw(8));
    try {
        auto eventSymbol =
            recordExchange ? *market_symbols::changeExchangeCode(symbol, recordExchange) : std::u16string(symbol);

        Quote result{std::move(eventSymbol),
                     bidTime,
                     exchange(1),
                     decimal(2, 0),
                     decimal(3, 1),
                     askTime,
                     exchange(5),
                     decimal(6, 2),
                     decimal(7, 3),
                     quoteTime(std::max(bidTime, askTime), packedSequence >> SEQUENCE_BITS),
                     eventTime,
                     static_cast<std::int32_t>(packedSequence & SEQUENCE_MASK),
                     static_cast<std::int32_t>(raw(9))};
        result.businessNumericSnapshot = businessValues;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

std::expected<Quote, MappingError> PreparedQuote::map(std::u16string_view symbol, RecordValueView values,
                                                      std::size_t symbolUnits, std::int64_t eventTime) const {
    const auto validation = mapping_.validate(values);

    if (!validation) {
        return std::unexpected(validation.error());
    }

    return projectQuoteFields(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}

std::expected<Quote, MappingError> PreparedQuote::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                          std::size_t symbolUnits, std::int64_t eventTime) const {
    const auto validation = mapping_.validateView(values);

    if (!validation) {
        return std::unexpected(validation.error());
    }

    return projectQuoteFields(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}
} // namespace org::ttldtor::qdcxx::events
