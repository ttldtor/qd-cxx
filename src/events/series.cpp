// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/series.hpp"

#include "decimal_projection.hpp"
#include "qd/events/time_helpers.hpp"

#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class SeriesField : std::size_t {
    INDEX,
    TIME,
    SEQUENCE,
    EXPIRATION,
    VOLATILITY,
    CALL_VOLUME,
    PUT_VOLUME,
    PUT_CALL_RATIO,
    FORWARD_PRICE,
    DIVIDEND,
    INTEREST
};

/** @param field Property in prepared selection order.
 * @return Dense property position, independent of record-local field IDs.
 */
constexpr std::size_t fieldIndex(SeriesField field) noexcept {
    return std::to_underlying(field);
}

constexpr std::size_t NUMERIC_START = fieldIndex(SeriesField::VOLATILITY);
constexpr std::array<std::u16string_view, 11> KEYS{u"Index",        u"Time",       u"Sequence",  u"Expiration",
                                                   u"Volatility",   u"CallVolume", u"PutVolume", u"PutCallRatio",
                                                   u"ForwardPrice", u"Dividend",   u"Interest"};
constexpr std::array<bool, KEYS.size()> REQUIRED{false, false, false, true,  true, false,
                                                 false, true,  true,  false, false};

/** Project owning Series after complete input validation.
 * @tparam Field Owned or borrowed canonical logical field representation.
 * @param symbol Exact underlying symbol, borrowed through return.
 * @param fields Complete validated local-order values.
 * @param selections Immutable selected positions in property order.
 * @param symbolUnits Finite ownership bound for the copied symbol.
 * @param eventTime Independent FIELD milliseconds.
 * @param eventFlags Raw indexed delivery flags.
 * @return Independent owning value or resource/allocation refusal.
 */
template <typename Field>
std::expected<Series, MappingError> projectSeries(std::u16string_view symbol, std::span<const Field> fields,
                                                  std::span<const std::optional<std::size_t>> selections,
                                                  std::size_t symbolUnits, std::int64_t eventTime,
                                                  std::int32_t eventFlags) {
    if (symbol.size() > symbolUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](SeriesField field) -> std::int32_t {
        const auto selected = selections[fieldIndex(field)];

        if (!selected) {
            return 0;
        }

        const auto &value = fields[*selected];

        return detail::projectPhysicalInt(value.type, value.raw);
    };

    try {
        Series result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index = raw(SeriesField::INDEX);
        const auto seconds = static_cast<std::uint32_t>(raw(SeriesField::TIME));
        const auto sequence = static_cast<std::uint32_t>(raw(SeriesField::SEQUENCE));
        result.timeSequence = std::bit_cast<std::int64_t>((std::uint64_t{seconds} << 32) | sequence);
        result.expiration = raw(SeriesField::EXPIRATION);
        const std::array targets{&result.volatility,   &result.callVolume, &result.putVolume, &result.putCallRatio,
                                 &result.forwardPrice, &result.dividend,   &result.interest};
        std::array<detail::BusinessNumericSnapshot, targets.size()> snapshots{};

        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto selected = selections[NUMERIC_START + index];

            if (!selected) {
                continue;
            }

            const auto &value = fields[*selected];
            *targets[index] = detail::projectPresentDecimal(value.type, value.raw);
            snapshots[index] = {targets[index]->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
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

const IndexedEventSource &Series::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t Series::getIndex() const noexcept {
    return index;
}

std::int64_t Series::getTime() const noexcept {
    return getTimeFromTimeSequence(timeSequence);
}

std::int32_t Series::getSequence() const noexcept {
    return getSequenceFromTimeSequence(timeSequence);
}

std::int32_t Series::getExpiration() const noexcept {
    return expiration;
}

double Series::getVolatility() const noexcept {
    return detail::businessValue(volatility, businessNumericSnapshot,
                                 fieldIndex(SeriesField::VOLATILITY) - NUMERIC_START);
}

double Series::getCallVolume() const noexcept {
    return detail::businessValue(callVolume, businessNumericSnapshot,
                                 fieldIndex(SeriesField::CALL_VOLUME) - NUMERIC_START);
}

double Series::getPutVolume() const noexcept {
    return detail::businessValue(putVolume, businessNumericSnapshot,
                                 fieldIndex(SeriesField::PUT_VOLUME) - NUMERIC_START);
}

double Series::getOptionVolume() const noexcept {
    const auto calls = getCallVolume();
    const auto puts = getPutVolume();

    if (std::isnan(puts)) {
        return calls;
    }

    if (std::isnan(calls)) {
        return puts;
    }

    return puts + calls;
}

double Series::getPutCallRatio() const noexcept {
    return detail::businessValue(putCallRatio, businessNumericSnapshot,
                                 fieldIndex(SeriesField::PUT_CALL_RATIO) - NUMERIC_START);
}

double Series::getForwardPrice() const noexcept {
    return detail::businessValue(forwardPrice, businessNumericSnapshot,
                                 fieldIndex(SeriesField::FORWARD_PRICE) - NUMERIC_START);
}

double Series::getDividend() const noexcept {
    return detail::businessValue(dividend, businessNumericSnapshot, fieldIndex(SeriesField::DIVIDEND) - NUMERIC_START);
}

double Series::getInterest() const noexcept {
    return detail::businessValue(interest, businessNumericSnapshot, fieldIndex(SeriesField::INTEREST) - NUMERIC_START);
}

std::expected<core::LocalSchema, core::SchemaError> defaultSeriesSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::COMPACT_INT,  Type::TIME_SECONDS, Type::SEQUENCE,     Type::DATE,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"Series", true, {}, {}, core::IndexedTimeLowInt32{fieldIndex(SeriesField::INDEX)}};
        record.fields.reserve(KEYS.size());
        record.lookups.reserve(KEYS.size());

        for (std::size_t index = 0; index < KEYS.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(KEYS[index]), index});
        }

        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedSeries::PreparedSeries(EventMapping mapping) noexcept : mapping_(std::move(mapping)) {
}

std::expected<PreparedSeries, MappingError> PreparedSeries::prepare(const core::LocalSchema &schema,
                                                                    std::int32_t localId, MappingLimits limits) {
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        requests[index] = {std::span(KEYS).subspan(index, 1),
                           index < NUMERIC_START ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                                                 : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES),
                           REQUIRED[index]};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedSeries(std::move(*mapping));
}

const core::RecordContext &PreparedSeries::context() const noexcept {
    return mapping_.context();
}

std::expected<Series, MappingError> PreparedSeries::map(std::u16string_view symbol, RecordValueView values,
                                                        std::size_t symbolUnits, std::int64_t eventTime,
                                                        std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return projectSeries(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}

std::expected<Series, MappingError> PreparedSeries::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                            std::size_t symbolUnits, std::int64_t eventTime,
                                                            std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return projectSeries(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
