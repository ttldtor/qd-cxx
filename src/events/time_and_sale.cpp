// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/time_and_sale.hpp"
#include "qd/events/nuam_time_and_sale.hpp"

#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "qd/events/short_string.hpp"
#include "qd/events/time_helpers.hpp"
#include "trade_projection.hpp"

#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class Field : std::size_t {
    TIME,
    SEQUENCE,
    TIME_NANO_PART,
    EXCHANGE,
    TRADE_ID,
    PRICE,
    SIZE,
    BID_PRICE,
    ASK_PRICE,
    SALE_CONDITIONS,
    FLAGS,
    BUYER,
    SELLER
};

constexpr std::size_t position(Field field) noexcept {
    return std::to_underlying(field);
}

constexpr std::array<std::u16string_view, 13> KEYS{
    u"Time",      u"Sequence",  u"TimeNanoPart",           u"Exchange", u"TradeId", u"Price", u"Size",
    u"Bid.Price", u"Ask.Price", u"ExchangeSaleConditions", u"Flags",    u"Buyer",   u"Seller"};
constexpr std::array<std::u16string_view, 2> EXCHANGE_KEYS{u"Exchange", u"ExchangeCode"};
constexpr std::array<std::u16string_view, 2> BID_KEYS{u"Bid.Price", u"BidPrice"};
constexpr std::array<std::u16string_view, 2> ASK_KEYS{u"Ask.Price", u"AskPrice"};
constexpr std::array<std::u16string_view, 2> CONDITIONS_KEYS{u"ExchangeSaleConditions", u"SaleConditions"};
constexpr std::array NUMERIC_FIELDS{Field::PRICE, Field::SIZE, Field::BID_PRICE, Field::ASK_PRICE};
constexpr std::array STRING_TYPES{core::SerialFieldType::UTF_CHAR_ARRAY, core::SerialFieldType::STRING};
constexpr std::array LONG_TYPES{core::SerialFieldType::LONG, core::SerialFieldType::WIDE_DECIMAL,
                                core::SerialFieldType::TIME_MILLIS};

/** Count the nonzero bytes in a sign-extended Java int passed to ShortString.decode.
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

/** Project validated source fields without retaining any caller storage.
 * @tparam Value Owning or borrowed complete field representation.
 * @param symbol Physical source symbol.
 * @param values Complete validated source values.
 * @param selected Ordered immutable field selections.
 * @param limits Finite output text allowances.
 * @param eventTime Independent FIELD event time.
 * @param eventFlags Indexed-event flags.
 * @param exchange Record-name regional suffix, or zero.
 * @return Independent event or a resource/allocation refusal.
 */
