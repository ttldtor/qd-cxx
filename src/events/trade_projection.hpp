// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "decimal_projection.hpp"
#include "qd/events/market_event_symbols.hpp"
#include "qd/events/trade.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace org::ttldtor::qdcxx::events::detail {
/** Generated selection positions after the common Time/Sequence/Nano/Exchange/Id/Price/Size prefix. */
struct TradeProjectionLayout {
    std::optional<std::size_t> tick; ///< Regular Trade fallback only; TradeETH has no Tick accessor.
    std::size_t change;              ///< Business price change selection.
    std::size_t dayId;               ///< Raw trading day selection.
    std::size_t volume;              ///< Business day volume selection.
    std::size_t turnover;            ///< Business day turnover selection.
    std::size_t flags;               ///< Complete raw flags selection.
};

inline constexpr TradeProjectionLayout TRADE_LAYOUT{7, 8, 9, 10, 11, 12};
///< Thirteen regular Trade property selections.
inline constexpr TradeProjectionLayout TRADE_ETH_LAYOUT{std::nullopt, 7, 8, 9, 10, 11};

///< Twelve extended-hours selections, without synthetic Tick or flag adjustment.

/** @param packed Signed seconds/high32 and packed Sequence/low32.
 * @return Business milliseconds including the unsigned ten-bit millisecond component.
 */
inline std::int64_t tradeTime(std::int64_t packed) noexcept {
    constexpr std::int64_t MILLIS_PER_SECOND = 1000;
    const auto seconds = packed >> 32;
    const auto millis = (static_cast<std::uint64_t>(packed) >> 22) & 0x3ff;

    return seconds * MILLIS_PER_SECOND + static_cast<std::int64_t>(millis);
}

/** @param packed Complete time/sequence word.
 * @param nano Raw signed nanosecond remainder, without clamping.
 * @return Java long-wrapped business nanoseconds.
 */
inline std::int64_t tradeTimeNanos(std::int64_t packed, std::int32_t nano) noexcept {
    constexpr std::uint64_t NANOS_PER_MILLI = 1000000;

    return std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(tradeTime(packed)) * NANOS_PER_MILLI +
                                       static_cast<std::uint64_t>(nano));
}

/** @param packed Complete time/sequence word.
 * @return Low22 sequence bits without uniqueness or ordering guarantees.
 */
inline std::int32_t tradeSequence(std::int64_t packed) noexcept {
    constexpr std::uint32_t SEQUENCE_MASK = (std::uint32_t{1} << 22) - 1;

    return static_cast<std::uint32_t>(packed) & SEQUENCE_MASK;
}

/** @param value Source business binary64 size or volume.
 * @return Java narrowing: truncate, saturate and map NaN to zero.
 */
inline std::int64_t tradeInteger(double value) noexcept {
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

/** @param flags Complete source flags.
 * @return Java direction code in bits1..3, with reserved6/7 exposed as UNDEFINED.
 */
inline Direction tradeDirection(std::int32_t flags) noexcept {
    const auto direction = (static_cast<std::uint32_t>(flags) >> 1) & 7;

    return direction <= std::to_underlying(Direction::UP) ? static_cast<Direction>(direction) : Direction::UNDEFINED;
}

/** @param flags Complete source flags, independent of the event's type.
 * @return Raw ETH bit0, including false for a TradeETH RTH transition.
 */
inline bool tradeExtendedHours(std::int32_t flags) noexcept {
    return (flags & 1) != 0;
}

/** Build a complete Trade from previously validated complete fields.
 * @tparam Event Flat owning Trade or TradeETH with the common initialized members.
 * @tparam Field Canonical owned or borrowed field representation.
 * @param symbol Exact physical symbol, alive through this call.
 * @param fields Complete validated local-order fields.
 * @param selected Retained immutable property positions in request order.
 * @param symbolUnits Maximum owned symbol units.
 * @param eventTime Independent event metadata milliseconds.
 * @param exchange Syntactic record exchange, zero for composite.
 * @param layout Actual generated field positions; absent Tick disables the regular-Trade fallback.
 * @return Independent complete event or resource/allocation refusal.
 */
template <class Event, class Field>
std::expected<Event, MappingError> projectTradeLike(std::u16string_view symbol, std::span<const Field> fields,
                                                    std::span<const std::optional<std::size_t>> selected,
                                                    std::size_t symbolUnits, std::int64_t eventTime, char16_t exchange,
                                                    const TradeProjectionLayout &layout) {
    if (symbol.size() > symbolUnits ||
        (exchange && !market_symbols::hasExchangeCode(symbol) && symbolUnits - symbol.size() < 2))
        return std::unexpected(MappingError::RESOURCE_LIMIT);

    const auto raw = [&](std::size_t index) -> std::int32_t {
        if (!selected[index])
            return 0;

        const auto &field = fields[*selected[index]];

        return projectPhysicalInt(field.type, field.raw);
    };
    std::array<BusinessNumericSnapshot, 5> business{};
    const auto numeric = [&](std::size_t index, std::size_t property) -> core::WideDecimal {
        if (!selected[index])
            return {0};

        const auto &field = fields[*selected[index]];
        const auto wide = projectPresentDecimal(field.type, field.raw);
        business[property] = {wide.raw, projectPresentBusinessDouble(field.type, field.raw)};

        return wide;
    };
    try {
        Event result;
        result.symbol = exchange ? *market_symbols::changeExchangeCode(symbol, exchange) : std::u16string(symbol);
        result.eventTime = eventTime;
        const auto packed =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(raw(0))) << 32) | static_cast<std::uint32_t>(raw(1));
        result.timeSequence = std::bit_cast<std::int64_t>(packed);
        result.timeNanoPart = raw(2);
        result.exchangeCode =
            selected[3] ? static_cast<char16_t>(static_cast<std::uint32_t>(raw(3)) & 0xffff) : exchange;
        result.tradeId = selected[4] ? fields[*selected[4]].raw : 0;
        result.price = numeric(5, 0);
        result.size = numeric(6, 1);
        result.change = numeric(layout.change, 2);
        result.dayId = raw(layout.dayId);
        result.dayVolume = numeric(layout.volume, 3);
        result.dayTurnover = numeric(layout.turnover, 4);
        result.flags = raw(layout.flags);

        if (layout.tick && result.getTickDirection() == Direction::UNDEFINED) {
            const auto tick = raw(*layout.tick);

            if (tick == 1 || tick == 2) {
                const auto direction = tick == 1 ? Direction::ZERO_UP : Direction::ZERO_DOWN;
                result.flags = std::bit_cast<std::int32_t>((static_cast<std::uint32_t>(result.flags) & ~0xeU) |
                                                           (static_cast<std::uint32_t>(direction) << 1));
            }
        }

        result.businessNumericSnapshot = business;

        return result;
    } catch (const std::bad_alloc &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    } catch (const std::length_error &) {
        return std::unexpected(MappingError::ALLOCATION_FAILURE);
    }
}
} // namespace org::ttldtor::qdcxx::events::detail
