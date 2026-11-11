// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#pragma once

namespace org::ttldtor::qdcxx::events {
/** Explicit verified business projection family; record names alone never grant a capability. */
enum class EventProjection {
    NATIVE,             ///< One existing native DTO mapper.
    QUOTE_ORDERS,       ///< Four independently selected composite/regional Order delegates from Quote fields.
    MARKET_MAKER_ORDERS ///< Four independently selected aggregate Order delegates from MarketMaker fields.
};
} // namespace org::ttldtor::qdcxx::events
