// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once
#include "qd/api/event_registry.hpp"

namespace org::ttldtor::qdcxx::api::detail {
/** Immutable supported type set; it selects routes independently of their local record IDs. */
enum class EventSelection {
    QUOTE = 1,                            ///< Selected QUOTE routes.
    PROFILE = 2,                          ///< Selected PROFILE routes.
    BOTH = 3,                             ///< Selected QUOTE, PROFILE routes.
    TRADE = 4,                            ///< Selected TRADE routes.
    QUOTE_TRADE = 5,                      ///< Selected QUOTE, TRADE routes.
    PROFILE_TRADE = 6,                    ///< Selected PROFILE, TRADE routes.
    QUOTE_PROFILE_TRADE = 7,              ///< Selected QUOTE, PROFILE, TRADE routes.
    TRADE_ETH = 8,                        ///< Selected TRADE_ETH routes.
    QUOTE_TRADE_ETH = 9,                  ///< Selected QUOTE, TRADE_ETH routes.
    PROFILE_TRADE_ETH = 10,               ///< Selected PROFILE, TRADE_ETH routes.
    QUOTE_PROFILE_TRADE_ETH = 11,         ///< Selected QUOTE, PROFILE, TRADE_ETH routes.
    TRADE_TRADE_ETH = 12,                 ///< Selected TRADE, TRADE_ETH routes.
    QUOTE_TRADE_TRADE_ETH = 13,           ///< Selected QUOTE, TRADE, TRADE_ETH routes.
    PROFILE_TRADE_TRADE_ETH = 14,         ///< Selected PROFILE, TRADE, TRADE_ETH routes.
    QUOTE_PROFILE_TRADE_TRADE_ETH = 15,   ///< Selected QUOTE, PROFILE, TRADE, TRADE_ETH routes.
    SUMMARY = 16,                         ///< Selected SUMMARY routes.
    QUOTE_SUMMARY = 17,                   ///< Selected QUOTE, SUMMARY routes.
    PROFILE_SUMMARY = 18,                 ///< Selected PROFILE, SUMMARY routes.
    QUOTE_PROFILE_SUMMARY = 19,           ///< Selected QUOTE, PROFILE, SUMMARY routes.
    TRADE_SUMMARY = 20,                   ///< Selected TRADE, SUMMARY routes.
    QUOTE_TRADE_SUMMARY = 21,             ///< Selected QUOTE, TRADE, SUMMARY routes.
    PROFILE_TRADE_SUMMARY = 22,           ///< Selected PROFILE, TRADE, SUMMARY routes.
    QUOTE_PROFILE_TRADE_SUMMARY = 23,     ///< Selected QUOTE, PROFILE, TRADE, SUMMARY routes.
    TRADE_ETH_SUMMARY = 24,               ///< Selected TRADE_ETH, SUMMARY routes.
    QUOTE_TRADE_ETH_SUMMARY = 25,         ///< Selected QUOTE, TRADE_ETH, SUMMARY routes.
    PROFILE_TRADE_ETH_SUMMARY = 26,       ///< Selected PROFILE, TRADE_ETH, SUMMARY routes.
    QUOTE_PROFILE_TRADE_ETH_SUMMARY = 27, ///< Selected QUOTE, PROFILE, TRADE_ETH, SUMMARY routes.
    TRADE_TRADE_ETH_SUMMARY = 28,         ///< Selected TRADE, TRADE_ETH, SUMMARY routes.
    QUOTE_TRADE_TRADE_ETH_SUMMARY = 29,   ///< Selected QUOTE, TRADE, TRADE_ETH, SUMMARY routes.
    PROFILE_TRADE_TRADE_ETH_SUMMARY = 30, ///< Selected PROFILE, TRADE, TRADE_ETH, SUMMARY routes.
    TEXT_MESSAGE = 32,                    ///< Selected TextMessage routes.
    TEXT_CONFIGURATION = 64,              ///< Selected TextConfiguration routes.
    MESSAGE = 128,                        ///< Selected serialized Message routes.
    CONFIGURATION = 256,                  ///< Selected serialized Configuration routes.
    ORDER = 512,                          ///< Selected ordinary indexed Order routes.
    MARKET_MAKER = 1024,                  ///< Selected indexed MarketMaker routes.
    SPREAD_ORDER = 1 << 11,               ///< Selected indexed SpreadOrder routes.
    ANALYTIC_ORDER = 1 << 12,             ///< Selected indexed AnalyticOrder routes.
    ALL = (1 << 13) - 1,                  ///< Every supported event route.
};

/** Test a finite supported-kind selection without depending on local record positions.
 * @param selection Nonempty supported type mask.
 * @param type Valid supported business kind.
 * @return Whether this kind belongs to the selected type set.
 */
constexpr bool selectsEvent(EventSelection selection, EventType type) noexcept {
    if (type != EventType::QUOTE && type != EventType::PROFILE && type != EventType::TRADE &&
        type != EventType::TRADE_ETH && type != EventType::SUMMARY && type != EventType::TEXT_MESSAGE &&
        type != EventType::TEXT_CONFIGURATION && type != EventType::MESSAGE && type != EventType::CONFIGURATION &&
        type != EventType::ORDER && type != EventType::MARKET_MAKER && type != EventType::SPREAD_ORDER &&
        type != EventType::ANALYTIC_ORDER) {
        return false;
    }

    return (static_cast<unsigned>(selection) & (1U << static_cast<unsigned>(type))) != 0;
}

/** Native order record families whose source identity is derived from the exact record suffix.
 * @param type Business type.
 * @return True when this family uses OrderSource suffixes and ordinary indexed coordinates.
 */
constexpr bool isOrderSourceEvent(EventType type) noexcept {
    return type == EventType::ORDER || type == EventType::SPREAD_ORDER || type == EventType::ANALYTIC_ORDER;
}

/** Exact owning value shape expected by the logical subscription's listeners. */
enum class EventDelivery {
    QUOTE,              ///< Existing single-type Quote values.
    PROFILE,            ///< Existing single-type Profile values.
    TRADE,              ///< Complete single-type Trade values.
    TRADE_ETH,          ///< Complete distinct single-type TradeETH values.
    SUMMARY,            ///< Complete owning session-summary values.
    TEXT_MESSAGE,       ///< Owning text messages.
    TEXT_CONFIGURATION, ///< Owning text configurations.
    MESSAGE,            ///< Owning serialized Message values.
    CONFIGURATION,      ///< Owning serialized Configuration values.
    ORDER,              ///< Complete owning ordinary indexed Order values.
    MARKET_MAKER,       ///< Complete owning indexed MarketMaker values.
    SPREAD_ORDER,       ///< Complete owning indexed SpreadOrder values.
    ANALYTIC_ORDER,     ///< Complete owning indexed AnalyticOrder values.
    REGISTERED          ///< Owning public RegisteredEvent variants, restricted by EventSelection.
};
} // namespace org::ttldtor::qdcxx::api::detail
