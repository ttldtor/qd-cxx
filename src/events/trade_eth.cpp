// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/trade_eth.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "trade_projection.hpp"
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>

namespace org::ttldtor::qdcxx::events {
std::int64_t TradeETH::getTime() const noexcept {
    return detail::tradeTime(timeSequence);
}

std::int64_t TradeETH::getTimeNanos() const noexcept {
    return detail::tradeTimeNanos(timeSequence, timeNanoPart);
}

std::int32_t TradeETH::getSequence() const noexcept {
    return detail::tradeSequence(timeSequence);
}

double TradeETH::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, 0);
}

double TradeETH::getSizeAsDouble() const noexcept {
    return detail::businessValue(size, businessNumericSnapshot, 1);
}

std::int64_t TradeETH::getSize() const noexcept {
    return detail::tradeInteger(getSizeAsDouble());
}

double TradeETH::getChange() const noexcept {
    return detail::businessValue(change, businessNumericSnapshot, 2);
}

double TradeETH::getDayVolumeAsDouble() const noexcept {
    return detail::businessValue(dayVolume, businessNumericSnapshot, 3);
}

std::int64_t TradeETH::getDayVolume() const noexcept {
    return detail::tradeInteger(getDayVolumeAsDouble());
}

double TradeETH::getDayTurnover() const noexcept {
    return detail::businessValue(dayTurnover, businessNumericSnapshot, 4);
}

Direction TradeETH::getTickDirection() const noexcept {
    return detail::tradeDirection(flags);
}

bool TradeETH::isExtendedTradingHours() const noexcept {
    return detail::tradeExtendedHours(flags);
}

std::expected<core::LocalSchema, core::SchemaError> defaultTradeETHSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array<std::u16string_view, 12> names{u"ETHLast.Time",     u"ETHLast.Sequence", u"Last.TimeNanoPart",
                                                        u"ETHLast.Exchange", u"TradeId",          u"ETHLast.Price",
                                                        u"ETHLast.Size",     u"ETHLast.Change",   u"DayId",
                                                        u"ETHVolume",        u"ETHDayTurnover",   u"ETHLast.Flags"};
    constexpr std::array<std::u16string_view, 12> aliases{u"Time",    u"Sequence",  u"TimeNanoPart", u"ExchangeCode",
                                                          u"TradeId", u"Price",     u"Size",         u"Change",
                                                          u"DayId",   u"DayVolume", u"DayTurnover",  u"Flags"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::COMPACT_INT,  Type::UTF_CHAR,
                               Type::LONG,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::DATE,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT};
    try {
        core::LocalRecord record{0, u"TradeETH", false, {}, {}};
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

PreparedTradeETH::PreparedTradeETH(EventMapping mapping, char16_t recordExchange)
    : mapping_(std::move(mapping)), recordExchange_(recordExchange) {
}

std::expected<PreparedTradeETH, MappingError> PreparedTradeETH::prepare(const core::LocalSchema &schema,
                                                                        std::int32_t localId, MappingLimits limits) {
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
    constexpr std::array<std::u16string_view, 2> time{u"ETHLast.Time", u"Time"};
    constexpr std::array<std::u16string_view, 2> sequence{u"ETHLast.Sequence", u"Sequence"};
    constexpr std::array<std::u16string_view, 2> nano{u"Last.TimeNanoPart", u"TimeNanoPart"};
    constexpr std::array<std::u16string_view, 2> exchange{u"ETHLast.Exchange", u"ExchangeCode"};
    constexpr std::array<std::u16string_view, 1> id{u"TradeId"};
    constexpr std::array<std::u16string_view, 2> price{u"ETHLast.Price", u"Price"};
    constexpr std::array<std::u16string_view, 2> size{u"ETHLast.Size", u"Size"};
    constexpr std::array<std::u16string_view, 2> change{u"ETHLast.Change", u"Change"};
    constexpr std::array<std::u16string_view, 1> day{u"DayId"};
    constexpr std::array<std::u16string_view, 2> volume{u"ETHVolume", u"DayVolume"};
    constexpr std::array<std::u16string_view, 2> turnover{u"ETHDayTurnover", u"DayTurnover"};
    constexpr std::array<std::u16string_view, 2> flags{u"ETHLast.Flags", u"Flags"};
    const std::array<FieldRequest, 12> requests{{{time, detail::RAW_INT_TYPES, false},
                                                 {sequence, detail::RAW_INT_TYPES, false},
                                                 {nano, detail::RAW_INT_TYPES, false},
                                                 {exchange, detail::RAW_INT_TYPES, false},
                                                 {id, longTypes, false},
                                                 {price, detail::NUMERIC_TYPES, true},
                                                 {size, detail::NUMERIC_TYPES, true},
                                                 {change, detail::NUMERIC_TYPES, false},
                                                 {day, detail::RAW_INT_TYPES, false},
                                                 {volume, detail::NUMERIC_TYPES, false},
                                                 {turnover, detail::NUMERIC_TYPES, false},
                                                 {flags, detail::RAW_INT_TYPES, true}}};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    return PreparedTradeETH(std::move(*mapping), market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedTradeETH::context() const noexcept {
    return mapping_.context();
}

std::expected<TradeETH, MappingError> PreparedTradeETH::map(std::u16string_view symbol, RecordValueView values,
                                                            std::size_t symbolUnits, std::int64_t eventTime) const {
    const auto validation = mapping_.validate(values);

    if (!validation)
        return std::unexpected(validation.error());

    return detail::projectTradeLike<TradeETH>(symbol, values.values, mapping_.selections(), symbolUnits, eventTime,
                                              recordExchange_, detail::TRADE_ETH_LAYOUT);
}

std::expected<TradeETH, MappingError> PreparedTradeETH::mapView(std::u16string_view symbol,
                                                                BorrowedRecordValueView values, std::size_t symbolUnits,
                                                                std::int64_t eventTime) const {
    const auto validation = mapping_.validateView(values);

    if (!validation)
        return std::unexpected(validation.error());

    return detail::projectTradeLike<TradeETH>(symbol, values.values, mapping_.selections(), symbolUnits, eventTime,
                                              recordExchange_, detail::TRADE_ETH_LAYOUT);
}
} // namespace org::ttldtor::qdcxx::events
