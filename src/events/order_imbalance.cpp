// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/order_imbalance.hpp"
#include "decimal_projection.hpp"
#include "qd/events/time_helpers.hpp"
#include "sourced_market_record.hpp"
#include <array>
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
namespace {
enum class Field : std::size_t { TIME, SEQUENCE, REF_PRICE, PAIRED_SIZE, IMBALANCE_SIZE, NEAR_PRICE, FAR_PRICE, FLAGS };

constexpr std::array<std::u16string_view, 8> NAMES{u"Time",          u"Sequence",  u"RefPrice", u"PairedSize",
                                                   u"ImbalanceSize", u"NearPrice", u"FarPrice", u"Flags"};
constexpr std::array NUMERIC_FIELDS{Field::REF_PRICE, Field::PAIRED_SIZE, Field::IMBALANCE_SIZE, Field::NEAR_PRICE,
                                    Field::FAR_PRICE};
constexpr std::uint32_t SIDE_MASK = 7U;
constexpr std::uint32_t AUCTION_MASK = 7U;
constexpr unsigned AUCTION_SHIFT = 3;

/** @param field Selection identity in generated property order.
 * @return Dense selection offset, independent of local wire order.
 */
constexpr std::size_t position(Field field) noexcept {
    return std::to_underlying(field);
}

/** Construct one independent owning event from validated field data.
 * @tparam Value Owning or borrowed canonical scalar representation.
 * @param symbol Exact unsuffixed event symbol.
 * @param values Complete validated local-order fields.
 * @param selections Resolved selections including absent optional fields.
 * @param sourceId Validated named-record source.
 * @param limits Output symbol and total UTF-16 bounds.
 * @param eventTime Independent FIELD milliseconds.
 * @return Complete event or resource/allocation refusal.
 */
template <class Value>
std::expected<OrderImbalance, MappingError> project(std::u16string_view symbol, std::span<const Value> values,
                                                    std::span<const std::optional<std::size_t>> selections,
                                                    std::int32_t sourceId, EventValueLimits limits,
                                                    std::int64_t eventTime) {
    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    const auto raw = [&](Field field) {
        const auto selected = selections[position(field)];

        return selected ? detail::projectPhysicalInt(values[*selected].type, values[*selected].raw) : 0;
    };
    constexpr unsigned SECONDS_SHIFT = 32;
    const auto high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(Field::TIME)));
    const auto low = static_cast<std::uint32_t>(raw(Field::SEQUENCE));

    try {
        OrderImbalance result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.sourceId = sourceId;
        result.timeSequence = std::bit_cast<std::int64_t>((high << SECONDS_SHIFT) | low);
        result.flags = raw(Field::FLAGS);

        std::array<detail::BusinessNumericSnapshot, NUMERIC_FIELDS.size()> snapshots{};
        const std::array targets{&result.refPrice, &result.pairedSize, &result.imbalanceSize, &result.nearPrice,
                                 &result.farPrice};

        for (std::size_t i = 0; i < NUMERIC_FIELDS.size(); ++i) {
            if (const auto selected = selections[position(NUMERIC_FIELDS[i])]) {
                const auto &value = values[*selected];
                *targets[i] = detail::projectPresentDecimal(value.type, value.raw);
                snapshots[i] = {targets[i]->raw, detail::projectPresentBusinessDouble(value.type, value.raw)};
            }
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

std::int32_t OrderImbalance::getSourceId() const noexcept {
    return sourceId;
}

OrderSource OrderImbalance::getSource() const {
    return OrderSource::valueOf(sourceId);
}

std::int64_t OrderImbalance::getTime() const noexcept {
    return getTimeFromTimeSequence(timeSequence);
}

std::int32_t OrderImbalance::getSequence() const noexcept {
    return getSequenceFromTimeSequence(timeSequence);
}

double OrderImbalance::getRefPrice() const noexcept {
    return detail::businessValue(refPrice, businessNumericSnapshot, 0);
}

double OrderImbalance::getPairedSize() const noexcept {
    return detail::businessValue(pairedSize, businessNumericSnapshot, 1);
}

double OrderImbalance::getImbalanceSize() const noexcept {
    return detail::businessValue(imbalanceSize, businessNumericSnapshot, 2);
}

double OrderImbalance::getNearPrice() const noexcept {
    return detail::businessValue(nearPrice, businessNumericSnapshot, 3);
}

double OrderImbalance::getFarPrice() const noexcept {
    return detail::businessValue(farPrice, businessNumericSnapshot, 4);
}

ImbalanceSide OrderImbalance::getImbalanceSide() const noexcept {
    const auto code = static_cast<std::uint32_t>(flags) & SIDE_MASK;

    return marketEnumValue(marketEnumFromCode<ImbalanceSide>(code)).value_or(ImbalanceSide::UNDEFINED);
}

AuctionType OrderImbalance::getAuctionType() const noexcept {
    const auto code = (static_cast<std::uint32_t>(flags) >> AUCTION_SHIFT) & AUCTION_MASK;

    return marketEnumValue(marketEnumFromCode<AuctionType>(code)).value_or(AuctionType::UNDEFINED);
}

std::expected<core::LocalSchema, core::SchemaError> defaultOrderImbalanceSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array types{Type::TIME_SECONDS, Type::SEQUENCE,     Type::WIDE_DECIMAL, Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT};
    constexpr std::array recordsNames{u"OrderImbalance", u"OrderImbalance#NTV", u"OrderImbalance#ARCA",
                                      u"OrderImbalance#NUAM"};

    try {
        std::array<core::LocalRecord, recordsNames.size()> records{};

        for (std::size_t id = 0; id < records.size(); ++id) {
            auto &record = records[id];
            record = core::LocalRecord{static_cast<std::int32_t>(id), recordsNames[id], false, {}, {}};

            for (std::size_t i = 0; i < NAMES.size(); ++i) {
                record.fields.push_back({std::to_underlying(types[i])});
                record.lookups.push_back({std::u16string(NAMES[i]), i});
            }
        }

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedOrderImbalance::PreparedOrderImbalance(EventMapping mapping, std::int32_t sourceId) noexcept
    : mapping_(std::move(mapping)), sourceId_(sourceId) {
}

std::expected<PreparedOrderImbalance, MappingError>
PreparedOrderImbalance::prepare(const core::LocalSchema &schema, std::int32_t localId, MappingLimits limits) {
    const auto source = detail::prepareSourcedMarketRecord(schema, localId, u"OrderImbalance", limits);

    if (!source) {
        return std::unexpected(source.error());
    }

    constexpr std::array<bool, NAMES.size()> required{false, false, true, true, true, false, false, true};
    std::array<FieldRequest, NAMES.size()> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const bool numeric = i >= position(Field::REF_PRICE) && i <= position(Field::FAR_PRICE);
        const auto types = numeric ? std::span<const core::SerialFieldType>(detail::NUMERIC_TYPES)
                                   : std::span<const core::SerialFieldType>(detail::RAW_INT_TYPES);
        requests[i] = {std::span(NAMES).subspan(i, 1), types, required[i]};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedOrderImbalance(std::move(*mapping), *source);
}

const core::RecordContext &PreparedOrderImbalance::context() const noexcept {
    return mapping_.context();
}

std::int32_t PreparedOrderImbalance::sourceId() const noexcept {
    return sourceId_;
}

std::expected<OrderImbalance, MappingError> PreparedOrderImbalance::map(std::u16string_view symbol,
                                                                        RecordValueView values, EventValueLimits limits,
                                                                        std::int64_t eventTime) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime);
}

std::expected<OrderImbalance, MappingError> PreparedOrderImbalance::mapView(std::u16string_view symbol,
                                                                            BorrowedRecordValueView values,
                                                                            EventValueLimits limits,
                                                                            std::int64_t eventTime) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return project(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime);
}
} // namespace org::ttldtor::qdcxx::events
