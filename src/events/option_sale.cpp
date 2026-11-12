// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/option_sale.hpp"

#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/short_string.hpp"
#include "qd/events/time_helpers.hpp"

#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class Field : std::size_t {
    INDEX,
    TIME,
    SEQUENCE,
    TIME_NANO_PART,
    EXCHANGE_CODE,
    PRICE,
    SIZE,
    BID_PRICE,
    ASK_PRICE,
    SALE_CONDITIONS,
    FLAGS,
    UNDERLYING_PRICE,
    VOLATILITY,
    DELTA,
    OPTION_SYMBOL
};

constexpr std::size_t position(Field field) noexcept {
    return std::to_underlying(field);
}

constexpr std::array<std::u16string_view, 15> KEYS{
    u"Index",      u"Time",     u"Sequence",    u"TimeNanoPart",           u"ExchangeCode", u"Price",
    u"Size",       u"BidPrice", u"AskPrice",    u"ExchangeSaleConditions", u"Flags",        u"UnderlyingPrice",
    u"Volatility", u"Delta",    u"OptionSymbol"};
constexpr std::array NUMERIC_FIELDS{
    Field::PRICE,      Field::SIZE, Field::BID_PRICE, Field::ASK_PRICE, Field::UNDERLYING_PRICE,
    Field::VOLATILITY, Field::DELTA};
constexpr std::array STRING_TYPES{core::SerialFieldType::UTF_CHAR_ARRAY, core::SerialFieldType::STRING};
constexpr std::int32_t TRADE_THROUGH_MASK = 0xff;
constexpr std::int32_t TRADE_THROUGH_SHIFT = 8;
constexpr std::int32_t SIDE_MASK = 3;
constexpr std::int32_t SIDE_SHIFT = 5;
constexpr std::int32_t SPREAD_LEG = 1 << 4;
constexpr std::int32_t EXTENDED_HOURS = 1 << 3;
constexpr std::int32_t VALID_TICK = 1 << 2;
constexpr std::int32_t TYPE_MASK = 3;

/** Count decoded units of a sign-extended Java int passed to ShortString.decode.
 * @param value Exact signed Java int.
 * @return Output UTF-16 units, at most eight.
 */
std::size_t shortStringUnits(std::int32_t value) noexcept {
    auto bits = static_cast<std::uint64_t>(static_cast<std::int64_t>(value));
    std::size_t result = 0;

    while (bits) {
        result += (bits & 0xff) != 0;
        bits >>= 8;
    }

    return result;
}

/** Project validated source fields without retaining caller storage.
 * @tparam Value Owning or borrowed complete field representation.
 * @param symbol Physical source symbol.
 * @param values Complete validated source values.
 * @param selected Ordered immutable field selections.
 * @param limits Finite output text allowances.
 * @param eventTime Independent FIELD event time.
 * @param eventFlags Indexed-event flags.
 * @return Independent event or a resource/allocation refusal.
 */
template <class Value>
std::expected<OptionSale, MappingError>
project(std::u16string_view symbol, std::span<const Value> values, std::span<const std::optional<std::size_t>> selected,
        EventValueLimits limits, std::int64_t eventTime, std::int32_t eventFlags) {
    const auto raw = [&](Field field) {
        const auto index = selected[position(field)];

        return index ? detail::projectPhysicalInt(values[*index].type, values[*index].raw) : 0;
    };
    const auto &optionField = values[*selected[position(Field::OPTION_SYMBOL)]];
    const auto optionSymbol = optionField.units ? std::optional<std::u16string_view>(*optionField.units) : std::nullopt;
    const auto conditionsUnits = shortStringUnits(raw(Field::SALE_CONDITIONS));
    const auto optionUnits = optionSymbol ? optionSymbol->size() : 0;

    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits ||
        conditionsUnits > limits.stringUnits || optionUnits > limits.stringUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    auto remaining = limits.totalUnits - symbol.size();

    for (const auto units : {conditionsUnits, optionUnits}) {
        if (units > remaining) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }

        remaining -= units;
    }

    try {
        OptionSale result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index = raw(Field::INDEX);
        result.timeSequence = std::bit_cast<std::int64_t>(
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(Field::TIME))) << 32) |
            static_cast<std::uint32_t>(raw(Field::SEQUENCE)));
        result.timeNanoPart = raw(Field::TIME_NANO_PART);
        result.exchangeCode = static_cast<char16_t>(static_cast<std::uint32_t>(raw(Field::EXCHANGE_CODE)) & 0xffff);
        const std::array destinations{&result.price,           &result.size,       &result.bidPrice, &result.askPrice,
                                      &result.underlyingPrice, &result.volatility, &result.delta};
        std::array<detail::BusinessNumericSnapshot, NUMERIC_FIELDS.size()> snapshots{};

        for (std::size_t index = 0; index < destinations.size(); ++index) {
            const auto &value = values[*selected[position(NUMERIC_FIELDS[index])]];
            *destinations[index] = detail::projectPresentDecimal(value.type, value.raw);
            snapshots[index] = {destinations[index]->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
        }

        result.businessNumericSnapshot = snapshots;
        result.exchangeSaleConditions = decodeShortString(raw(Field::SALE_CONDITIONS));
        result.flags = raw(Field::FLAGS);

        if (optionSymbol) {
            result.optionSymbol.emplace(*optionSymbol);
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

const IndexedEventSource &OptionSale::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t OptionSale::getIndex() const noexcept {
    return index;
}

std::int64_t OptionSale::getTimeSequence() const noexcept {
    return timeSequence;
}

std::int64_t OptionSale::getTime() const noexcept {
    return getTimeFromTimeSequence(timeSequence);
}

std::int64_t OptionSale::getTimeNanos() const noexcept {
    return getNanosFromMillisAndNanoPart(getTime(), timeNanoPart);
}

std::int32_t OptionSale::getTimeNanoPart() const noexcept {
    return timeNanoPart;
}

std::int32_t OptionSale::getSequence() const noexcept {
    return getSequenceFromTimeSequence(timeSequence);
}

char16_t OptionSale::getExchangeCode() const noexcept {
    return exchangeCode;
}

double OptionSale::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, 0);
}

