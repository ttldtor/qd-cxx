// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/derived_order.hpp"
#include "derived_order_sources.hpp"
#include "market_maker_mapping.hpp"
#include "numeric_int_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "qd/events/short_string.hpp"
#include "qd/events/time_helpers.hpp"
#include <array>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
/** @param role Caller-supplied role.
 * @return True only for one actual generated delegate identity.
 */
bool knownRole(DerivedOrderRole role) noexcept {
    return role == DerivedOrderRole::BID || role == DerivedOrderRole::ASK || role == DerivedOrderRole::UNITARY_BID ||
           role == DerivedOrderRole::UNITARY_ASK;
}

/** @param role Validated delegate identity.
 * @return Whether this is the bid side.
 */
bool bidRole(DerivedOrderRole role) noexcept {
    return role == DerivedOrderRole::BID || role == DerivedOrderRole::UNITARY_BID;
}

/** @param role Validated delegate identity.
 * @return Whether flags/index use the unitary representation.
 */
bool unitaryRole(DerivedOrderRole role) noexcept {
    return role == DerivedOrderRole::UNITARY_BID || role == DerivedOrderRole::UNITARY_ASK;
}

/** Prepare only the eight fields read by Quote-derived delegates; Sequence/nanos remain unselected.
 * @param schema Complete validated declarations.
 * @param localId Exact physical record.
 * @param limits Finite field-plan preparation credits.
 * @return Owned field plan or typed refusal.
 */
std::expected<EventMapping, MappingError> prepareQuoteOrderMapping(const core::LocalSchema &schema,
                                                                   std::int32_t localId, MappingLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array timeTypes{Type::TIME_SECONDS, Type::TIME_MILLIS};
    constexpr std::array<std::array<std::u16string_view, 2>, 8> keys{{{u"Bid.Time", u"BidTime"},
                                                                      {u"Bid.Exchange", u"BidExchangeCode"},
                                                                      {u"Bid.Price", u"BidPrice"},
                                                                      {u"Bid.Size", u"BidSize"},
                                                                      {u"Ask.Time", u"AskTime"},
                                                                      {u"Ask.Exchange", u"AskExchangeCode"},
                                                                      {u"Ask.Price", u"AskPrice"},
                                                                      {u"Ask.Size", u"AskSize"}}};
    const std::array<std::span<const Type>, 8> types{
        timeTypes, detail::RAW_INT_TYPES, detail::NUMERIC_TYPES, detail::NUMERIC_TYPES,
        timeTypes, detail::RAW_INT_TYPES, detail::NUMERIC_TYPES, detail::NUMERIC_TYPES};
    constexpr std::array required{false, false, true, true, false, false, true, true};
    std::array<FieldRequest, 8> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        requests[i] = {keys[i], types[i], required[i]};
    }

    return EventMapping::prepare(schema, localId, requests, limits);
}

/** Map the selected generated delegate after complete shape validation.
 * @tparam Field Canonical owning or borrowed scalar representation.
 * @param symbol Exact physical symbol, with no regional transformation.
 * @param fields Entire validated local-order shape.
 * @param selected Immutable property selections.
 * @param kind Validated physical family.
 * @param recordExchange Actual immutable regional record suffix.
 * @param role Validated delegate identity.
 * @param sourceId Actual source selected by this delegate.
 * @param limits Finite independent output bounds.
 * @param eventTime Independent FIELD milliseconds.
 * @param eventFlags Actual collector-produced cursor flags.
 * @return Whole independent Order or atomic typed refusal.
 */
