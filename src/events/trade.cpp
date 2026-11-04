// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/trade.hpp"
#include "decimal_projection.hpp"
#include "qd/events/market_event_symbols.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::uint32_t SEQUENCE_MASK = (std::uint32_t{1} << 22) - 1;
constexpr std::int64_t MILLIS_PER_SECOND = 1000;
constexpr std::uint64_t NANOS_PER_MILLI = 1000000;

/** Apply Java's defined narrowing conversion without an out-of-range C++ cast.
 * @param value Source binary64 business size or volume.
 * @return Truncated finite integer, saturated endpoint, or zero for NaN.
 */
std::int64_t javaLong(double value) noexcept {
    if (std::isnan(value))
        return 0;

    if (value >= 0x1p63)
        return std::numeric_limits<std::int64_t>::max();

    if (value <= -0x1p63)
        return std::numeric_limits<std::int64_t>::min();

    return static_cast<std::int64_t>(value);
}
} // namespace

std::int64_t Trade::getTime() const noexcept {
    const auto seconds = timeSequence >> 32;
    const auto millis = (static_cast<std::uint64_t>(timeSequence) >> 22) & 0x3ff;

    return seconds * MILLIS_PER_SECOND + static_cast<std::int64_t>(millis);
}

std::int64_t Trade::getTimeNanos() const noexcept {
    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(getTime()) * NANOS_PER_MILLI +
                                       static_cast<std::uint64_t>(timeNanoPart));
}

std::int32_t Trade::getSequence() const noexcept {
    return static_cast<std::uint32_t>(timeSequence) & SEQUENCE_MASK;
}

double Trade::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, 0);
}

double Trade::getSizeAsDouble() const noexcept {
    return detail::businessValue(size, businessNumericSnapshot, 1);
}

std::int64_t Trade::getSize() const noexcept {
    return javaLong(getSizeAsDouble());
}

double Trade::getChange() const noexcept {
    return detail::businessValue(change, businessNumericSnapshot, 2);
}

double Trade::getDayVolumeAsDouble() const noexcept {
    return detail::businessValue(dayVolume, businessNumericSnapshot, 3);
}

std::int64_t Trade::getDayVolume() const noexcept {
    return javaLong(getDayVolumeAsDouble());
}

double Trade::getDayTurnover() const noexcept {
    return detail::businessValue(dayTurnover, businessNumericSnapshot, 4);
}

Direction Trade::getTickDirection() const noexcept {
    const auto direction = (static_cast<std::uint32_t>(flags) >> 1) & 7;

    return direction <= std::to_underlying(Direction::UP) ? static_cast<Direction>(direction) : Direction::UNDEFINED;
}

bool Trade::isExtendedTradingHours() const noexcept {
    return (flags & 1) != 0;
}

std::expected<core::LocalSchema, core::SchemaError> defaultTradeSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array<std::u16string_view, 13> names{
        u"Last.Time",  u"Last.Sequence", u"Last.TimeNanoPart", u"Last.Exchange", u"TradeId",
        u"Last.Price", u"Last.Size",     u"Last.Tick",         u"Last.Change",   u"DayId",
        u"Volume",     u"DayTurnover",   u"Last.Flags"};
    constexpr std::array<std::u16string_view, 13> aliases{
        u"Time", u"Sequence", u"TimeNanoPart", u"ExchangeCode", u"TradeId",     u"Price", u"Size",
        u"Tick", u"Change",   u"DayId",        u"DayVolume",    u"DayTurnover", u"Flags"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::COMPACT_INT,  Type::UTF_CHAR,
                               Type::LONG,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT,
                               Type::WIDE_DECIMAL, Type::DATE,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::COMPACT_INT};
    try {
        core::LocalRecord record{0, u"Trade", false, {}, {}};
        record.fields.reserve(names.size());
        record.lookups.reserve(names.size() * 2);

        for (std::size_t index = 0; index < names.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(names[index]), index});

            if (aliases[index] != names[index])
                record.lookups.push_back({std::u16string(aliases[index]), index});
        }

        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedTrade::PreparedTrade(EventMapping mapping, char16_t recordExchange)
    : mapping_(std::move(mapping)), recordExchange_(recordExchange) {
}