template <class Value>
std::expected<TimeAndSale, MappingError>
project(std::u16string_view symbol, std::span<const Value> values, std::span<const std::optional<std::size_t>> selected,
        EventValueLimits limits, std::int64_t eventTime, std::int32_t eventFlags, char16_t exchange) {
    const auto raw = [&](Field field) {
        const auto index = selected[position(field)];

        return index ? detail::projectPhysicalInt(values[*index].type, values[*index].raw) : 0;
    };
    const auto text = [&](Field field) -> std::optional<std::u16string_view> {
        const auto index = selected[position(field)];

        if (!index || !values[*index].units)
            return std::nullopt;

        return std::u16string_view(*values[*index].units);
    };
    const auto buyer = text(Field::BUYER);
    const auto seller = text(Field::SELLER);
    const auto symbolUnits = symbol.size() + (exchange && !market_symbols::hasExchangeCode(symbol) ? 2 : 0);
    const auto conditionsUnits = shortStringUnits(raw(Field::SALE_CONDITIONS));
    const auto buyerUnits = buyer ? buyer->size() : 0;
    const auto sellerUnits = seller ? seller->size() : 0;

    if (symbolUnits < symbol.size() || symbolUnits > limits.symbolUnits || symbolUnits > limits.totalUnits ||
        conditionsUnits > limits.stringUnits || buyerUnits > limits.stringUnits || sellerUnits > limits.stringUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    auto remaining = limits.totalUnits - symbolUnits;

    for (const auto units : {conditionsUnits, buyerUnits, sellerUnits}) {
        if (units > remaining)
            return std::unexpected(MappingError::RESOURCE_LIMIT);

        remaining -= units;
    }

    try {
        TimeAndSale result;
        result.symbol = exchange ? *market_symbols::changeExchangeCode(symbol, exchange) : std::u16string(symbol);
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index = std::bit_cast<std::int64_t>(
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(Field::TIME))) << 32) |
            static_cast<std::uint32_t>(raw(Field::SEQUENCE)));
        result.timeNanoPart = raw(Field::TIME_NANO_PART);
        result.exchangeCode = static_cast<char16_t>(static_cast<std::uint32_t>(raw(Field::EXCHANGE)) & 0xffff);

        if (selected[position(Field::TRADE_ID)])
            result.tradeId = values[*selected[position(Field::TRADE_ID)]].raw;

        const std::array destinations{&result.price, &result.size, &result.bidPrice, &result.askPrice};
        std::array<detail::BusinessNumericSnapshot, NUMERIC_FIELDS.size()> snapshots{};

        for (std::size_t index = 0; index < destinations.size(); ++index) {
            const auto &value = values[*selected[position(NUMERIC_FIELDS[index])]];
            *destinations[index] = detail::projectPresentDecimal(value.type, value.raw);
            snapshots[index] = {destinations[index]->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
        }

        result.businessNumericSnapshot = snapshots;
        result.exchangeSaleConditions = decodeShortString(raw(Field::SALE_CONDITIONS));
        result.flags = raw(Field::FLAGS);

        if (buyer)
            result.buyer.emplace(*buyer);

        if (seller)
            result.seller.emplace(*seller);

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

const IndexedEventSource &TimeAndSale::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int32_t TimeAndSale::getEventFlags() const noexcept {
    return eventFlags;
}

std::int64_t TimeAndSale::getIndex() const noexcept {
    return index;
}

std::int64_t TimeAndSale::getTime() const noexcept {
    return getTimeFromTimeSequence(index);
}

std::int64_t TimeAndSale::getTimeNanos() const noexcept {
    return getNanosFromMillisAndNanoPart(getTime(), timeNanoPart);
}

std::int32_t TimeAndSale::getSequence() const noexcept {
    return getSequenceFromTimeSequence(index);
}

std::int32_t TimeAndSale::getTimeNanoPart() const noexcept {
    return timeNanoPart;
}

char16_t TimeAndSale::getExchangeCode() const noexcept {
    return exchangeCode;
}

std::int64_t TimeAndSale::getTradeId() const noexcept {
    return tradeId;
}

double TimeAndSale::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, 0);
}

double TimeAndSale::getSizeAsDouble() const noexcept {
    return detail::businessValue(size, businessNumericSnapshot, 1);
}

std::int64_t TimeAndSale::getSize() const noexcept {
    return detail::tradeInteger(getSizeAsDouble());
}

double TimeAndSale::getBidPrice() const noexcept {
    return detail::businessValue(bidPrice, businessNumericSnapshot, 2);
}

double TimeAndSale::getAskPrice() const noexcept {
    return detail::businessValue(askPrice, businessNumericSnapshot, 3);
}

const std::optional<std::u16string> &TimeAndSale::getExchangeSaleConditions() const noexcept {
    return exchangeSaleConditions;
}

char16_t TimeAndSale::getTradeThroughExempt() const noexcept {
    return static_cast<char16_t>(getBits(flags, 0xff, 8));
}

Side TimeAndSale::getAggressorSide() const noexcept {
    return marketEnumValue(marketEnumFromCode<Side>(getBits(flags, 3, 5))).value_or(Side::UNDEFINED);
}

bool TimeAndSale::isSpreadLeg() const noexcept {
    return (flags & (1 << 4)) != 0;
}

bool TimeAndSale::isExtendedTradingHours() const noexcept {
    return (flags & (1 << 3)) != 0;
}

bool TimeAndSale::isValidTick() const noexcept {
    return (flags & (1 << 2)) != 0;
}

TimeAndSaleType TimeAndSale::getType() const noexcept {
    return marketEnumValue(marketEnumFromCode<TimeAndSaleType>(getBits(flags, 3, 0))).value_or(TimeAndSaleType::NEW);
}

bool TimeAndSale::isNew() const noexcept {
    return getType() == TimeAndSaleType::NEW;
}

