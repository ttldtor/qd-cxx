// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/summary.hpp"
#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/market_event_symbols.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
std::int32_t Summary::getDayId() const noexcept {
    return dayId;
}

std::int32_t Summary::getPrevDayId() const noexcept {
    return prevDayId;
}

std::int64_t Summary::getOpenInterest() const noexcept {
    return openInterest;
}

double Summary::getDayOpenPrice() const noexcept {
    return detail::businessValue(dayOpenPrice, businessNumericSnapshot, 0);
}

double Summary::getDayHighPrice() const noexcept {
    return detail::businessValue(dayHighPrice, businessNumericSnapshot, 1);
}

double Summary::getDayLowPrice() const noexcept {
    return detail::businessValue(dayLowPrice, businessNumericSnapshot, 2);
}

double Summary::getDayClosePrice() const noexcept {
    return detail::businessValue(dayClosePrice, businessNumericSnapshot, 3);
}

double Summary::getPrevDayClosePrice() const noexcept {
    return detail::businessValue(prevDayClosePrice, businessNumericSnapshot, 4);
}

double Summary::getPrevDayVolume() const noexcept {
    return detail::businessValue(prevDayVolume, businessNumericSnapshot, 5);
}

PriceType Summary::getDayClosePriceType() const noexcept {
    return marketEnumFromCode<PriceType>(getBits(flags, 3, 2));
}

PriceType Summary::getPrevDayClosePriceType() const noexcept {
    return marketEnumFromCode<PriceType>(getBits(flags, 3, 0));
}

namespace {
constexpr std::array<std::u16string_view, 10> LOCAL_KEYS{
    u"DayId",     u"DayOpen.Price",      u"DayHigh.Price", u"DayLow.Price", u"DayClose.Price",
    u"PrevDayId", u"PrevDayClose.Price", u"PrevDayVolume", u"OpenInterest", u"Flags"};
///< Actual generated mapping selection order; Summary has no business time fields.
constexpr std::array<std::u16string_view, 10> PROPERTY_KEYS{
    u"DayId",     u"DayOpenPrice",      u"DayHighPrice",  u"DayLowPrice",  u"DayClosePrice",
    u"PrevDayId", u"PrevDayClosePrice", u"PrevDayVolume", u"OpenInterest", u"Flags"};
///< Production default properties remove dots from the local name.
constexpr std::array<std::size_t, 6> NUMERIC_SELECTIONS{1, 2, 3, 4, 6, 7};

///< Six wide and direct-business fields; OpenInterest uses the actual mapping's long accessor.

/** Exact WideDecimal.toLong integer path, with no business-int narrowing (ADR-0010).
 * @param raw Complete source wide bits; every rank and significand is accepted.
 * @return Nearest integer with half ties toward positive infinity, sign saturation and nonfinite defaults.
 */
std::int64_t wideInterestLong(std::int64_t raw) noexcept {
    constexpr int INTEGER_RANK = 128;
    constexpr int MAX_EXACT_POWER = 18;
    const auto significand = raw >> 8;
    const auto rank = static_cast<int>(static_cast<std::uint64_t>(raw) & 0xff);
    const auto saturated = significand > 0   ? std::numeric_limits<std::int64_t>::max()
                           : significand < 0 ? std::numeric_limits<std::int64_t>::min()
                                             : std::int64_t{0};

    if (!rank) {
        return saturated;
    }

    if (rank == INTEGER_RANK) {
        return significand;
    }

    const auto power = rank > INTEGER_RANK ? rank - INTEGER_RANK : INTEGER_RANK - rank;

    if (power > MAX_EXACT_POWER) {
        return rank > INTEGER_RANK ? 0 : saturated;
    }

    std::int64_t factor = 1;

    for (int index = 0; index < power; ++index) {
        factor *= 10;
    }

    if (rank > INTEGER_RANK) {
        const auto half = factor / 2;

        return significand >= 0 ? (significand / half + 1) >> 1 : ((significand + 1) / half) >> 1;
    }

    if (significand > std::numeric_limits<std::int64_t>::max() / factor ||
        significand < std::numeric_limits<std::int64_t>::min() / factor) {
        return saturated;
    }

    return significand * factor;
}

/** Actual RecordMapping.getAsLong dispatch, correcting the pinned delegate narrowing defect (ADR-0010).
 * @param type Exact already validated NUMERIC_TYPES identity.
 * @param raw Complete logical field bits, including both words of a long/wide field.
 * @return Full signed64 OI, preserving physical input width and Java long saturation/defaults.
 */
std::int64_t businessInterest(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::WIDE_DECIMAL) {
        return wideInterestLong(raw);
    }

    if (type != core::SerialFieldType::DECIMAL) {
        return raw;
    }

    const auto value = detail::projectPresentBusinessDouble(type, raw);

    if (std::isnan(value)) {
        return 0;
    }

    constexpr double LONG_UPPER_EXCLUSIVE = 0x1p63;

    if (value >= LONG_UPPER_EXCLUSIVE) {
        return std::numeric_limits<std::int64_t>::max();
    }

    if (value <= -LONG_UPPER_EXCLUSIVE) {
        return std::numeric_limits<std::int64_t>::min();
    }

    return static_cast<std::int64_t>(value);
}

