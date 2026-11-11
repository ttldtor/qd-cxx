// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
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

namespace org::ttldtor::qdcxx::events::detail {
/** Actual generated property selection order, independent of caller field positions. */
enum class VariantField : std::size_t {
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
    TEXT,
    ICEBERG_PEAK_SIZE,
    ICEBERG_HIDDEN_SIZE,
    ICEBERG_EXECUTED_SIZE,
    ICEBERG_FLAGS
};

/** @param field Named generated property selection.
 * @return Dense immutable selection position, never a source/local field ID.
 */
constexpr std::size_t fieldIndex(VariantField field) noexcept {
    return static_cast<std::size_t>(field);
}

inline constexpr std::array<std::u16string_view, 21> VARIANT_KEYS{u"Index",
                                                                  u"Time",
                                                                  u"Sequence",
                                                                  u"TimeNanoPart",
                                                                  u"ActionTime",
                                                                  u"OrderId",
                                                                  u"AuxOrderId",
                                                                  u"Price",
                                                                  u"Size",
                                                                  u"ExecutedSize",
                                                                  u"Count",
                                                                  u"Flags",
                                                                  u"TradeId",
                                                                  u"TradePrice",
                                                                  u"TradeSize",
                                                                  u"MMID",
                                                                  u"IcebergPeakSize",
                                                                  u"IcebergHiddenSize",
                                                                  u"IcebergExecutedSize",
                                                                  u"IcebergFlags",
                                                                  u"MarketMaker"};
inline constexpr std::array NUMERIC_SELECTIONS{VariantField::PRICE, VariantField::SIZE, VariantField::EXECUTED_SIZE,
                                               VariantField::TRADE_PRICE, VariantField::TRADE_SIZE};

/** Apply the generated Count getAsInt accessor, not a business-double-to-long substitute.
 * @param type Exact previously selected numeric type.
 * @param raw Complete canonical logical bits.
 * @return Signed32 production result widened by the Order delegate to signed64.
 */
inline std::int32_t variantCount(core::SerialFieldType type, std::int64_t raw) noexcept {
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
template <class Event, bool Spread, class Field>
std::expected<Event, MappingError> projectVariant(std::u16string_view symbol, std::span<const Field> fields,
                                                  std::span<const std::optional<std::size_t>> selected,
                                                  std::int32_t sourceId, EventValueLimits limits,
                                                  std::int64_t eventTime, std::int32_t eventFlags) {
    const auto raw = [&](VariantField field) {
        const auto i = fieldIndex(field);

        return selected[i] ? detail::projectPhysicalInt(fields[*selected[i]].type, fields[*selected[i]].raw) : 0;
    };
    const auto full = [&](VariantField field) {
        const auto i = fieldIndex(field);

        return selected[i] ? fields[*selected[i]].raw : 0;
    };
    std::int64_t makerRaw = 0;
    std::optional<std::u16string_view> spreadText;
    std::size_t textUnits = 0;

    if constexpr (Spread) {
        const auto &field = fields[*selected[fieldIndex(VariantField::TEXT)]];

        if (field.units) {
            spreadText = std::u16string_view(*field.units);
            textUnits = spreadText->size();
        }
    } else {
        makerRaw = raw(VariantField::TEXT);
        auto bits = static_cast<std::uint64_t>(makerRaw);

        while (bits) {
            textUnits += (bits & 0xff) != 0;
            bits >>= 8;
        }
    }

    if (symbol.size() > limits.symbolUnits || symbol.size() > limits.totalUnits || textUnits > limits.stringUnits ||
        textUnits > limits.totalUnits - symbol.size()) {
        return std::unexpected(MappingError::RESOURCE_LIMIT);
    }

    try {
        Event result;
        result.symbol = symbol;
        result.eventTime = eventTime;
        result.eventFlags = eventFlags;
        result.index =
            std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(static_cast<std::uint32_t>(sourceId)) << 32) |
                                        static_cast<std::uint32_t>(raw(VariantField::INDEX)));
        result.timeSequence = std::bit_cast<std::int64_t>(
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(VariantField::TIME))) << 32) |
            static_cast<std::uint32_t>(raw(VariantField::SEQUENCE)));
        result.timeNanoPart = raw(VariantField::TIME_NANO_PART);
        result.actionTime = full(VariantField::ACTION_TIME);
        result.orderId = full(VariantField::ORDER_ID);
        result.auxOrderId = full(VariantField::AUX_ORDER_ID);
        result.flags = raw(VariantField::FLAGS);
        result.tradeId = full(VariantField::TRADE_ID);

        if constexpr (Spread) {
            if (spreadText) {
                result.spreadSymbol = std::u16string(*spreadText);
            }
        } else {
            result.marketMaker = decodeShortString(makerRaw);
        }

        if (selected[fieldIndex(VariantField::COUNT)]) {
            const auto &f = fields[*selected[fieldIndex(VariantField::COUNT)]];
            result.count = variantCount(f.type, f.raw);
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

        if constexpr (!Spread) {
            std::array<BusinessNumericSnapshot, 3> icebergSnapshots{};
            const std::array icebergTargets{&result.icebergPeakSize, &result.icebergHiddenSize,
                                            &result.icebergExecutedSize};
            constexpr std::array icebergSelections{VariantField::ICEBERG_PEAK_SIZE, VariantField::ICEBERG_HIDDEN_SIZE,
                                                   VariantField::ICEBERG_EXECUTED_SIZE};

            for (std::size_t i = 0; i < icebergTargets.size(); ++i) {
                if (!selected[fieldIndex(icebergSelections[i])]) {
                    continue;
                }

                const auto &f = fields[*selected[fieldIndex(icebergSelections[i])]];
                *icebergTargets[i] = projectPresentDecimal(f.type, f.raw);
                icebergSnapshots[i] = {icebergTargets[i]->raw, projectPresentBusinessDouble(f.type, f.raw)};
            }

            result.icebergNumericSnapshot = icebergSnapshots;
            result.icebergFlags = raw(VariantField::ICEBERG_FLAGS);
        }

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}

