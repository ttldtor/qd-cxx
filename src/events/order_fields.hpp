// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "decimal_projection.hpp"
#include "numeric_int_projection.hpp"
#include "qd/events/order.hpp"
#include "qd/events/short_string.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>

namespace org::ttldtor::qdcxx::events::detail {
namespace order_fields {
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

inline constexpr std::array<std::u16string_view, 17> KEYS{
    u"Index",      u"Time",       u"Sequence",  u"TimeNanoPart", u"ActionTime", u"OrderId",
    u"AuxOrderId", u"Price",      u"Size",      u"ExecutedSize", u"Count",      u"Flags",
    u"TradeId",    u"TradePrice", u"TradeSize", u"MMID",         u"MarketMaker"};
///< Final key is an alias, not an extra field selection.
constexpr std::size_t SELECTION_COUNT = 16;
inline constexpr std::array NUMERIC_SELECTIONS{OrderField::PRICE, OrderField::SIZE, OrderField::EXECUTED_SIZE,
                                               OrderField::TRADE_PRICE, OrderField::TRADE_SIZE};

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
            result.count = detail::projectPresentInt(f.type, f.raw);
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

/** Select ordinary order fields together with required variant fields in one bounded prepare.
 * @tparam ExtraCount Number of appended variant field selections.
 * @param schema Immutable source schema, borrowed only through return.
 * @param localId Explicit selected record.
 * @param limits Finite preparation allowances after any source-name inspection.
 * @param extra Variant field requests, borrowed only through return.
 * @return Complete owning shape and selections, or original refusal.
 */
template <std::size_t ExtraCount>
std::expected<EventMapping, MappingError> prepare(const core::LocalSchema &schema, std::int32_t localId,
                                                  MappingLimits limits,
                                                  const std::array<FieldRequest, ExtraCount> &extra) {
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
    std::array<FieldRequest, SELECTION_COUNT + ExtraCount> requests{};

    for (std::size_t i = 0; i < SELECTION_COUNT; ++i) {
        const bool maker = i == fieldIndex(OrderField::MARKET_MAKER);
        keys[i] = {KEYS[i], maker ? KEYS.back() : KEYS[i]};
        requests[i] = {std::span(keys[i]).first(maker ? 2 : 1), types[i], required[i]};
    }

    std::ranges::copy(extra, requests.begin() + SELECTION_COUNT);

    return EventMapping::prepare(schema, localId, requests, limits);
}
} // namespace order_fields
} // namespace org::ttldtor::qdcxx::events::detail
