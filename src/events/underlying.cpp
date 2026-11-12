// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/underlying.hpp"

#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/time_helpers.hpp"

#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class UnderlyingField : std::size_t {
    TIME,
    SEQUENCE,
    VOLATILITY,
    FRONT_VOLATILITY,
    BACK_VOLATILITY,
    CALL_VOLUME,
    PUT_VOLUME,
    PUT_CALL_RATIO
};

constexpr std::size_t fieldIndex(UnderlyingField field) noexcept {
    return std::to_underlying(field);
}

constexpr std::size_t NUMERIC_START = fieldIndex(UnderlyingField::VOLATILITY);
constexpr std::array<std::u16string_view, 8> KEYS{u"Time",           u"Sequence",   u"Volatility", u"FrontVolatility",
                                                  u"BackVolatility", u"CallVolume", u"PutVolume",  u"PutCallRatio"};

/** Project an owning event after complete input validation.
 * @tparam Field Owning or borrowed canonical field representation.
 * @param symbol Event symbol, borrowed through return.
 * @param fields Complete validated local-order fields.
 * @param selected Prepared property positions.
 * @param symbolUnits Maximum copied symbol units.
 * @param eventTime Independent FIELD milliseconds.
 * @param eventFlags Actual indexed delivery flags.
 * @return Independent owning event or resource/allocation refusal.
 */
template <typename Field>
std::expected<Underlying, MappingError> projectUnderlying(std::u16string_view symbol, std::span<const Field> fields,
                                                          std::span<const std::optional<std::size_t>> selected,
                                                          std::size_t symbolUnits, std::int64_t eventTime,
                                                          std::int32_t eventFlags) {
    if (symbol.size() > symbolUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](UnderlyingField field) -> std::int32_t {
        const auto id = selected[fieldIndex(field)];

        if (!id) {
            return 0;
        }

        const auto &value = fields[*id];

        return detail::projectPhysicalInt(value.type, value.raw);
    };

    try {
        Underlying result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        const auto seconds = static_cast<std::uint32_t>(raw(UnderlyingField::TIME));
        const auto sequence = static_cast<std::uint32_t>(raw(UnderlyingField::SEQUENCE));
        result.index = std::bit_cast<std::int64_t>((std::uint64_t{seconds} << 32) | sequence);
        const std::array targets{&result.volatility, &result.frontVolatility, &result.backVolatility,
                                 &result.callVolume, &result.putVolume,       &result.putCallRatio};
        std::array<detail::BusinessNumericSnapshot, targets.size()> snapshots{};

        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto id = selected[NUMERIC_START + index];

            if (!id) {
                continue;
            }

            const auto &value = fields[*id];
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

const IndexedEventSource &Underlying::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t Underlying::getIndex() const noexcept {
    return index;
}

std::int64_t Underlying::getTime() const noexcept {
    return getTimeFromTimeSequence(index);
}

std::int32_t Underlying::getSequence() const noexcept {
    return getSequenceFromTimeSequence(index);
}

double Underlying::getVolatility() const noexcept {
    return detail::businessValue(volatility, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::VOLATILITY) - NUMERIC_START);
}

double Underlying::getFrontVolatility() const noexcept {
    return detail::businessValue(frontVolatility, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::FRONT_VOLATILITY) - NUMERIC_START);
}

double Underlying::getBackVolatility() const noexcept {
    return detail::businessValue(backVolatility, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::BACK_VOLATILITY) - NUMERIC_START);
}

double Underlying::getCallVolume() const noexcept {
    return detail::businessValue(callVolume, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::CALL_VOLUME) - NUMERIC_START);
}

double Underlying::getPutVolume() const noexcept {
    return detail::businessValue(putVolume, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::PUT_VOLUME) - NUMERIC_START);
}

double Underlying::getOptionVolume() const noexcept {
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

double Underlying::getPutCallRatio() const noexcept {
    return detail::businessValue(putCallRatio, businessNumericSnapshot,
                                 fieldIndex(UnderlyingField::PUT_CALL_RATIO) - NUMERIC_START);
}

std::expected<core::LocalSchema, core::SchemaError> defaultUnderlyingSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"Underlying", true, {}, {}, core::IndexedTimePairInt32{0, 1}};

        for (std::size_t index = 0; index < KEYS.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(KEYS[index]), index});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedUnderlying::PreparedUnderlying(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)) {
}

std::expected<PreparedUnderlying, MappingError>
PreparedUnderlying::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        requests[index] = {std::span(KEYS).subspan(index, 1),
                           index < NUMERIC_START ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                                                 : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES),
                           false};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    const auto &layout = schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout;

    if (layout) {
        const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*layout);
        const auto selections = mapping->selections();

        if (!pair || selections[fieldIndex(UnderlyingField::TIME)] != pair->firstField ||
            selections[fieldIndex(UnderlyingField::SEQUENCE)] != pair->secondField) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }
    }

    return PreparedUnderlying(std::move(*mapping), layout);
}

const core::RecordContext &PreparedUnderlying::context() const noexcept {
    return mapping_.context();
}

std::expected<Underlying, MappingError> PreparedUnderlying::map(std::u16string_view symbol, RecordValueView values,
                                                                std::size_t symbolUnits, std::int64_t eventTime,
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

    return projectUnderlying(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}

std::expected<Underlying, MappingError>
PreparedUnderlying::mapView(std::u16string_view symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
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

    return projectUnderlying(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