std::expected<PreparedTrade, MappingError> PreparedTrade::prepare(const core::LocalSchema &schema, std::int32_t localId,
                                                                  MappingLimits limits) {
    if (!schema.recordContext(localId).valid())
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    constexpr std::size_t RECORD_NAME_SCANS = 3;

    if (record.name.size() > limits.nameUnits || record.name.size() > limits.totalNameUnits ||
        record.name.size() > limits.comparisonWork / RECORD_NAME_SCANS)
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    limits.totalNameUnits -= record.name.size();
    limits.comparisonWork -= record.name.size() * RECORD_NAME_SCANS;
    using Type = core::SerialFieldType;
    constexpr std::array longTypes{Type::LONG, Type::WIDE_DECIMAL, Type::TIME_MILLIS};
    constexpr std::array<std::u16string_view, 2> time{u"Last.Time", u"Time"};
    constexpr std::array<std::u16string_view, 2> sequence{u"Last.Sequence", u"Sequence"};
    constexpr std::array<std::u16string_view, 2> nano{u"Last.TimeNanoPart", u"TimeNanoPart"};
    constexpr std::array<std::u16string_view, 2> exchange{u"Last.Exchange", u"ExchangeCode"};
    constexpr std::array<std::u16string_view, 1> id{u"TradeId"};
    constexpr std::array<std::u16string_view, 2> price{u"Last.Price", u"Price"};
    constexpr std::array<std::u16string_view, 2> size{u"Last.Size", u"Size"};
    constexpr std::array<std::u16string_view, 2> tick{u"Last.Tick", u"Tick"};
    constexpr std::array<std::u16string_view, 2> change{u"Last.Change", u"Change"};
    constexpr std::array<std::u16string_view, 1> day{u"DayId"};
    constexpr std::array<std::u16string_view, 2> volume{u"Volume", u"DayVolume"};
    constexpr std::array<std::u16string_view, 1> turnover{u"DayTurnover"};
    constexpr std::array<std::u16string_view, 2> flags{u"Last.Flags", u"Flags"};
    const std::array<FieldRequest, 13> requests{{{time, detail::RAW_INT_TYPES, false},
                                                 {sequence, detail::RAW_INT_TYPES, false},
                                                 {nano, detail::RAW_INT_TYPES, false},
                                                 {exchange, detail::RAW_INT_TYPES, false},
                                                 {id, longTypes, false},
                                                 {price, detail::NUMERIC_TYPES, true},
                                                 {size, detail::NUMERIC_TYPES, true},
                                                 {tick, detail::RAW_INT_TYPES, false},
                                                 {change, detail::NUMERIC_TYPES, false},
                                                 {day, detail::RAW_INT_TYPES, false},
                                                 {volume, detail::NUMERIC_TYPES, false},
                                                 {turnover, detail::NUMERIC_TYPES, false},
                                                 {flags, detail::RAW_INT_TYPES, false}}};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    return PreparedTrade(std::move(*mapping), market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedTrade::context() const noexcept {
    return mapping_.context();
}

namespace {
/** Build a complete Trade from previously validated complete fields.
 * @tparam Field Canonical owned or borrowed field representation.
 * @param symbol Exact physical symbol, alive through this call.
 * @param fields Complete validated local-order fields.
 * @param selected Retained immutable property positions in request order.
 * @param symbolUnits Maximum owned symbol units.
 * @param eventTime Independent event metadata milliseconds.
 * @param exchange Syntactic record exchange, zero for composite.
 * @return Independent complete event or resource/allocation refusal.
 */
template <class Field>
std::expected<Trade, MappingError> projectTrade(std::u16string_view symbol, std::span<const Field> fields,
                                                std::span<const std::optional<std::size_t>> selected,
                                                std::size_t symbolUnits, std::int64_t eventTime, char16_t exchange) {
    if (symbol.size() > symbolUnits ||
        (exchange && !market_symbols::hasExchangeCode(symbol) && symbolUnits - symbol.size() < 2))
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    const auto raw = [&](std::size_t index) -> std::int32_t {
        if (!selected[index])
            return 0;

        const auto &field = fields[*selected[index]];

        return detail::projectPhysicalInt(field.type, field.raw);
    };
    std::array<detail::BusinessNumericSnapshot, 5> business{};
    const auto numeric = [&](std::size_t index, std::size_t property) -> core::WideDecimal {
        if (!selected[index])
            return {0};

        const auto &field = fields[*selected[index]];
        const auto wide = detail::projectPresentDecimal(field.type, field.raw);
        business[property] = {wide.raw, detail::projectPresentBusinessDouble(field.type, field.raw)};

        return wide;
    };
    try {
        Trade result;
        result.symbol = exchange ? *market_symbols::changeExchangeCode(symbol, exchange) : std::u16string(symbol);
        result.eventTime = eventTime;
        const auto packed =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(0))) << 32) | static_cast<std::uint32_t>(raw(1));
        result.timeSequence = std::bit_cast<std::int64_t>(packed);
        result.timeNanoPart = raw(2);
        result.exchangeCode =
            selected[3] ? static_cast<char16_t>(static_cast<std::uint32_t>(raw(3)) & 0xffff) : exchange;
        result.tradeId = selected[4] ? fields[*selected[4]].raw : 0;
        result.price = numeric(5, 0);
        result.size = numeric(6, 1);
        result.change = numeric(8, 2);
        result.dayId = raw(9);
        result.dayVolume = numeric(10, 3);
        result.dayTurnover = numeric(11, 4);
        result.flags = raw(12);

        if (result.getTickDirection() == Direction::UNDEFINED) {
            const auto tick = raw(7);

            if (tick == 1 || tick == 2) {
                const auto direction = tick == 1 ? Direction::ZERO_UP : Direction::ZERO_DOWN;
                result.flags = std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(result.flags) & ~0xeU) |
                                                           (static_cast<std::uint32_t>(direction) << 1));
            }
        }

        result.businessNumericSnapshot = business;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

std::expected<Trade, MappingError> PreparedTrade::map(std::u16string_view symbol, RecordValueView values,
                                                      std::size_t symbolUnits, std::int64_t eventTime) const {
    const auto validation = mapping_.validate(values);

    if (!validation)
        return std::unexpected(validation.error());

    return projectTrade(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}

std::expected<Trade, MappingError> PreparedTrade::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                          std::size_t symbolUnits, std::int64_t eventTime) const {
    const auto validation = mapping_.validateView(values);

    if (!validation)
        return std::unexpected(validation.error());

    return projectTrade(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}
} // namespace org::ttldtor::qdcxx::events
