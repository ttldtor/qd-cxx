// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/theo_price.hpp"

#include "decimal_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/time_helpers.hpp"

#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class TheoPriceField : std::size_t { TIME, SEQUENCE, PRICE, UNDERLYING_PRICE, DELTA, GAMMA, DIVIDEND, INTEREST };

constexpr std::size_t fieldIndex(TheoPriceField field) noexcept {
    return std::to_underlying(field);
}

constexpr std::size_t NUMERIC_START = fieldIndex(TheoPriceField::PRICE);
constexpr std::array<std::array<std::u16string_view, 3>, 8> KEYS{{
    {u"Time", u"Theo.Time", {}},
    {u"Sequence", u"Theo.Sequence", {}},
    {u"TheoPrice", u"Theo.Price", u"Price"},
    {u"TheoUnderlyingPrice", u"Theo.UnderlyingPrice", u"UnderlyingPrice"},
    {u"TheoDelta", u"Theo.Delta", u"Delta"},
    {u"TheoGamma", u"Theo.Gamma", u"Gamma"},
    {u"TheoDividend", u"Theo.Dividend", u"Dividend"},
    {u"TheoInterest", u"Theo.Interest", u"Interest"},
}};
constexpr std::array REQUIRED{true, false, true, true, true, true, false, false};

constexpr std::span<const std::u16string_view> fieldKeys(std::size_t index) noexcept {
    return std::span(KEYS[index]).first(index < NUMERIC_START ? 2 : 3);
}

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
std::expected<TheoPrice, MappingError> projectTheoPrice(std::u16string_view symbol, std::span<const Field> fields,
                                                        std::span<const std::optional<std::size_t>> selected,
                                                        std::size_t symbolUnits, std::int64_t eventTime,
                                                        std::int32_t eventFlags) {
    if (symbol.size() > symbolUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](TheoPriceField field) -> std::int32_t {
        const auto id = selected[fieldIndex(field)];

        if (!id) {
            return 0;
        }

        const auto &value = fields[*id];

        return detail::projectPhysicalInt(value.type, value.raw);
    };

    try {
        TheoPrice result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        const auto seconds = static_cast<std::uint32_t>(raw(TheoPriceField::TIME));
        const auto sequence = static_cast<std::uint32_t>(raw(TheoPriceField::SEQUENCE));
        result.index = std::bit_cast<std::int64_t>((std::uint64_t{seconds} << 32) | sequence);
        const std::array targets{&result.price, &result.underlyingPrice, &result.delta,
                                 &result.gamma, &result.dividend,        &result.interest};
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

const IndexedEventSource &TheoPrice::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t TheoPrice::getIndex() const noexcept {
    return index;
}

std::int64_t TheoPrice::getTime() const noexcept {
    return getTimeFromTimeSequence(index);
}

std::int32_t TheoPrice::getSequence() const noexcept {
    return getSequenceFromTimeSequence(index);
}

double TheoPrice::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, fieldIndex(TheoPriceField::PRICE) - NUMERIC_START);
}

double TheoPrice::getUnderlyingPrice() const noexcept {
    return detail::businessValue(underlyingPrice, businessNumericSnapshot,
                                 fieldIndex(TheoPriceField::UNDERLYING_PRICE) - NUMERIC_START);
}

double TheoPrice::getDelta() const noexcept {
    return detail::businessValue(delta, businessNumericSnapshot, fieldIndex(TheoPriceField::DELTA) - NUMERIC_START);
}

double TheoPrice::getGamma() const noexcept {
    return detail::businessValue(gamma, businessNumericSnapshot, fieldIndex(TheoPriceField::GAMMA) - NUMERIC_START);
}

double TheoPrice::getDividend() const noexcept {
    return detail::businessValue(dividend, businessNumericSnapshot,
                                 fieldIndex(TheoPriceField::DIVIDEND) - NUMERIC_START);
}

double TheoPrice::getInterest() const noexcept {
    return detail::businessValue(interest, businessNumericSnapshot,
                                 fieldIndex(TheoPriceField::INTEREST) - NUMERIC_START);
}

std::expected<core::LocalSchema, core::SchemaError> defaultTheoPriceSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"TheoPrice", true, {}, {}, core::IndexedTimePairInt32{0, 1}};

        for (std::size_t index = 0; index < KEYS.size(); ++index) {
            record.fields.push_back({std::to_underlying(types[index])});

            for (const auto key : fieldKeys(index))
                record.lookups.push_back({std::u16string(key), index});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedTheoPrice::PreparedTheoPrice(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)) {
}

std::expected<PreparedTheoPrice, MappingError> PreparedTheoPrice::prepare(const core::LocalSchema &schema,
                                                                          std::int32_t localId, MappingLimits limits) {
    std::array<FieldRequest, KEYS.size()> requests{};

    for (std::size_t index = 0; index < requests.size(); ++index) {
        requests[index] = {fieldKeys(index),
                           index < NUMERIC_START ? std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES)
                                                 : std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES),
                           REQUIRED[index]};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    const auto &layout = schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout;

    if (layout) {
        const auto *pair = std::get_if<core::IndexedTimePairInt32>(&*layout);
        const auto selections = mapping->selections();

        if (!pair || selections[fieldIndex(TheoPriceField::TIME)] != pair->firstField ||
            selections[fieldIndex(TheoPriceField::SEQUENCE)] != pair->secondField) {
            return std::unexpected(MappingError::INVALID_INPUT);
        }
    }

    return PreparedTheoPrice(std::move(*mapping), layout);
}

const core::RecordContext &PreparedTheoPrice::context() const noexcept {
    return mapping_.context();
}

std::expected<TheoPrice, MappingError> PreparedTheoPrice::map(std::u16string_view symbol, RecordValueView values,
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

    return projectTheoPrice(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}

std::expected<TheoPrice, MappingError>
PreparedTheoPrice::mapView(std::u16string_view symbol, BorrowedRecordValueView values, std::size_t symbolUnits,
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

    return projectTheoPrice(symbol, values.values, mapping_.selections(), symbolUnits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