bool TimeAndSale::isCorrection() const noexcept {
    return getType() == TimeAndSaleType::CORRECTION;
}

bool TimeAndSale::isCancel() const noexcept {
    return getType() == TimeAndSaleType::CANCEL;
}

const std::optional<std::u16string> &TimeAndSale::getBuyer() const noexcept {
    return buyer;
}

const std::optional<std::u16string> &TimeAndSale::getSeller() const noexcept {
    return seller;
}

std::expected<core::LocalSchema, core::SchemaError> defaultTimeAndSaleSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array names{u"Time",      u"Sequence",  u"Exchange",
                               u"TradeId",   u"Price",     u"Size",
                               u"Bid.Price", u"Ask.Price", u"ExchangeSaleConditions",
                               u"Flags"};
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::UTF_CHAR,     Type::LONG,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::SHORT_STRING, Type::COMPACT_INT};
    try {
        core::LocalRecord record{0, u"TimeAndSale", true, {}, {}, core::IndexedTimePairInt32{0, 1}};

        for (std::size_t index = 0; index < names.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({names[index], index});
        }

        record.lookups.push_back({u"ExchangeCode", 2});
        record.lookups.push_back({u"BidPrice", 6});
        record.lookups.push_back({u"AskPrice", 7});
        record.lookups.push_back({u"SaleConditions", 8});

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedTimeAndSale::PreparedTimeAndSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout,
                                         char16_t exchange) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)), recordExchange_(exchange) {
}

std::expected<PreparedTimeAndSale, MappingError>
PreparedTimeAndSale::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid())
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    constexpr std::size_t NAME_SCANS = 3;

    if (record.name.size() > limits.nameUnits || record.name.size() > limits.totalNameUnits ||
        record.name.size() > limits.comparisonWork / NAME_SCANS) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= record.name.size();
    limits.comparisonWork -= record.name.size() * NAME_SCANS;
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        requests[index] = {std::span(KEYS).subspan(index, 1), detail::RAW_INT_TYPES, true};
    }

    requests[position(Field::TIME_NANO_PART)].required = false;
    requests[position(Field::EXCHANGE)].keys = EXCHANGE_KEYS;
    requests[position(Field::TRADE_ID)] = {std::span(KEYS).subspan(position(Field::TRADE_ID), 1), LONG_TYPES, false};

    for (const auto field : NUMERIC_FIELDS)
        requests[position(field)].types = detail::NUMERIC_TYPES;

    requests[position(Field::BID_PRICE)].keys = BID_KEYS;
    requests[position(Field::ASK_PRICE)].keys = ASK_KEYS;
    requests[position(Field::SALE_CONDITIONS)].keys = CONDITIONS_KEYS;
    requests[position(Field::BUYER)] = {std::span(KEYS).subspan(position(Field::BUYER), 1), STRING_TYPES, false};
    requests[position(Field::SELLER)] = {std::span(KEYS).subspan(position(Field::SELLER), 1), STRING_TYPES, false};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    if (record.indexedTimeLayout) {
        const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*record.indexedTimeLayout);
        const auto selected = mapping->selections();

        if (!pair || selected[position(Field::TIME)] != pair->firstField ||
            selected[position(Field::SEQUENCE)] != pair->secondField)
            return std::unexpected(MappingError::INVALID_INPUT);
    }

    return PreparedTimeAndSale(std::move(*mapping), record.indexedTimeLayout,
                               market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedTimeAndSale::context() const noexcept {
    return mapping_.context();
}

std::expected<TimeAndSale, MappingError>
PreparedTimeAndSale::map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits,
                         std::int64_t eventTime, std::int32_t eventFlags,
                         std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_))
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto valid = indexedRemovalTime
                           ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                           : mapping_.validate(values);

    if (!valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags, recordExchange_);
}

std::expected<TimeAndSale, MappingError>
PreparedTimeAndSale::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
                             std::int64_t eventTime, std::int32_t eventFlags,
                             std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_))
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto valid = indexedRemovalTime
                           ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                           : mapping_.validateView(values);

    if (!valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags, recordExchange_);
}

