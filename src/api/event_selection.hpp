// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/event_registry.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Immutable supported type set; it selects routes independently of their local record IDs. */
enum class EventSelection {
    QUOTE = 1,                    ///< Every registered Quote route.
    PROFILE = 2,                  ///< Every registered Profile route.
    BOTH = 3,                     ///< Quote and Profile routes.
    TRADE = 4,                    ///< Every registered Trade route.
    QUOTE_TRADE = 5,              ///< Quote and Trade routes.
    PROFILE_TRADE = 6,            ///< Profile and Trade routes.
    QUOTE_PROFILE_TRADE = 7,      ///< Regular three-kind selection, without extended-hours trades.
    TRADE_ETH = 8,                ///< Every registered TradeETH route.
    QUOTE_TRADE_ETH = 9,          ///< Quote and TradeETH routes.
    PROFILE_TRADE_ETH = 10,       ///< Profile and TradeETH routes.
    QUOTE_PROFILE_TRADE_ETH = 11, ///< Quote, Profile and TradeETH routes.
    TRADE_TRADE_ETH = 12,         ///< Both independent regular and extended-hours trade routes.
    QUOTE_TRADE_TRADE_ETH = 13,   ///< Quote and both trade kinds.
    PROFILE_TRADE_TRADE_ETH = 14, ///< Profile and both trade kinds.
    ALL = 15                      ///< Every registered route of all four supported types.
};

/** Test a finite four-kind selection without depending on local record positions.
 * @param selection Nonempty supported type mask.
 * @param type Valid supported business kind.
 * @return Whether this kind belongs to the selected type set.
 */
constexpr bool selectsEvent(EventSelection selection, EventType type) noexcept {
    return (static_cast<unsigned>(selection) & (1U << static_cast<unsigned>(type))) != 0;
}

/** Exact owning value shape expected by the logical subscription's listeners. */
enum class EventDelivery {
    QUOTE,     ///< Existing single-type Quote values.
    PROFILE,   ///< Existing single-type Profile values.
    TRADE,     ///< Complete single-type Trade values.
    TRADE_ETH, ///< Complete distinct single-type TradeETH values.
    REGISTERED ///< Owning public RegisteredEvent variants, restricted by EventSelection.
};
} // namespace org::ttldtor::qdcxx::api::detail
