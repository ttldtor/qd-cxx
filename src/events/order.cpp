// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/events/order.hpp"
#include "decimal_projection.hpp"
#include "order_record_source.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/short_string.hpp"
#include "wide_integer_projection.hpp"
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

namespace {
/** Actual generated property selection order, independent of caller field positions. */
enum class OrderField : std::size_t {
    INDEX,
    TIME,
    SEQUENCE,
    TIME_NANO_PART,
    ACTION_TIME,
    ORDER_ID,
    AUX_ORDER_ID,
    PRICE,
    SIZE,
    EXECUTED_SIZE,
    COUNT,
    FLAGS,
    TRADE_ID,
    TRADE_PRICE,
    TRADE_SIZE,
    MARKET_MAKER
};

/** @param field Named generated property selection.
 * @return Dense immutable selection position, never a source/local field ID.
 */
constexpr std::size_t fieldIndex(OrderField field) noexcept {
    return static_cast<std::size_t>(field);
}

constexpr std::array<std::u16string_view, 17> KEYS{
    u"Index",      u"Time",       u"Sequence",  u"TimeNanoPart", u"ActionTime", u"OrderId",
    u"AuxOrderId", u"Price",      u"Size",      u"ExecutedSize", u"Count",      u"Flags",
    u"TradeId",    u"TradePrice", u"TradeSize", u"MMID",         u"MarketMaker"};
///< Final key is an alias, not an extra field selection.
constexpr std::size_t SELECTION_COUNT = 16;
constexpr std::array NUMERIC_SELECTIONS{OrderField::PRICE, OrderField::SIZE, OrderField::EXECUTED_SIZE,
                                        OrderField::TRADE_PRICE, OrderField::TRADE_SIZE};

/** Apply the generated Count getAsInt accessor, not a business-double-to-long substitute.
 * @param type Exact previously selected numeric type.
 * @param raw Complete canonical logical bits.
 * @return Signed32 production result widened by the Order delegate to signed64.
 */
std::int32_t orderCount(core::SerialFieldType type, std::int64_t raw) noexcept {
    if (type == core::SerialFieldType::DECIMAL) {
        const auto value = detail::projectPresentBusinessDouble(type, raw);

        if (std::isnan(value)) {
            return 0;
        }

        if (value >= 0x1p31) {
            return std::numeric_limits<std::int32_t>::max();
        }

        if (value <= -0x1p31) {
            return std::numeric_limits<std::int32_t>::min();
        }

        return static_cast<std::int32_t>(value);
    }

    const auto value = type == core::SerialFieldType::WIDE_DECIMAL ? detail::projectWideInteger(raw) : raw;

    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
}

/** Produce owning fields only after complete schema and canonical-value validation.
 * @tparam Field Existing owned or callback-borrowed canonical field representation.
 * @param symbol Complete exact event symbol, borrowed through return.
 * @param fields Complete validated local-order values.
 * @param selected Immutable property selections, including absent optional fields.
 * @param sourceId Exact independently resolved record source ID.
 * @param limits Finite output UTF-16 allowances.
 * @param eventTime Independent FIELD milliseconds.
 * @param eventFlags Actual indexed delivery flags.
 * @return Complete owning event or atomic resource/allocation refusal.
 */
template <class Field>
std::expected<Order, MappingError> projectOrder(std::u16string_view symbol, std::span<const Field> fields,
                                                std::span<const std::optional<std::size_t>> selected,
                                                std::int32_t sourceId, EventValueLimits limits, std::int64_t eventTime,
                                                std::int32_t eventFlags) {
    const auto raw = [&](OrderField field) {
        const auto i = fieldIndex(field);

        return selected[i] ? detail::projectPhysicalInt(fields[*selected[i]].type, fields[*selected[i]].raw) : 0;
    };
    const auto full = [&](OrderField field) {
        const auto i = fieldIndex(field);

        return selected[i] ? fields[*selected[i]].raw : 0;
    };
    const auto makerRaw = static_cast<std::int64_t>(raw(OrderField::MARKET_MAKER));
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

    try {
        Order result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index =
            std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(sourceId)) << 32) |
                                        static_cast<std::uint32_t>(raw(OrderField::INDEX)));
        result.timeSequence = std::bit_cast<std::int64_t>(
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(OrderField::TIME))) << 32) |
            static_cast<std::uint32_t>(raw(OrderField::SEQUENCE)));
        result.timeNanoPart = raw(OrderField::TIME_NANO_PART);
        result.actionTime = full(OrderField::ACTION_TIME);
        result.orderId = full(OrderField::ORDER_ID);
        result.auxOrderId = full(OrderField::AUX_ORDER_ID);
        result.flags = raw(OrderField::FLAGS);
        result.tradeId = full(OrderField::TRADE_ID);
        result.marketMaker = decodeShortString(makerRaw);

        if (selected[fieldIndex(OrderField::COUNT)]) {
            const auto &f = fields[*selected[fieldIndex(OrderField::COUNT)]];
            result.count = orderCount(f.type, f.raw);
        }

        std::array<detail::BusinessNumericSnapshot, 5> snapshots{};
        const std::array targets{&result.price, &result.size, &result.executedSize, &result.tradePrice,
                                 &result.tradeSize};

        for (std::size_t i = 0; i < targets.size(); ++i) {
            if (!selected[fieldIndex(NUMERIC_SELECTIONS[i])]) {
                continue;
            }

            const auto &f = fields[*selected[fieldIndex(NUMERIC_SELECTIONS[i])]];
            *targets[i] = detail::projectPresentDecimal(f.type, f.raw);
            snapshots[i] = {targets[i]->raw, detail::projectPresentBusinessDouble(f.type, f.raw)};
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

    using Type = core::SerialFieldType;
    constexpr std::array indexTypes{Type::COMPACT_INT};
    constexpr std::array longTypes{Type::LONG, Type::WIDE_DECIMAL, Type::TIME_MILLIS};
    const std::array<std::span<const Type>, SELECTION_COUNT> types{indexTypes,
                                                                   detail::RAW_INT_TYPES,
                                                                   detail::RAW_INT_TYPES,
                                                                   detail::RAW_INT_TYPES,
                                                                   longTypes,
                                                                   longTypes,
                                                                   longTypes,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::RAW_INT_TYPES,
                                                                   longTypes,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::NUMERIC_TYPES,
                                                                   detail::RAW_INT_TYPES};
    constexpr std::array<bool, SELECTION_COUNT> required{true, true,  true,  false, false, false, false, true,
                                                         true, false, false, true,  false, false, false, false};
    std::array<std::array<std::u16string_view, 2>, SELECTION_COUNT> keys{};
    std::array<FieldRequest, SELECTION_COUNT> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const bool maker = i == fieldIndex(OrderField::MARKET_MAKER);
        keys[i] = {KEYS[i], maker ? KEYS.back() : KEYS[i]};
        requests[i] = {std::span(keys[i]).first(maker ? 2 : 1), types[i], required[i]};
    }

    auto mapping = EventMapping::prepare(schema, localId, requests, limits);

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

    return projectOrder(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}

std::expected<Order, MappingError> PreparedOrder::mapView(std::u16string_view symbol, BorrowedRecordValueView values,
                                                          EventValueLimits limits, std::int64_t eventTime,
                                                          std::int32_t eventFlags) const {
    if (const auto valid = mapping_.validateView(values); !valid) {
        return std::unexpected(valid.error());
    }

    return projectOrder(symbol, values.values, mapping_.selections(), sourceId_, limits, eventTime, eventFlags);
}
} // namespace org::ttldtor::qdcxx::events