/** Project only after the complete context and canonical field shapes have been validated.
 * @tparam Field Owned or borrowed logical field representation.
 * @param symbol Borrowed physical symbol, unchanged through return.
 * @param fields Complete validated local-order values.
 * @param selections Immutable prepared field positions in actual generated request order.
 * @param symbolUnits Maximum owned output symbol size.
 * @param eventTime Independent FIELD metadata milliseconds.
 * @param exchange Syntactic record exchange or zero for composite.
 * @return Independent complete Summary or resource/allocation refusal.
 */
template <class Field>
std::expected<Summary, MappingError> projectSummary(std::u16string_view symbol, std::span<const Field> fields,
                                                    std::span<const std::optional<std::size_t>> selections,
                                                    std::size_t symbolUnits, std::int64_t eventTime,
                                                    char16_t exchange) {
    if (symbol.size() > symbolUnits ||
        (exchange && !market_symbols::hasExchangeCode(symbol) && symbolUnits - symbol.size() < 2)) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](std::size_t index) {
        return selections[index]
                   ? detail::projectPhysicalInt(fields[*selections[index]].type, fields[*selections[index]].raw)
                   : 0;
    };
    try {
        Summary result;
        result.symbol = exchange ? *market_symbols::changeExchangeCode(symbol, exchange) : std::u16string(symbol);
        result.eventTime = eventTime;
        result.dayId = raw(0);
        result.prevDayId = raw(5);
        result.flags = raw(9);
        std::array<detail::BusinessNumericSnapshot, 6> snapshots{};
        std::array<core::WideDecimal *, 6> targets{&result.dayOpenPrice,      &result.dayHighPrice,
                                                   &result.dayLowPrice,       &result.dayClosePrice,
                                                   &result.prevDayClosePrice, &result.prevDayVolume};

        for (std::size_t index = 0; index < targets.size(); ++index) {
            if (!selections[NUMERIC_SELECTIONS[index]]) {
                continue;
            }

            const auto &field = fields[*selections[NUMERIC_SELECTIONS[index]]];
            *targets[index] = detail::projectPresentDecimal(field.type, field.raw);
            snapshots[index] = {targets[index]->raw, detail::projectPresentBusinessDouble(field.type, field.raw)};
        }

        if (selections[8]) {
            const auto &field = fields[*selections[8]];
            result.openInterest = businessInterest(field.type, field.raw);
        }

        result.businessNumericSnapshot = snapshots;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

std::expected<core::LocalSchema, core::SchemaError> defaultSummarySchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::DATE,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::DATE,         Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::COMPACT_INT};
    try {
        core::LocalRecord record{0, u"Summary", false, {}, {}};
        record.fields.reserve(LOCAL_KEYS.size());
        record.lookups.reserve(15);

        for (std::size_t index = 0; index < LOCAL_KEYS.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(LOCAL_KEYS[index]), index});

            if (LOCAL_KEYS[index] != PROPERTY_KEYS[index]) {
                record.lookups.push_back({std::u16string(PROPERTY_KEYS[index]), index});
            }
        }

        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedSummary::PreparedSummary(EventMapping mapping, char16_t recordExchange)
    : mapping_(std::move(mapping)), recordExchange_(recordExchange) {
}

std::expected<PreparedSummary, MappingError> PreparedSummary::prepare(const core::LocalSchema &schema,
                                                                      std::int32_t localId, MappingLimits limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    const auto &record = schema.records()[static_cast<std::size_t>(localId)];
    constexpr std::size_t RECORD_NAME_SCANS = 3;

    if (record.name.size() > limits.nameUnits || record.name.size() > limits.totalNameUnits ||
        record.name.size() > limits.comparisonWork / RECORD_NAME_SCANS) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    limits.totalNameUnits -= record.name.size();
    limits.comparisonWork -= record.name.size() * RECORD_NAME_SCANS;
    std::array<std::array<std::u16string_view, 2>, 10> keys{};
    std::array<FieldRequest, 10> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        keys[index] = {LOCAL_KEYS[index], PROPERTY_KEYS[index]};
        const bool raw = index == 0 || index == 5 || index == 9;
        const bool required = index == 0 || index == 1 || index == 2 || index == 3 || index == 5 || index == 6;
        requests[index] = {std::span(keys[index]).first(LOCAL_KEYS[index] == PROPERTY_KEYS[index] ? 1 : 2),
                           raw ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                               : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES),
                           required};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedSummary(std::move(*mapping), market_symbols::getExchangeCode(record.name));
}

const core::RecordContext &PreparedSummary::context() const noexcept {
    return mapping_.context();
}

std::expected<Summary, MappingError> PreparedSummary::map(std::u16string_view symbol, RecordValueView values,
                                                          std::size_t symbolUnits, std::int64_t eventTime) const {
    auto validation = mapping_.validate(values);

    if (!validation) {
        return std::unexpected(validation.error());
    }

    return projectSummary(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}

std::expected<Summary, MappingError> PreparedSummary::mapView(std::u16string_view symbol,
                                                              BorrowedRecordValueView values, std::size_t symbolUnits,
                                                              std::int64_t eventTime) const {
    auto validation = mapping_.validateView(values);

    if (!validation) {
        return std::unexpected(validation.error());
    }

    return projectSummary(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, recordExchange_);
}
} // namespace org::ttldtor::qdcxx::events
