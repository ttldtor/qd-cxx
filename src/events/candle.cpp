// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/candle.hpp"
#include "qd/events/event_flags.hpp"

#include "decimal_projection.hpp"
#include "qd/events/time_helpers.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
constexpr std::array<std::u16string_view, 13> CANDLE_KEYS{
    u"Time",   u"Sequence", u"Count",      u"Open",       u"High",          u"Low",         u"Close",
    u"Volume", u"VWAP",     u"Bid.Volume", u"Ask.Volume", u"ImpVolatility", u"OpenInterest"};
constexpr std::array<bool, CANDLE_KEYS.size()> CANDLE_REQUIRED{true,  true,  false, true,  true,  true, true,
                                                               false, false, false, false, false, false};
constexpr std::array<std::u16string_view, 4> TRADE_KEYS{u"Time", u"Sequence", u"Price", u"Size"};
constexpr std::array<std::int64_t, 19> POWERS{1LL,
                                              10LL,
                                              100LL,
                                              1000LL,
                                              10000LL,
                                              100000LL,
                                              1000000LL,
                                              10000000LL,
                                              100000000LL,
                                              1000000000LL,
                                              10000000000LL,
                                              100000000000LL,
                                              1000000000000LL,
                                              10000000000000LL,
                                              100000000000000LL,
                                              1000000000000000LL,
                                              10000000000000000LL,
                                              100000000000000000LL,
                                              1000000000000000000LL};
constexpr std::size_t FIRST_NUMERIC = 3;

/** Java narrowing from a business double to long.
 * @param value Finite, nonfinite or out-of-range numeric value.
 * @return Truncated or saturated long; NaN becomes zero.
 */
std::int64_t narrowDouble(double value) noexcept {
    if (std::isnan(value))
        return 0;

    if (value >= 0x1p63)
        return std::numeric_limits<std::int64_t>::max();

    if (value <= -0x1p63)
        return std::numeric_limits<std::int64_t>::min();

    return static_cast<std::int64_t>(value);
}

/** Java WideDecimal.toLong performs exact decimal rounding toward positive infinity at half.
 * @param value Complete raw wide decimal.
 * @return Rounded or saturated integer, with NaN mapped to zero.
 */
std::int64_t wideToLong(core::WideDecimal value) noexcept {
    const auto significand = value.significand();
    const int exponent = value.exponent();

    if (value.kind() != core::DecimalKind::FINITE) {
        if (significand == 0)
            return 0;

        return significand > 0 ? std::numeric_limits<std::int64_t>::max() : std::numeric_limits<std::int64_t>::min();
    }

    if (exponent < 0) {
        const auto scale = -exponent;

        if (scale >= static_cast<int>(POWERS.size()))
            return 0;

        const auto divisor = POWERS[scale];
        auto result = significand / divisor;
        const auto remainder = significand % divisor;

        if (remainder >= divisor / 2)
            ++result;
        else if (remainder < -divisor / 2)
            --result;

        return result;
    }

    if (significand == 0)
        return 0;

    if (exponent >= static_cast<int>(POWERS.size()))
        return significand > 0 ? std::numeric_limits<std::int64_t>::max() : std::numeric_limits<std::int64_t>::min();

    const auto factor = POWERS[exponent];

    if (significand > std::numeric_limits<std::int64_t>::max() / factor)
        return std::numeric_limits<std::int64_t>::max();

    if (significand < std::numeric_limits<std::int64_t>::min() / factor)
        return std::numeric_limits<std::int64_t>::min();

    return significand * factor;
}

/** Read the Java mapping's numeric-to-long count without losing LONG/WIDE precision.
 * @param type Validated complete numeric field type.
 * @param raw Exact source field bits.
 * @return Java record mapping long projection.
 */
std::int64_t countValue(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::WIDE_DECIMAL)
        return wideToLong(core::WideDecimal{raw});

    if (type == core::SerialFieldType::DECIMAL)
        return narrowDouble(detail::projectPresentBusinessDouble(type, raw));

    return raw;
}

/** Create a candle after complete field validation.
 * @tparam Field Canonical owning or borrowed field representation.
 * @param symbol Independently shared immutable candle identity.
 * @param fields Complete validated local source field order.
 * @param selected Prepared property positions.
 * @param kind Java delegate kind.
 * @param symbolUnits Maximum canonical symbol UTF-16 units.
 * @param eventTime Selected FIELD milliseconds.
 * @param eventFlags Exact indexed flags.
 * @return Owning complete candle or structured failure.
 */