template <class Field>
std::expected<Order, MappingError>
projectDerivedOrder(std::u16string_view symbol, std::span<const Field> fields,
                    std::span<const std::optional<std::size_t>> selected, DerivedOrderKind kind,
                    char16_t recordExchange, DerivedOrderRole role, std::int32_t sourceId, EventValueLimits limits,
                    std::int64_t eventTime, std::int32_t eventFlags) {
    const auto raw = [&](std::size_t i) -> std::int32_t {
        return selected[i] ? detail::projectPhysicalInt(fields[*selected[i]].type, fields[*selected[i]].raw) : 0;
    };
    const bool bid = bidRole(role);
    const bool unitary = unitaryRole(role);
    const bool maker = kind == DerivedOrderKind::MARKET_MAKER;
    const auto timeIndex = maker ? (bid ? std::size_t{2} : std::size_t{6}) : (bid ? std::size_t{0} : std::size_t{4});
    const auto priceIndex = maker ? timeIndex + 1 : timeIndex + 2;
    const auto sizeIndex = priceIndex + 1;
    const auto exchange = maker ? static_cast<char16_t>(raw(0))
                                : (recordExchange ? recordExchange : static_cast<char16_t>(raw(timeIndex + 1)));

    if (exchange > 0x7f) {
        return std::unexpected(MappingError::INVALID_VALUES);
    }

    const auto makerRaw = maker ? static_cast<std::int64_t>(raw(1)) : 0;
    auto makerBits = static_cast<std::uint64_t>(makerRaw);
    std::size_t makerUnits = 0;

    while (makerBits) {
        makerUnits += (makerBits & 0xff) != 0;
        makerBits >>= 8;
    }

    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits || makerUnits > limits.stringUnits ||
        makerUnits > limits.totalUnits - symbol.size()) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    std::int64_t time = 0;

    if (selected[timeIndex]) {
        const auto &f = fields[*selected[timeIndex]];
        time = f.type == core::SerialFieldType::TIME_MILLIS ? f.raw : f.raw * 1000;
    }

    const auto &price = fields[*selected[priceIndex]];
    const auto &size = fields[*selected[sizeIndex]];
    auto index = static_cast<std::uint64_t>(sourceId) << 48;
    const auto indexExchange = maker ? exchange : recordExchange;
    index |= static_cast<std::uint64_t>(unitary ? indexExchange & 0x7fff : indexExchange) << 32;

    if (unitary && bid) {
        index |= std::uint64_t{1} << 47;
    }

    if (maker) {
        index |= static_cast<std::uint32_t>(raw(1));
    }

    const auto scope = maker ? Scope::AGGREGATE : (recordExchange ? Scope::REGIONAL : Scope::COMPOSITE);
    try {
        Order result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.index = std::bit_cast<std::int64_t>(index);
        result.eventFlags = maker ? eventFlags : 0;

        if (unitary) {
            result.eventFlags =
                bid ? (eventFlags & ~(indexed_event_flags::SNAPSHOT_END | indexed_event_flags::SNAPSHOT_SNIP)) |
                          indexed_event_flags::TX_PENDING
                    : eventFlags & ~indexed_event_flags::SNAPSHOT_BEGIN;
        }

        result.timeSequence = withTimeSequenceTime(0, time);
        result.price = detail::projectPresentDecimal(price.type, price.raw);
        result.size = detail::projectPresentDecimal(size.type, size.raw);

        if (maker && selected[timeIndex + 3]) {
            const auto &f = fields[*selected[timeIndex + 3]];
            result.count = detail::projectPresentInt(f.type, f.raw);
        }

        result.flags = (static_cast<std::int32_t>(exchange) << 4) |
                       (marketEnumCode(bid ? Side::BUY : Side::SELL) << 2) | marketEnumCode(scope);
        result.marketMaker = decodeShortString(makerRaw);
        constexpr auto NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
        result.businessNumericSnapshot = std::array<detail::BusinessNumericSnapshot, 5>{
            {{result.price.raw, detail::projectPresentBusinessDouble(price.type, price.raw)},
             {result.size.raw, detail::projectPresentBusinessDouble(size.type, size.raw)},
             {0, NAN_VALUE},
             {0, NAN_VALUE},
             {0, NAN_VALUE}}};

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

PreparedDerivedOrder::PreparedDerivedOrder(EventMapping mapping, DerivedOrderKind kind, char16_t exchange,
                                           std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)), kind_(kind), recordExchange_(exchange) {
}

