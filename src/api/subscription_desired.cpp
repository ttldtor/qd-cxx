// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "subscription_desired.hpp"

#include "../core/delivery_statistics.hpp"
#include "subscription_observer.hpp"

#include <algorithm>
#include <limits>

namespace org::ttldtor::qdcxx::api::detail {
bool commitSubscriptionDesiredLocked(SubscriptionPort &port, std::vector<SubscriptionSelector> symbols,
                                     std::optional<std::int64_t> fromTime) {
    const bool unchanged = fromTime == port.fromTime && symbols.size() == port.symbols.size() &&
                           std::ranges::all_of(symbols, [&](const auto &symbol) {
                               const auto previous = std::ranges::find(port.symbols, symbol);

                               return previous != port.symbols.end() && symbol.sameDemand(*previous);
                           });

    if (unchanged)
        return false;

    if (port.revision == std::numeric_limits<std::uint64_t>::max())
        throw Error({ErrorCode::RESOURCE_LIMIT, "subscription-revision"});

    prepareSubscriptionRangeChangeLocked(port, symbols, port.revision + 1, fromTime);
    port.symbols.swap(symbols);
    port.fromTime = fromTime;
    ++port.revision;
    port.statistics->update([&](auto &s) {
        s.desiredSelectors = port.symbols.size();
        s.desiredRevision = port.revision;
    });

    return true;
}

bool replaceSubscriptionFromTimeLocked(SubscriptionPort &port, std::int64_t fromTime) {
    if (!port.timeSeries)
        throw Error({ErrorCode::UNSUPPORTED_PROFILE, "subscription-time-series-mode"});

    if (port.fromTime == fromTime)
        return false;

    std::vector<SubscriptionSelector> symbols;
    symbols.reserve(port.maximumSymbols);

    for (const auto &symbol : port.symbols)
        symbols.push_back(SubscriptionSelector::timeSeries(std::u16string(symbol.eventSymbol()), fromTime));

    return commitSubscriptionDesiredLocked(port, std::move(symbols), fromTime);
}
} // namespace org::ttldtor::qdcxx::api::detail