template <typename Field>
std::expected<Candle, MappingError> project(const CandleSymbol &symbol, std::span<const Field> fields,
                                            std::span<const std::optional<std::size_t>> selected, CandleRecordKind kind,
                                            std::size_t symbolUnits, std::int64_t eventTime, std::int32_t eventFlags) {
    const auto text = symbol.getSymbol();

    if (text && text->size() > symbolUnits)
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    const auto physical = [&](std::size_t index) {
        const auto &field = fields[*selected[index]];

        return static_cast<std::uint32_t>(detail::projectPhysicalInt(field.type, field.raw));
    };

    Candle result;
    result.eventSymbol = symbol;
    result.eventTime = eventTime;
    result.eventFlags = eventFlags;
    result.index = std::bit_cast<std::int64_t>((std::uint64_t{physical(0)} << 32) | physical(1));
    std::array<detail::BusinessNumericSnapshot, 10> snapshots{};
    const std::array targets{&result.open,          &result.high,        &result.low,       &result.close,
                             &result.volume,        &result.vwap,        &result.bidVolume, &result.askVolume,
                             &result.impVolatility, &result.openInterest};

    const auto setNumeric = [&](std::size_t target, std::optional<std::size_t> source) {
        if (!source)
            return;

        const auto &field = fields[*source];
        *targets[target] = detail::projectPresentDecimal(field.type, field.raw);
        snapshots[target] = {targets[target]->raw, detail::projectPresentBusinessDouble(field.type, field.raw)};
    };

    if (kind == CandleRecordKind::TRADE_HISTORY) {
        result.count = 1;

        for (std::size_t index = 0; index < 4; ++index)
            setNumeric(index, selected[2]);

        setNumeric(4, selected[3]);
    } else {
        if (selected[2]) {
            const auto &field = fields[*selected[2]];
            result.count = countValue(field.type, field.raw);
        }

        for (std::size_t index = 0; index < targets.size(); ++index)
            setNumeric(index, selected[FIRST_NUMERIC + index]);
    }

    result.businessNumericSnapshot = snapshots;

    return result;
}
} // namespace

std::optional<CandleSymbol> Candle::getEventSymbol() const noexcept {
    return eventSymbol;
}

const IndexedEventSource &Candle::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t Candle::getIndex() const noexcept {
    return index;
}

std::int64_t Candle::getTime() const noexcept {
    return getTimeFromTimeSequence(index);
}

std::int32_t Candle::getSequence() const noexcept {
    return getSequenceFromTimeSequence(index);
}

std::int64_t Candle::getCount() const noexcept {
    return count;
}

double Candle::getOpen() const noexcept {
    return detail::businessValue(open, businessNumericSnapshot, 0);
}

double Candle::getHigh() const noexcept {
    return detail::businessValue(high, businessNumericSnapshot, 1);
}

double Candle::getLow() const noexcept {
    return detail::businessValue(low, businessNumericSnapshot, 2);
}

double Candle::getClose() const noexcept {
    return detail::businessValue(close, businessNumericSnapshot, 3);
}

double Candle::getVolumeAsDouble() const noexcept {
    return detail::businessValue(volume, businessNumericSnapshot, 4);
}

std::int64_t Candle::getVolume() const noexcept {
    return narrowDouble(getVolumeAsDouble());
}

double Candle::getVWAP() const noexcept {
    return detail::businessValue(vwap, businessNumericSnapshot, 5);
}

double Candle::getBidVolumeAsDouble() const noexcept {
    return detail::businessValue(bidVolume, businessNumericSnapshot, 6);
}

std::int64_t Candle::getBidVolume() const noexcept {
    return narrowDouble(getBidVolumeAsDouble());
}

double Candle::getAskVolumeAsDouble() const noexcept {
    return detail::businessValue(askVolume, businessNumericSnapshot, 7);
}

std::int64_t Candle::getAskVolume() const noexcept {
    return narrowDouble(getAskVolumeAsDouble());
}

double Candle::getImpVolatility() const noexcept {
    return detail::businessValue(impVolatility, businessNumericSnapshot, 8);
}

