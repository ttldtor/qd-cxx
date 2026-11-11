// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/order.hpp"
#include "order_fields.hpp"
#include "order_record_source.hpp"
#include "qd/events/event_flags.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events {
std::int64_t Order::getIndex() const noexcept {
    return index;
}

std::int32_t Order::getSourceId() const noexcept {
    return orderIndexSourceId(index);
}

OrderSource Order::getSource() const {
    return orderIndexSource(index);
}

std::int64_t Order::getTime() const noexcept {
    constexpr std::int64_t MILLIS_PER_SECOND = 1000;
    constexpr unsigned MILLIS_SHIFT = 22;
    constexpr std::uint64_t MILLIS_MASK = 0x3ff;

    return (timeSequence >> 32) * MILLIS_PER_SECOND +
           static_cast<std::int64_t>((static_cast<std::uint64_t>(timeSequence) >> MILLIS_SHIFT) & MILLIS_MASK);
}

std::int64_t Order::getTimeNanos() const noexcept {
    constexpr std::uint64_t NANOS_PER_MILLI = 1000000;

    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(getTime()) * NANOS_PER_MILLI +
                                       static_cast<std::uint64_t>(timeNanoPart));
}

std::int32_t Order::getSequence() const noexcept {
    constexpr std::uint32_t SEQUENCE_MASK = (1U << 22) - 1;

    return static_cast<std::uint32_t>(timeSequence) & SEQUENCE_MASK;
}

double Order::getPrice() const noexcept {
    return detail::businessValue(price, businessNumericSnapshot, 0);
}

double Order::getSizeAsDouble() const noexcept {
    return detail::businessValue(size, businessNumericSnapshot, 1);
}

std::int64_t Order::getSize() const noexcept {
    const auto value = getSizeAsDouble();

    if (std::isnan(value)) {
        return 0;
    }

    if (value >= 0x1p63) {
        return std::numeric_limits<std::int64_t>::max();
    }

    if (value <= -0x1p63) {
        return std::numeric_limits<std::int64_t>::min();
    }

    return static_cast<std::int64_t>(value);
}

bool Order::hasSize() const noexcept {
    const auto value = getSizeAsDouble();

    return value != 0 && !std::isnan(value);
}

double Order::getExecutedSize() const noexcept {
    return detail::businessValue(executedSize, businessNumericSnapshot, 2);
}

std::int64_t Order::getCount() const noexcept {
    return count;
}

double Order::getTradePrice() const noexcept {
    return detail::businessValue(tradePrice, businessNumericSnapshot, 3);
}

double Order::getTradeSize() const noexcept {
    return detail::businessValue(tradeSize, businessNumericSnapshot, 4);
}

char16_t Order::getExchangeCode() const noexcept {
    return static_cast<char16_t>(getBits(flags, 0x7f, 4));
}

Side Order::getOrderSide() const noexcept {
    return marketEnumValue(marketEnumFromCode<Side>(getBits(flags, 3, 2))).value_or(Side::UNDEFINED);
}

Scope Order::getScope() const noexcept {
    return marketEnumFromCode<Scope>(getBits(flags, 3, 0));
}

OrderAction Order::getAction() const noexcept {
    return marketEnumValue(marketEnumFromCode<OrderAction>(getBits(flags, 0xf, 11))).value_or(OrderAction::UNDEFINED);
}

std::expected<core::LocalSchema, core::SchemaError> defaultOrderSchema(core::SchemaLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array names{u"Index", u"Time", u"Sequence", u"Price", u"Size", u"Count", u"Flags", u"MMID"};
    constexpr std::array types{Type::COMPACT_INT,  Type::TIME_SECONDS, Type::SEQUENCE,    Type::WIDE_DECIMAL,
                               Type::WIDE_DECIMAL, Type::WIDE_DECIMAL, Type::COMPACT_INT, Type::SHORT_STRING};
    try {
        core::LocalRecord record{0, u"Order", true, {}, {}, core::IndexedTimeLowInt32{0}};

        for (std::size_t i = 0; i < names.size(); ++i) {
            record.fields.push_back({std::to_underlying(types[i])});
            record.lookups.push_back({names[i], i});
        }

        record.lookups.push_back({u"MarketMaker", 7});
        const std::array records{std::move(record)};

        return core::LocalSchema::create(records, limits, core::LocalProfile::BINARY_FIELDS_V1);
    } catch (const std::bad_alloc &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(core::SchemaError::ALLOCATION_FAILURE);
    }
}

PreparedOrder::PreparedOrder(EventMapping mapping, std::int32_t sourceId) noexcept
    : mapping_(std::move(mapping)), sourceId_(sourceId) {
}

std::expected<PreparedOrder, MappingError> PreparedOrder::prepare(const core::LocalSchema &schema, std::int32_t localId,
                                                                  MappingLimits limits) {
    if (!schema.recordContext(localId).valid()) {
        return std::unexpected(MappingError::INVALID_INPUT);
    }

    auto source = detail::prepareRecordOrderSource(schema.records()[static_cast<std::size_t>(localId)].name, limits);

    if (!source) {
        return std::unexpected(source.error());
    }

    auto mapping = detail::order_fields::prepare(schema, localId, limits, std::array<FieldRequest, 0>{});

    if (!mapping) {
        return std::unexpected(mapping.error());
    }

    return PreparedOrder(std::move(*mapping), source->id());
}

const core::RecordContext &PreparedOrder::context() const noexcept {
    return mapping_.context();
}

std::int32_t PreparedOrder::sourceId() const noexcept {
    return sourceId_;
}

std::expected<Order, MappingError> PreparedOrder::map(std::u16string_view symbol, RecordValueView values,
                                                      EventValueLimits limits, std::int64_t eventTime,
                                                      std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validate(values); !valid) {
        return std::unexpected(valid.error());
    }

    return detail::order_fields::projectOrder(symbol, values.values, mapping_.selections(), sourceId_, limits,
                                              eventTime, eventFlags);
}

std::expected<Order, MappingError> PreparedOrder::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                          EventValueLimits limits, std::int64_t eventTime,
                                                          std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return detail::order_fields::projectOrder(symbol, values.values, mapping_.selections(), sourceId_, limits,
                                              eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
