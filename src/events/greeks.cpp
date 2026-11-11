// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/greeks.hpp"

#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/time_helpers.hpp"

#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class GreeksField : std::size_t { TIME, SEQUENCE, PRICE, VOLATILITY, DELTA, GAMMA, THETA, RHO, VEGA };

constexpr std::size_t fieldIndex(GreeksField field) noexcept {
    return std::to_underlying(field);
}

constexpr std::size_t NUMERIC_START = fieldIndex(GreeksField::PRICE);
constexpr std::array<std::u16string_view, 9> KEYS{u"Time",  u"Sequence", u"GreeksPrice", u"Volatility", u"Delta",
                                                  u"Gamma", u"Theta",    u"Rho",         u"Vega"};
constexpr std::array<std::u16string_view, 3> PRICE_KEYS{u"GreeksPrice", u"Greeks.Price", u"Price"};

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
std::expected<Greeks, MappingError> projectGreeks(std::u16string_view symbol, std::span<const Field> fields,
                                                  std::span<const std::optional<std::size_t>> selected,
                                                  std::size_t symbolUnits, std::int64_t eventTime,
                                                  std::int32_t eventFlags) {
    if (symbol.size() > symbolUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](GreeksField field) -> std::int32_t {
        const auto id = selected[fieldIndex(field)];

        if (!id) {
            return 0;
        }

        const auto &value = fields[*id];

        return detail::projectPhysicalInt(value.type, value.raw);
    };

    try {
        Greeks result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        const auto seconds = static_cast<std::uint32_t>(raw(GreeksField::TIME));
        const auto sequence = static_cast<std::uint32_t>(raw(GreeksField::SEQUENCE));
        result.index = std::bit_cast<std::int64_t>((std::uint64_t{seconds} << 32) | sequence);
        const std::array targets{&result.price, &result.volatility, &result.delta, &result.gamma,
                                 &result.theta, &result.rho,        &result.vega};
        std::array<detail::BusinessNumericSnapshot, targets.size()> snapshots{};

        for (std::size_t index = 0; index < targets.size(); ++index) {
            const auto &value = fields[*selected[NUMERIC_START + index]];
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

const IndexedEventSource &Greeks::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t Greeks::getIndex() const noexcept {
    return index;
}

std::int64_t Greeks::getTime() const noexcept {
    return getTimeFromTimeSequence(index);
}

std::int32_t Greeks::getSequence() const noexcept {
    return getSequenceFromTimeSequence(index);
}

double Greeks::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, fieldIndex(GreeksField::PRICE) - NUMERIC_START);
}

double Greeks::getVolatility() const noexcept {
    return detail::businessValue(volatility, businessNumericSnapshot,
                                 fieldIndex(GreeksField::VOLATILITY) - NUMERIC_START);
}

double Greeks::getDelta() const noexcept {
    return detail::businessValue(delta, businessNumericSnapshot, fieldIndex(GreeksField::DELTA) - NUMERIC_START);
}

double Greeks::getGamma() const noexcept {
    return detail::businessValue(gamma, businessNumericSnapshot, fieldIndex(GreeksField::GAMMA) - NUMERIC_START);
}

double Greeks::getTheta() const noexcept {
    return detail::businessValue(theta, businessNumericSnapshot, fieldIndex(GreeksField::THETA) - NUMERIC_START);
}

double Greeks::getRho() const noexcept {
    return detail::businessValue(rho, businessNumericSnapshot, fieldIndex(GreeksField::RHO) - NUMERIC_START);
}

double Greeks::getVega() const noexcept {
    return detail::businessValue(vega, businessNumericSnapshot, fieldIndex(GreeksField::VEGA) - NUMERIC_START);
}

std::expected<core::LocalSchema, core::SchemaError> defaultGreeksSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"Greeks", true, {}, {}, core::IndexedTimePairInt32{0, 1}};

        for (std::size_t index = 0; index < KEYS.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});
            record.lookups.push_back({std::u16string(KEYS[index]), index});
        }

        record.lookups.push_back({u"Greeks.Price", fieldIndex(GreeksField::PRICE)});
        record.lookups.push_back({u"Price", fieldIndex(GreeksField::PRICE)});

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedGreeks::PreparedGreeks(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)) {
}

std::expected<PreparedGreeks, MappingError> PreparedGreeks::prepare(const core::LocalSchema &schema,
                                                                    std::int32_t localId, MappingLimits limits) {
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        requests[index] = {std::span(KEYS).subspan(index, 1),
                           index < NUMERIC_START ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                                                 : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES),
                           index >= NUMERIC_START};
    }

    requests[fieldIndex(GreeksField::PRICE)].keys = PRICE_KEYS;
    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    const auto &layout = schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout;

    if (layout) {
        const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*layout);
        const auto selections = mapping->selections();

        if (!pair || selections[fieldIndex(GreeksField::TIME)] != pair->firstField ||
            selections[fieldIndex(GreeksField::SEQUENCE)] != pair->secondField) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }
    }

    return PreparedGreeks(std::move(*mapping), layout);
}

const core::RecordContext &PreparedGreeks::context() const noexcept {
    return mapping_.context();
}

std::expected<Greeks, MappingError> PreparedGreeks::map(std::u16string_view symbol, RecordValueView values,
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

    return projectGreeks(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}

std::expected<Greeks, MappingError> PreparedGreeks::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                            std::size_t symbolUnits, std::int64_t eventTime,
                                                            std::int32_t eventFlags,
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

    return projectGreeks(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