double Candle::getOpenInterestAsDouble() const noexcept {
    return detail::businessValue(openInterest, businessNumericSnapshot, 9);
}

std::int64_t Candle::getOpenInterest() const noexcept {
    return narrowDouble(getOpenInterestAsDouble());
}

std::expected<core::LocalSchema, core::SchemaError> defaultCandleSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;

    try {
        core::LocalRecord record{0, u"Candle", true, {}, {}, core::IndexedTimePairInt32{0, 1}};
        record.fields.reserve(CANDLE_KEYS.size());
        record.lookups.reserve(CANDLE_KEYS.size());

        for (std::size_t index = 0; index < CANDLE_KEYS.size(); ++index) {
            const auto type = index == 0 ? Type::TIME_SECONDS : index == 1 ? Type::SEQUENCE : Type::WIDE_DECIMAL;
            record.fields.push_back({std::to_underlying(type)});
            record.lookups.push_back({std::u16string(CANDLE_KEYS[index]), index});
        }

        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedCandle::PreparedCandle(EventMapping mapping, CandleRecordKind kind,
                               std::optional<core::IndexedTimeLayout> indexedTimeLayout) noexcept
    : mapping_(std::move(mapping)), kind_(kind), indexedTimeLayout_(indexedTimeLayout) {
}

std::expected<PreparedCandle, MappingError> PreparedCandle::prepare(const core::LocalSchema &schema,
                                                                    std::int32_t localId, CandleRecordKind kind,
                                                                    MappingLimits limits) {
    if (kind != CandleRecordKind::CANDLE && kind != CandleRecordKind::TRADE_HISTORY)
        return std::unexpected(MappingError::INVALID_INPUT);

    std::array<FieldRequest, CANDLE_KEYS.size()> requests{};
    const bool trade = kind == CandleRecordKind::TRADE_HISTORY;
    const auto count = trade ? TRADE_KEYS.size() : CANDLE_KEYS.size();

    for (std::size_t index = 0; index < count; ++index) {
        const auto keys = trade ? std::span(TRADE_KEYS).subspan(index, 1) : std::span(CANDLE_KEYS).subspan(index, 1);
        const auto types = index < 2 ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                                     : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES);
        requests[index] = {keys, types, trade || CANDLE_REQUIRED[index]};
    }

    auto mapping = EventMapping::prepare(schema, localId, std::span(requests).first(count), limits);

    if (!mapping)
        return std::unexpected(mapping.error());

    std::optional<core::IndexedTimeLayout> indexedTimeLayout;
    const auto &declaredLayout = schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout;
    const auto *pair = declaredLayout ? std::get_if<core::IndexedTimePairInt32>(&*declaredLayout) : nullptr;
    const auto selected = mapping->selections();

    if (pair && selected[0] == pair->firstField && selected[1] == pair->secondField)
        indexedTimeLayout = *pair;

    return PreparedCandle(std::move(*mapping), kind, indexedTimeLayout);
}

const core::RecordContext &PreparedCandle::context() const noexcept {
    return mapping_.context();
}

std::expected<Candle, MappingError> PreparedCandle::map(const CandleSymbol &symbol, RecordValueView values,
                                                        std::size_t symbolUnits, std::int64_t eventTime,
                                                        std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), kind_, symbolUnits, eventTime, eventFlags);
}

std::expected<Candle, MappingError> PreparedCandle::mapView(const CandleSymbol &symbol, BorrowedRecordValueView values,
                                                            std::size_t symbolUnits, std::int64_t eventTime,
                                                            std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), kind_, symbolUnits, eventTime, eventFlags);
}

std::expected<Candle, MappingError>
PreparedCandle::mapIndexedRemovalView(const CandleSymbol &symbol, BorrowedRecordValueView values,
                                      std::size_t symbolUnits, std::int64_t eventTime, std::int32_t eventFlags,
                                      std::int64_t rawTime) const {
    if (!indexedTimeLayout_ || !(eventFlags & indexed_event_flags::REMOVE_EVENT))
        return std::unexpected(MappingError::INVALID_INPUT);

    if (const auto valid = mapping_.validateIndexedRemoval(values, *indexedTimeLayout_, rawTime); !valid)
        return std::unexpected(valid.error());

    return project(symbol, values.values, mapping_.selections(), kind_, symbolUnits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