/** Prepare all selected base and variant fields in one bounded mapping transaction.
 * @tparam Spread True selects required nullable text; false selects optional MMID and iceberg fields.
 * @param schema Validated source borrowed only through return.
 * @param localId Explicit selected record.
 * @param limits Finite preparation allowances, already charged for source-name inspection.
 * @return Independently owned complete shape/selections or atomic original refusal.
 */
template <bool Spread>
std::expected<EventMapping, MappingError> prepareVariant(const core::LocalSchema &schema, std::int32_t localId,
                                                         MappingLimits limits) {
    using Type = core::SerialFieldType;
    constexpr std::array indexTypes{Type::COMPACT_INT};
    constexpr std::array longTypes{Type::LONG, Type::WIDE_DECIMAL, Type::TIME_MILLIS};
    constexpr std::array textTypes{Type::UTF_CHAR_ARRAY, Type::STRING};
    std::array<std::span<const Type>, 20> types{
        indexTypes,    RAW_INT_TYPES, RAW_INT_TYPES, RAW_INT_TYPES, longTypes,     longTypes,    longTypes,
        NUMERIC_TYPES, NUMERIC_TYPES, NUMERIC_TYPES, NUMERIC_TYPES, RAW_INT_TYPES, longTypes,    NUMERIC_TYPES,
        NUMERIC_TYPES, RAW_INT_TYPES, NUMERIC_TYPES, NUMERIC_TYPES, NUMERIC_TYPES, RAW_INT_TYPES};
    std::array<bool, 20> required{true,  true, true,  false, false, false, false, true,  true,  false,
                                  false, true, false, false, false, false, false, false, false, false};
    constexpr std::size_t SELECTION_COUNT = Spread ? 16 : 20;

    if constexpr (Spread) {
        types[fieldIndex(VariantField::TEXT)] = textTypes;
        required[fieldIndex(VariantField::TEXT)] = true;
    }

    std::array<std::array<std::u16string_view, 2>, SELECTION_COUNT> keys{};
    std::array<FieldRequest, SELECTION_COUNT> requests{};

    for (std::size_t i = 0; i < requests.size(); ++i) {
        const bool maker = !Spread && i == fieldIndex(VariantField::TEXT);
        const auto key = Spread && i == fieldIndex(VariantField::TEXT) ? u"SpreadSymbol" : VARIANT_KEYS[i];
        keys[i] = {key, maker ? VARIANT_KEYS.back() : key};
        requests[i] = {std::span(keys[i]).first(maker ? 2 : 1), types[i], required[i]};
    }

    return EventMapping::prepare(schema, localId, requests, limits);
}
} // namespace org::ttldtor::qdcxx::events::detail