double OptionSale::getSize() const noexcept {
    return detail::businessValue(size, businessNumericSnapshot, 1);
}

double OptionSale::getBidPrice() const noexcept {
    return detail::businessValue(bidPrice, businessNumericSnapshot, 2);
}

double OptionSale::getAskPrice() const noexcept {
    return detail::businessValue(askPrice, businessNumericSnapshot, 3);
}

double OptionSale::getUnderlyingPrice() const noexcept {
    return detail::businessValue(underlyingPrice, businessNumericSnapshot, 4);
}

double OptionSale::getVolatility() const noexcept {
    return detail::businessValue(volatility, businessNumericSnapshot, 5);
}

double OptionSale::getDelta() const noexcept {
    return detail::businessValue(delta, businessNumericSnapshot, 6);
}

const std::optional<std::u16string> &OptionSale::getExchangeSaleConditions() const noexcept {
    return exchangeSaleConditions;
}

const std::optional<std::u16string> &OptionSale::getOptionSymbol() const noexcept {
    return optionSymbol;
}

char16_t OptionSale::getTradeThroughExempt() const noexcept {
    return static_cast<char16_t>(getBits(flags, TRADE_THROUGH_MASK, TRADE_THROUGH_SHIFT));
}

Side OptionSale::getAggressorSide() const noexcept {
    return marketEnumValue(marketEnumFromCode<Side>(getBits(flags, SIDE_MASK, SIDE_SHIFT))).value_or(Side::UNDEFINED);
}

bool OptionSale::isSpreadLeg() const noexcept {
    return (flags & SPREAD_LEG) != 0;
}

bool OptionSale::isExtendedTradingHours() const noexcept {
    return (flags & EXTENDED_HOURS) != 0;
}

bool OptionSale::isValidTick() const noexcept {
    return (flags & VALID_TICK) != 0;
}

TimeAndSaleType OptionSale::getType() const noexcept {
    return marketEnumValue(marketEnumFromCode<TimeAndSaleType>(getBits(flags, TYPE_MASK, 0)))
        .value_or(TimeAndSaleType::NEW);
}

bool OptionSale::isNew() const noexcept {
    return getType() == TimeAndSaleType::NEW;
}

bool OptionSale::isCorrection() const noexcept {
    return getType() == TimeAndSaleType::CORRECTION;
}

bool OptionSale::isCancel() const noexcept {
    return getType() == TimeAndSaleType::CANCEL;
}

std::expected<core::LocalSchema, core::SchemaError> defaultOptionSaleSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::COMPACT_INT,  Type::TIME_SECONDS,  Type::SEQUENCE,     Type::UTF_CHAR,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,  Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::SHORT_STRING, Type::COMPACT_INT,   Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::UTF_CHAR_ARRAY};
    try {
        core::LocalRecord record{0, u"OptionSale", true, {}, {}, core::IndexedTimeLowInt32{0}};
        std::size_t fieldIndex = 0;

        for (std::size_t index = 0; index < KEYS.size(); ++index) {
            if (index == position(Field::TIME_NANO_PART)) {
                continue;
            }

            record.fields.push_back({std::to_underlying(types[fieldIndex])});
            record.lookups.push_back({std::u16string(KEYS[index]), fieldIndex});
            ++fieldIndex;
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedOptionSale::PreparedOptionSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)) {
}

std::expected<PreparedOptionSale, MappingError>
PreparedOptionSale::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        const auto isNumeric = std::ranges::find(NUMERIC_FIELDS, static_cast<Field>(index)) != NUMERIC_FIELDS.end();
        const auto types = index == position(Field::OPTION_SYMBOL)
                               ? std::span<const core::SerialFieldType>(STRING_TYPES)
                           : isNumeric ? std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES)
                                       : std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES);
        requests[index] = {std::span(KEYS).subspan(index, 1), types, index != position(Field::TIME_NANO_PART)};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedOptionSale(std::move(*mapping),
                              schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout);
}

const core::RecordContext &PreparedOptionSale::context() const noexcept {
    return mapping_.context();
}

std::expected<OptionSale, MappingError> PreparedOptionSale::map(std::u16string_view symbol, RecordValueView values,
                                                                EventValueLimits limits, std::int64_t eventTime,
                                                                std::int32_t eventFlags,
                                                                std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (const auto valid = indexedRemovalTime
                               ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                               : mapping_.validate(values);
        !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags);
}

std::expected<OptionSale, MappingError>
PreparedOptionSale::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
                            std::int64_t eventTime, std::int32_t eventFlags,
                            std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (const auto valid = indexedRemovalTime
                               ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                               : mapping_.validateView(values);
        !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