std::expected<PreparedDerivedOrder, MappingError> PreparedDerivedOrder::prepare(const core::LocalSchema &schema,
                                                                                std::int32_t localId,
                                                                                DerivedOrderKind kind,
                                                                                MappingLimits limits) {
    if (!schema.recordContext(localId).valid() ||
        (kind != DerivedOrderKind::QUOTE && kind != DerivedOrderKind::MARKET_MAKER)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    char16_t exchange = 0;

    if (kind == DerivedOrderKind::QUOTE) {
        const auto &name = schema.records()[static_cast<std::size_t>(localId)].name;
        constexpr std::size_t RECORD_NAME_SCANS = 3;

        if (name.size() > limits.nameUnits || name.size() > limits.totalNameUnits ||
            name.size() > limits.comparisonWork / RECORD_NAME_SCANS) {
            return std::unexpected(MappingError::RESOURCE_LIMIT);
        }

        limits.totalNameUnits -= name.size();
        limits.comparisonWork -= name.size() * RECORD_NAME_SCANS;
        exchange = market_symbols::getExchangeCode(name);
    }

    auto mapping = kind == DerivedOrderKind::QUOTE ? prepareQuoteOrderMapping(schema, localId, limits)
                                                   : detail::prepareMarketMakerMapping(schema, localId, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedDerivedOrder(std::move(*mapping), kind, exchange,
                                schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout);
}

const core::RecordContext &PreparedDerivedOrder::context() const noexcept {
    return mapping_.context();
}

DerivedOrderKind PreparedDerivedOrder::kind() const noexcept {
    return kind_;
}

std::optional<std::int32_t> PreparedDerivedOrder::sourceId(DerivedOrderRole role) const noexcept {
    if (!knownRole(role)) {
        return std::nullopt;
    }

    const auto sources = detail::derivedOrderSourceIds(kind_, recordExchange_);

    return unitaryRole(role) ? sources.unitary : (bidRole(role) ? sources.bid : sources.ask);
}

std::expected<Order, MappingError> PreparedDerivedOrder::map(std::u16string_view symbol, RecordValueView values,
                                                             DerivedOrderRole role, EventValueLimits limits,
                                                             std::int64_t eventTime, std::int32_t eventFlags,
                                                             std::optional<std::int64_t> indexedRemovalTime) const {
    const auto source = sourceId(role);

    if (!source) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (indexedRemovalTime && (kind_ != DerivedOrderKind::MARKET_MAKER ||
                               !(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (const auto valid = indexedRemovalTime
                               ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                               : mapping_.validate(values);
        !valid) {
        return std::unexpected(valid.error());
    }

    return projectDerivedOrder(symbol, values.values, mapping_.selections(), kind_, recordExchange_, role, *source,
                               limits, eventTime, eventFlags);
}

std::expected<Order, MappingError> PreparedDerivedOrder::mapView(std::u16string_view symbol,
                                                                 BorrowedRecordValueView values, DerivedOrderRole role,
                                                                 EventValueLimits limits, std::int64_t eventTime,
                                                                 std::int32_t eventFlags,
                                                                 std::optional<std::int64_t> indexedRemovalTime) const {
    const auto source = sourceId(role);

    if (!source) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (indexedRemovalTime && (kind_ != DerivedOrderKind::MARKET_MAKER ||
                               !(eventFlags & indexed_event_flags::REMOVE_EVENT) || !indexedTimeLayout_)) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    if (const auto valid = indexedRemovalTime
                               ? mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, *indexedRemovalTime)
                               : mapping_.validateView(values);
        !valid) {
        return std::unexpected(valid.error());
    }

    return projectDerivedOrder(symbol, values.values, mapping_.selections(), kind_, recordExchange_, role, *source,
                               limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
