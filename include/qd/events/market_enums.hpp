// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

#include "qd/events/event_value.hpp"

#include <concepts>
#include <cstdint>
#include <expected>

namespace org::ttldtor::qdcxx::events {
/** Trade-price movement; arbitrary underlying codes retain their complete raw value. */
enum class Direction : std::int32_t {
    UNDEFINED = 0, ///< Unknown or uncomputed movement.
    DOWN = 1,      ///< Lower than the preceding price.
    ZERO_DOWN = 2, ///< Unchanged after a downward movement.
    ZERO = 3,      ///< Equal to the only available comparison price.
    ZERO_UP = 4,   ///< Unchanged after an upward movement.
    UP = 5         ///< Higher than the preceding price.
};
/** Trading state; reserved codes project to UNDEFINED. */
enum class TradingStatus : std::int32_t {
    UNDEFINED = 0, ///< Unspecified trading state.
    HALTED = 1,    ///< Trading is halted.
    ACTIVE = 2     ///< Trading is active.
};
/** Short-sale restriction; reserved codes project to UNDEFINED. */
enum class ShortSaleRestriction : std::int32_t {
    UNDEFINED = 0, ///< Unspecified restriction state.
    ACTIVE = 1,    ///< Restriction is active.
    INACTIVE = 2   ///< Restriction is inactive.
};
/** Order side; reserved codes project to UNDEFINED. */
enum class Side : std::int32_t {
    UNDEFINED = 0, ///< Unspecified side.
    BUY = 1,       ///< Buy side.
    SELL = 2       ///< Sell side.
};
/** Aggregation scope of a market event. */
enum class Scope : std::int32_t {
    COMPOSITE = 0, ///< Composite market information.
    REGIONAL = 1,  ///< Regional exchange information.
    AGGREGATE = 2, ///< Aggregated order information.
    ORDER = 3      ///< Individual order information.
};
/** Full-order-book action; reserved codes project to UNDEFINED. */
enum class OrderAction : std::int32_t {
    UNDEFINED = 0,    ///< Unspecified action.
    NEW = 1,          ///< New order.
    REPLACE = 2,      ///< Replace an order.
    MODIFY = 3,       ///< Modify an order.
    DELETE_ORDER = 4, ///< Delete an order; Java DELETE, with a Windows-macro-safe C++ spelling.
    PARTIAL = 5,      ///< Partial execution.
    EXECUTE = 6,      ///< Full execution.
    TRADE = 7,        ///< Non-book trade.
    BUST = 8          ///< Cancel a previously reported trade.
};
/** Iceberg-order classification; reserved codes project to UNDEFINED. */
enum class IcebergType : std::int32_t {
    UNDEFINED = 0, ///< No iceberg classification.
    NATIVE = 1,    ///< Exchange-native iceberg.
    SYNTHETIC = 2  ///< Synthetic iceberg.
};
/** Classification of a price. */
enum class PriceType : std::int32_t {
    REGULAR = 0,     ///< Regular price.
    INDICATIVE = 1,  ///< Indicative price.
    PRELIMINARY = 2, ///< Preliminary price.
    FINAL = 3        ///< Final price.
};
/** OTC quotation price classification; reserved codes project to UNPRICED. */
enum class OtcMarketsPriceType : std::int32_t {
    UNPRICED = 0, ///< No price.
    ACTUAL = 1,   ///< Actual quoted price.
    WANTED = 2    ///< Bid wanted or offer wanted.
};
/** Time-and-sale correction classification; reserved codes project to NEW. */
enum class TimeAndSaleType : std::int32_t {
    NEW = 0,        ///< New report.
    CORRECTION = 1, ///< Correct an earlier report.
    CANCEL = 2      ///< Cancel an earlier report.
};
/** Auction classification; reserved codes project to UNDEFINED. */
enum class AuctionType : std::int32_t {
    UNDEFINED = 0, ///< Unspecified auction.
    OPENING = 1,   ///< Opening auction.
    CLOSING = 2,   ///< Closing auction.
    REOPENING = 3, ///< Reopening auction.
    OTHER = 4      ///< Other auction.
};
/** Auction imbalance state; reserved codes project to UNDEFINED. */
enum class ImbalanceSide : std::int32_t {
    UNDEFINED = 0,           ///< Unspecified imbalance.
    BUY = 1,                 ///< Buy-side imbalance.
    SELL = 2,                ///< Sell-side imbalance.
    NO_IMBALANCE = 3,        ///< No imbalance.
    INSUFFICIENT_ORDERS = 4, ///< Too few orders to establish a match.
    PAUSED = 5               ///< Auction is paused.
};

/** The complete supported market business-enum set.
 * @tparam Enum One of the twelve declared market enum types; cv-qualified types are not admitted.
 */
template <typename Enum>
concept MarketEnum =
    std::same_as<Enum, Direction> || std::same_as<Enum, TradingStatus> || std::same_as<Enum, ShortSaleRestriction> ||
    std::same_as<Enum, Side> || std::same_as<Enum, Scope> || std::same_as<Enum, OrderAction> ||
    std::same_as<Enum, IcebergType> || std::same_as<Enum, PriceType> || std::same_as<Enum, OtcMarketsPriceType> ||
    std::same_as<Enum, TimeAndSaleType> || std::same_as<Enum, AuctionType> || std::same_as<Enum, ImbalanceSide>;

namespace detail {
/** @tparam Enum Supported market enum.
 * @return Number of explicitly named contiguous codes, starting at zero.
 */
template <MarketEnum Enum> constexpr std::int32_t marketEnumCount() noexcept {
    if constexpr (std::same_as<Enum, Direction> || std::same_as<Enum, ImbalanceSide>) {
        return 6;
    } else if constexpr (std::same_as<Enum, OrderAction>) {
        return 9;
    } else if constexpr (std::same_as<Enum, AuctionType>) {
        return 5;
    } else if constexpr (std::same_as<Enum, Scope> || std::same_as<Enum, PriceType>) {
        return 4;
    } else {
        return 3;
    }
}

/** @tparam Enum Supported market enum.
 * @return Power-of-two size of the production bit-pattern lookup table.
 */
template <MarketEnum Enum> constexpr std::int32_t marketEnumTableSize() noexcept {
    if constexpr (std::same_as<Enum, OrderAction>) {
        return 16;
    } else if constexpr (std::same_as<Enum, Direction> || std::same_as<Enum, ImbalanceSide> ||
                         std::same_as<Enum, AuctionType>) {
        return 8;
    } else {
        return 4;
    }
}
} // namespace detail

/** Retain any signed32 code, including reserved and out-of-table values, without validation or projection.
 * @tparam Enum Supported market enum with a fixed signed32 underlying representation.
 * @param code Complete raw numeric code, not a masked business projection.
 * @return Enum holding exactly code; the result need not name a known enumerator.
 */
template <MarketEnum Enum> [[nodiscard]] constexpr Enum marketEnumFromCode(std::int32_t code) noexcept {
    return static_cast<Enum>(code);
}

/** Recover an enum's unmodified raw code.
 * @tparam Enum Supported market enum.
 * @param value Known or unknown raw enum value.
 * @return Exact signed32 code; no sentinel conversion occurs.
 */
template <MarketEnum Enum> [[nodiscard]] constexpr std::int32_t marketEnumCode(Enum value) noexcept {
    return static_cast<std::int32_t>(value);
}

/** Test whether a raw value names a declared enumerator.
 * @tparam Enum Supported market enum.
 * @param value Known or unknown raw enum value.
 * @return True only for explicitly named codes; reserved table entries return false.
 */
template <MarketEnum Enum> [[nodiscard]] constexpr bool isKnownMarketEnum(Enum value) noexcept {
    const auto code = marketEnumCode(value);

    return code >= 0 && code < detail::marketEnumCount<Enum>();
}

/** Apply the Java valueOf bit-pattern table without changing the input raw value.
 * @tparam Enum Supported market enum.
 * @param value Raw enum value; retain it separately when round-trip preservation is required.
 * @return Known code, code-zero sentinel for reserved table entries, or INVALID_ENUM_CODE outside the table.
 */
template <MarketEnum Enum>
[[nodiscard]] constexpr std::expected<Enum, EventValueError> marketEnumValue(Enum value) noexcept {
    const auto code = marketEnumCode(value);

    if (code < 0 || code >= detail::marketEnumTableSize<Enum>()) {
        return std::unexpected(EventValueError::INVALID_ENUM_CODE);
    }

    return isKnownMarketEnum(value) ? value : static_cast<Enum>(0);
}
} // namespace org::ttldtor::qdcxx::events
