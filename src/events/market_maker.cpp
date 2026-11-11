// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/market_maker.hpp"
#include "market_maker_mapping.hpp"
#include "numeric_int_projection.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/short_string.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
std::int64_t MarketMaker::getIndex() const noexcept {
    return index;
}

std::int32_t MarketMaker::getSourceId() const noexcept {
    return 0;
}

const IndexedEventSource &MarketMaker::getSource() const noexcept {
    return IndexedEventSource::DEFAULT;
}

std::int64_t MarketMaker::getTime() const noexcept {
    return std::max(bidTimeMs, askTimeMs);
}

char16_t MarketMaker::getExchangeCode() const noexcept {
    return static_cast<char16_t>(static_cast<std::uint64_t>(index) >> 32);
}

std::optional<std::u16string> MarketMaker::getMarketMaker() const {
    return decodeShortString(static_cast<std::uint32_t>(index));
}

double MarketMaker::getBidPrice() const noexcept {
    return detail::businessValue(bidPrice, businessNumericSnapshot, 0);
}

double MarketMaker::getBidSize() const noexcept {
    return detail::businessValue(bidSize, businessNumericSnapshot, 1);
}

double MarketMaker::getAskPrice() const noexcept {
    return detail::businessValue(askPrice, businessNumericSnapshot, 2);
}

double MarketMaker::getAskSize() const noexcept {
    return detail::businessValue(askSize, businessNumericSnapshot, 3);
}

std::int64_t MarketMaker::getBidTime() const noexcept {
    return bidTimeMs;
}

std::int64_t MarketMaker::getAskTime() const noexcept {
    return askTimeMs;
}

std::int64_t MarketMaker::getBidCount() const noexcept {
    return bidCount;
}

std::int64_t MarketMaker::getAskCount() const noexcept {
    return askCount;
}

std::expected<core::LocalSchema, core::SchemaError> defaultMarketMakerSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::UTF_CHAR,     Type::SHORT_STRING, Type::TIME_SECONDS, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::TIME_SECONDS, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL};
    try {
        core::LocalRecord record{0, u"MarketMaker", true, {}, {}, core::IndexedTimePairInt32{0, 1}};

        for (std::size_t i = 0; i < types.size(); ++i) {
            record.fields.push_back({std::to_underlying(types[i])});
            const auto &keys = detail::MARKET_MAKER_KEYS[i];
            record.lookups.push_back({std::u16string(keys[0]), i});
            record.lookups.push_back({u"MarketMaker." + std::u16string(keys[0]), i});
            record.lookups.push_back({std::u16string(keys[1]), i});
        }

        return core::LocalSchema::create(std::span(&record, 1), limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedMarketMaker::PreparedMarketMaker(EventMapping mapping, std::optional<core::IndexedTimeLayout> layout) noexcept
    : mapping_(std::move(mapping)), indexedTimeLayout_(std::move(layout)) {
}

std::expected<PreparedMarketMaker, MappingError>
PreparedMarketMaker::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    auto mapping = detail::prepareMarketMakerMapping(schema, localId, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedMarketMaker(std::move(*mapping),
                               schema.records()[static_cast<std::size_t>(localId)].indexedTimeLayout);
}

const core::RecordContext &PreparedMarketMaker::context() const noexcept {
    return mapping_.context();
}

namespace {
/** Project validated fields after complete output-bound checks, without intermediate owning values.
 * @tparam Field Complete owning or borrowed canonical scalar representation.
 * @param symbol Borrowed physical/event symbol.
 * @param fields Entire validated local-order shape.
 * @param selected Ten independently owned property selections.
 * @param limits Finite output symbol/total units.
 * @param eventTime Independent FIELD metadata.
 * @param eventFlags Collector delivery flags.
 * @return Complete independent output or atomic allocation/resource refusal.
 */
template <class Field>
std::expected<MarketMaker, MappingError> projectMarketMaker(std::u16string_view symbol, std::span<const Field> fields,
                                                            std::span<const std::optional<std::size_t>> selected,
                                                            EventValueLimits limits, std::int64_t eventTime,
                                                            std::int32_t eventFlags) {
    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](std::size_t i) {
        return detail::projectPhysicalInt(fields[*selected[i]].type, fields[*selected[i]].raw);
    };
    const auto time = [&](std::size_t i) -> std::int64_t {
        if (!selected[i]) {
            return 0;
        }

        const auto &field = fields[*selected[i]];

        return field.type == core::SerialFieldType::TIME_MILLIS ? field.raw : field.raw * 1000;
    };
    const auto count = [&](std::size_t i) -> std::int64_t {
        if (!selected[i]) {
            return 0;
        }

        const auto &field = fields[*selected[i]];

        return detail::projectPresentInt(field.type, field.raw);
    };
    std::array<detail::BusinessNumericSnapshot, 4> snapshots{};
    const auto numeric = [&](std::size_t i, std::size_t slot) {
        const auto &field = fields[*selected[i]];
        const auto wide = detail::projectPresentDecimal(field.type, field.raw);
        snapshots[slot] = {wide.raw, detail::projectPresentBusinessDouble(field.type, field.raw)};

        return wide;
    };
    try {
        MarketMaker result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index = std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<char16_t>(raw(0))) << 32) |
                                                   static_cast<std::uint32_t>(raw(1)));
        result.bidTimeMs = time(2);
        result.bidPrice = numeric(3, 0);
        result.bidSize = numeric(4, 1);
        result.bidCount = count(5);
        result.askTimeMs = time(6);
        result.askPrice = numeric(7, 2);
        result.askSize = numeric(8, 3);
        result.askCount = count(9);
        result.businessNumericSnapshot = snapshots;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace

std::expected<MarketMaker, MappingError>
PreparedMarketMaker::map(std::u16string_view symbol, RecordValueView values, EventValueLimits limits,
                         std::int64_t eventTime, std::int32_t eventFlags,
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

    return projectMarketMaker(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags);
}

std::expected<MarketMaker, MappingError>
PreparedMarketMaker::mapView(std::u16string_view symbol, BorrowedRecordValueView values, EventValueLimits limits,
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

    return projectMarketMaker(symbol, values.values, mapping_.selections(), limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