std::expected<core::LocalSchema, core::SchemaError> defaultNuamTimeAndSaleSchema(core::SchemaLimits limits) {
    auto base = defaultTimeAndSaleSchema(limits);

    if (!base)
        return std::unexpected(base.error());

    try {
        auto record = base->records().front();
        record.name = u"NuamTimeAndSale";
        const auto index = record.fields.size();
        record.fields.push_back({std::to_underlying(core::SerialFieldType::LONG)});
        record.lookups.push_back({u"MatchId", index});

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedNuamTimeAndSale::PreparedNuamTimeAndSale(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout,
                                                 char16_t exchange) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)), recordExchange_(exchange) {
}

std::expected<PreparedNuamTimeAndSale, MappingError>
PreparedNuamTimeAndSale::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid())
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    constexpr std::size_t NAME_SCANS = 3;

    if (record.name.size() > limits.nameUnits || record.name.size() > limits.totalNameUnits ||
        record.name.size() > limits.comparisonWork / NAME_SCANS)
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    limits.totalNameUnits -= record.name.size();
    limits.comparisonWork -= record.name.size() * NAME_SCANS;
    std::array<FieldRequest, KEYS.size() + 1> requests{};

    for (std::size_t index = 0; index < KEYS.size(); ++index)
        requests[index] = {std::span(KEYS).subspan(index, 1), detail::RAW_INT_TYPES, true};

    requests[position(Field::TIME_NANO_PART)].required = false;
    requests[position(Field::EXCHANGE)].keys = EXCHANGE_KEYS;
    requests[position(Field::TRADE_ID)] = {std::span(KEYS).subspan(position(Field::TRADE_ID), 1), LONG_TYPES, true};

    for (const auto field : NUMERIC_FIELDS)
        requests[position(field)].types = detail::NUMERIC_TYPES;

    requests[position(Field::BID_PRICE)].keys = BID_KEYS;
    requests[position(Field::ASK_PRICE)].keys = ASK_KEYS;
    requests[position(Field::SALE_CONDITIONS)].keys = CONDITIONS_KEYS;
    requests[position(Field::BUYER)] = {std::span(KEYS).subspan(position(Field::BUYER), 1), STRING_TYPES, false};
    requests[position(Field::SELLER)] = {std::span(KEYS).subspan(position(Field::SELLER), 1), STRING_TYPES, false};
    constexpr std::array<std::u16string_view, 1> MATCH_ID{u"MatchId"};
    requests.back() = {MATCH_ID, LONG_TYPES, true};
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    if (record.indexedTimeLayout) {
        const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*record.indexedTimeLayout);
        const auto selected = mapping->selections();

        if (!pair || selected[position(Field::TIME)] != pair->firstField ||
            selected[position(Field::SEQUENCE)] != pair->secondField)
            return std::unexpected(MappingError::INVALID_INPUT);
    }

    return PreparedNuamTimeAndSale(std::move(*mapping), record.indexedTimeLayout,
                                   market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedNuamTimeAndSale::context() const noexcept {
    return mapping_.context();
}

std::expected<NuamTimeAndSale, MappingError>
PreparedNuamTimeAndSale::map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits,
                             std::int64_t eventTime, std::int32_t eventFlags,
                             std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_))
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto valid = indexedRemovalTime
                           ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                           : mapping_.validate(values);

    if (!valid)
        return std::unexpected(valid.error());

    auto base = project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags, recordExchange_);

    if (!base)
        return std::unexpected(base.error());

    NuamTimeAndSale result;
    static_cast<TimeAndSale &>(result) = std::move(*base);
    result.matchId = values.values[*mapping_.selections().back()].raw;

    return result;
}

std::expected<NuamTimeAndSale, MappingError>
PreparedNuamTimeAndSale::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
                                 std::int64_t eventTime, std::int32_t eventFlags,
                                 std::optional<std::int64_t> indexedRemovalTime) const {
    if (indexedRemovalTime && (!(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_))
        return std::unexpected(MappingError::INVALID_INPUT);

    const auto valid = indexedRemovalTime
                           ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                           : mapping_.validateView(values);

    if (!valid)
        return std::unexpected(valid.error());

    auto base = project(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags, recordExchange_);

    if (!base)
        return std::unexpected(base.error());

    NuamTimeAndSale result;
    static_cast<TimeAndSale &>(result) = std::move(*base);
    result.matchId = values.values[*mapping_.selections().back()].raw;

    return result;
}
} // namespace org::ttldtor::qdcxx::events
