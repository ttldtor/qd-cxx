// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/nuam_trade.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "trade_projection.hpp"

#include <array>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::size_t TRADE_FIELD_COUNT = 13; ///< Inherited Trade mapper property count.

enum class NuamTradeField : std::size_t {
    STAT_TIME = TRADE_FIELD_COUNT,
    SIGNIFICANT_PRICE,
    LAST_PRICE_FOR_ALL,
    TRADE_COUNT,
    VWAP
};

/** @param field One additional NuamTrade property.
 * @return Its position in the complete mapping selection.
 */
constexpr std::size_t fieldIndex(NuamTradeField field) noexcept {
    return std::to_underlying(field);
}

constexpr std::size_t NUAM_TRADE_FIELD_COUNT = fieldIndex(NuamTradeField::VWAP) + 1;

/** Map validated complete fields to an independent NuamTrade.
 * @tparam Field Exact owning or borrowed field representation.
 * @param symbol Borrowed physical symbol.
 * @param fields Validated complete field values.
 * @param selections Ordered field selections.
 * @param symbolUnits Maximum output symbol units.
 * @param eventTime Independent FIELD metadata.
 * @param exchange Regional exchange or zero.
 * @return Independent event or resource/allocation error.
 */
template <typename Field>
std::expected<NuamTrade, MappingError> projectNuamTrade(std::u16string_view symbol, std::span<const Field> fields,
                                                        std::span<const std::optional<std::size_t>> selections,
                                                        std::size_t symbolUnits, std::int64_t eventTime,
                                                        char16_t exchange) {
    auto base = detail::projectTradeLike<Trade>(symbol, fields, selections, symbolUnits, eventTime, exchange,
                                                detail::TRADE_LAYOUT);

    if (!base) {
        return std::unexpected(base.error());
    }

    NuamTrade result;
    static_cast<Trade &>(result) = std::move(*base);
    result.tradeStatTime = fields[*selections[fieldIndex(NuamTradeField::STAT_TIME)]].raw;
    result.numberOfTrades =
        detail::projectPhysicalInt(fields[*selections[fieldIndex(NuamTradeField::TRADE_COUNT)]].type,
                                   fields[*selections[fieldIndex(NuamTradeField::TRADE_COUNT)]].raw);
    const std::array properties{std::pair{fieldIndex(NuamTradeField::SIGNIFICANT_PRICE), &result.lastSignificantPrice},
                                std::pair{fieldIndex(NuamTradeField::LAST_PRICE_FOR_ALL), &result.lastPriceForAll},
                                std::pair{fieldIndex(NuamTradeField::VWAP), &result.vwap}};
    std::array<detail::BusinessNumericSnapshot, 3> snapshots{};

    for (std::size_t index = 0; index < properties.size(); ++index) {
        const auto &value = fields[*selections[properties[index].first]];
        *properties[index].second = detail::projectPresentDecimal(value.type, value.raw);
        snapshots[index] = {properties[index].second->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
    }

    result.nuamNumericSnapshot = snapshots;

    return result;
}
} // namespace

std::int64_t NuamTrade::getTradeStatTime() const noexcept {
    return tradeStatTime;
}

double NuamTrade::getLastSignificantPrice() const noexcept {
    return detail::businessValue(lastSignificantPrice, nuamNumericSnapshot, 0);
}

double NuamTrade::getLastPriceForAll() const noexcept {
    return detail::businessValue(lastPriceForAll, nuamNumericSnapshot, 1);
}

std::int32_t NuamTrade::getNumberOfTrades() const noexcept {
    return numberOfTrades;
}

double NuamTrade::getVWAP() const noexcept {
    return detail::businessValue(vwap, nuamNumericSnapshot, 2);
}

std::expected<core::LocalSchema, core::SchemaError> defaultNuamTradeSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array<std::u16string_view, NUAM_TRADE_FIELD_COUNT> names{
        u"Last.Time",  u"Last.Sequence", u"Last.TimeNanoPart",    u"Last.Exchange",   u"TradeId",        u"Last.Price",
        u"Last.Size",  u"Last.Tick",     u"Last.Change",          u"DayId",           u"Volume",         u"DayTurnover",
        u"Last.Flags", u"TradeStatTime", u"LastSignificantPrice", u"LastPriceForAll", u"NumberOfTrades", u"VWAP"};
    constexpr std::array<std::u16string_view, NUAM_TRADE_FIELD_COUNT> aliases{u"Time",
                                                                              u"Sequence",
                                                                              u"TimeNanoPart",
                                                                              u"ExchangeCode",
                                                                              u"TradeId",
                                                                              u"Price",
                                                                              u"Size",
                                                                              u"Tick",
                                                                              u"Change",
                                                                              u"DayId",
                                                                              u"DayVolume",
                                                                              u"DayTurnover",
                                                                              u"Flags",
                                                                              u"TradeStatTime",
                                                                              u"LastSignificantPrice",
                                                                              u"LastPriceForAll",
                                                                              u"NumberOfTrades",
                                                                              u"VWAP"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::COMPACT_INT,  Type::UTF_CHAR,
                               Type::LONG,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT,
                               Type::WIDE_DECIMAL, Type::DATE,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::COMPACT_INT,  Type::TIME_MILLIS,  Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::COMPACT_INT,  Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"NuamTrade", false, {}, {}};
        record.fields.reserve(names.size());
        record.lookups.reserve(names.size() * 2);

        for (std::size_t index = 0; index < names.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(names[index]), index});

            if (aliases[index] != names[index]) {
                record.lookups.push_back({std::u16string(aliases[index]), index});
            }
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedNuamTrade::PreparedNuamTrade(EventMapping mapping, char16_t recordExchange)
    : mapping_(std::move(mapping)), recordExchange_(recordExchange) {
}

std::expected<PreparedNuamTrade, MappingError> PreparedNuamTrade::prepare(const core::LocalSchema &schema,
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
    constexpr std::array<std::u16string_view, 1> statTime{u"TradeStatTime"};
    constexpr std::array<std::u16string_view, 1> significant{u"LastSignificantPrice"};
    constexpr std::array<std::u16string_view, 1> allPrice{u"LastPriceForAll"};
    constexpr std::array<std::u16string_view, 1> count{u"NumberOfTrades"};
    constexpr std::array<std::u16string_view, 1> vwap{u"VWAP"};
    const std::array<FieldRequest, NUAM_TRADE_FIELD_COUNT> requests{{{time, detail::RAW_INT_TYPES, false},
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
                                                                     {flags, detail::RAW_INT_TYPES, false},
                                                                     {statTime, longTypes, true},
                                                                     {significant, detail::NUMERIC_TYPES, true},
                                                                     {allPrice, detail::NUMERIC_TYPES, true},
                                                                     {count, detail::RAW_INT_TYPES, true},
                                                                     {vwap, detail::NUMERIC_TYPES, true}}};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    return PreparedNuamTrade(std::move(*mapping), market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedNuamTrade::context() const noexcept {
    return mapping_.context();
}

std::expected<NuamTrade, MappingError> PreparedNuamTrade::map(std::u16string_view symbol, RecordValueView values,
                                                              std::size_t symbolUnits, std::int64_t eventTime) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return projectNuamTrade(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}

std::expected<NuamTrade, MappingError> PreparedNuamTrade::mapView(std::u16string_view symbol,
                                                                  BorrowedRecordValueView values,
                                                                  std::size_t symbolUnits,
                                                                  std::int64_t eventTime) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return projectNuamTrade(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}
} // namespace org::ttldtor::qdcxx::events
