// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "feed_filter_coordinates.hpp"

#include "qd/events/market_event_symbols.hpp"

#include <algorithm>
#include <type_traits>

namespace org::ttldtor::qdcxx::api::detail {
namespace {
[[noreturn]] void refuse(ErrorCode code, std::string_view stage) {
    throw Error({code, stage});
}

bool supported(const EventRegistry &registry, EventType type) noexcept {
    if (std::ranges::any_of(registry.registrations(), [type](const auto &route) {
            return route.type == type;
        })) {
        return true;
    }

    switch (type) {
    case EventType::QUOTE:
    case EventType::PROFILE:
    case EventType::TRADE:
    case EventType::TRADE_ETH:
    case EventType::SUMMARY:
    case EventType::TEXT_MESSAGE:
    case EventType::TEXT_CONFIGURATION:
    case EventType::MESSAGE:
    case EventType::CONFIGURATION:
        return true;
    }

    return false;
}

/** Only the existing market projections convert regional record/symbol suffixes. */
bool usesRegionalCoordinates(EventType type) noexcept {
    return type == EventType::QUOTE || type == EventType::TRADE || type == EventType::TRADE_ETH ||
           type == EventType::SUMMARY;
}

/** Non-market projections use the generic source-selection rule, rather than MarketEventDelegateSet. */
bool usesGenericSourceSelection(EventType type) noexcept {
    return type == EventType::TEXT_MESSAGE || type == EventType::TEXT_CONFIGURATION || type == EventType::MESSAGE ||
           type == EventType::CONFIGURATION;
}

std::vector<FilterCoordinate> coordinates(const EventRegistry &registry, DeliveryContract contract, EventType type,
                                          std::u16string_view symbol, bool wildcard, const FeedFilterLimits &limits) {
    if (!registry.valid() || !supported(registry, type) ||
        (contract != DeliveryContract::TICKER && contract != DeliveryContract::STREAM &&
         contract != DeliveryContract::HISTORY)) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-coordinates");
    }

    if (symbol.size() > limits.symbolUnits) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-symbol-units");
    }

    const auto exchange = events::market_symbols::getExchangeCode(symbol);
    bool regional = false;
    std::size_t selected = 0;

    for (const auto &route : registry.registrations()) {
        if (route.type != type) {
            continue;
        }

        const auto &record = registry.schema().records()[static_cast<std::size_t>(route.localRecordId)];
        // Profile and generic routes preserve their complete logical symbols, even for regional-looking names.
        const auto routeExchange =
            usesRegionalCoordinates(type) ? events::market_symbols::getExchangeCode(record.name) : 0;
        regional = regional || (exchange != 0 && routeExchange == exchange);
    }

    for (const auto &route : registry.registrations()) {
        if (route.type != type) {
            continue;
        }

        const auto &record = registry.schema().records()[static_cast<std::size_t>(route.localRecordId)];
        const auto routeExchange =
            usesRegionalCoordinates(type) ? events::market_symbols::getExchangeCode(record.name) : 0;

        if (wildcard || (routeExchange ? routeExchange == exchange : !regional)) {
            if (selected == limits.coordinates) {
                refuse(ErrorCode::RESOURCE_LIMIT, "filter-coordinate-count");
            }

            ++selected;
        }
    }

    std::vector<FilterCoordinate> result;
    result.reserve(selected);

    for (const auto &route : registry.registrations()) {
        if (route.type != type) {
            continue;
        }

        const auto &record = registry.schema().records()[static_cast<std::size_t>(route.localRecordId)];
        const auto routeExchange =
            usesRegionalCoordinates(type) ? events::market_symbols::getExchangeCode(record.name) : 0;

        if (!(wildcard || (routeExchange ? routeExchange == exchange : !regional))) {
            continue;
        }

        std::u16string physical;

        if (!wildcard) {
            physical = routeExchange ? *events::market_symbols::changeExchangeCode(symbol, 0) : std::u16string(symbol);
        }

        const bool cipherWildcard = wildcard || physical == u"*";
        result.push_back({record.name, std::move(physical), cipherWildcard});
    }

    return result;
}
} // namespace

std::vector<FilterCoordinate> filterSubscriptionCoordinates(const EventRegistry &registry, DeliveryContract contract,
                                                            EventType type, const SubscriptionSelector &selector,
                                                            const FeedFilterLimits &limits) {
    if (!registry.valid() || !supported(registry, type) ||
        (contract != DeliveryContract::TICKER && contract != DeliveryContract::STREAM &&
         contract != DeliveryContract::HISTORY)) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-coordinates");
    }

    if (selector.eventSymbol().size() > limits.symbolUnits) {
        refuse(ErrorCode::RESOURCE_LIMIT, "filter-symbol-units");
    }

    // Time-series is checked before indexed: the current receiving projections have no history delegate.
    if (selector.kind() == SubscriptionSymbolKind::TIME_SERIES) {
        return {};
    }

    if (usesGenericSourceSelection(type) && selector.kind() == SubscriptionSymbolKind::INDEXED &&
        selector.sourceId().value_or(0) > 0) {
        return {};
    }

    const bool wildcard = selector.kind() == SubscriptionSymbolKind::ALL;

    if (!wildcard && selector.eventSymbol().starts_with(u'*')) {
        return {};
    }

    return coordinates(registry, contract, type, selector.eventSymbol(), wildcard, limits);
}

std::vector<FilterCoordinate> filterEventCoordinates(const EventRegistry &registry, DeliveryContract contract,
                                                     const RegisteredEvent &event, const FeedFilterLimits &limits) {
    if (event.valueless_by_exception()) {
        refuse(ErrorCode::INVALID_ARGUMENT, "filter-event-value");
    }

    return std::visit(
        [&](const auto &value) {
            using Event = std::remove_cvref_t<decltype(value)>;
            constexpr auto type = [] {
                if constexpr (std::is_same_v<Event, events::Quote>) {
                    return EventType::QUOTE;
                } else if constexpr (std::is_same_v<Event, events::Profile>) {
                    return EventType::PROFILE;
                } else if constexpr (std::is_same_v<Event, events::Trade>) {
                    return EventType::TRADE;
                } else if constexpr (std::is_same_v<Event, events::TradeETH>) {
                    return EventType::TRADE_ETH;
                } else if constexpr (std::is_same_v<Event, events::Summary>) {
                    return EventType::SUMMARY;
                } else if constexpr (std::is_same_v<Event, events::TextMessage>) {
                    return EventType::TEXT_MESSAGE;
                } else if constexpr (std::is_same_v<Event, events::TextConfiguration>) {
                    return EventType::TEXT_CONFIGURATION;
                } else if constexpr (std::is_same_v<Event, events::Message>) {
                    return EventType::MESSAGE;
                } else if constexpr (std::is_same_v<Event, events::Configuration>) {
                    return EventType::CONFIGURATION;
                } else {
                    static_assert(std::is_same_v<Event, void>, "Add explicit filter coordinates for a new event");
                }
            }();

            return coordinates(registry, contract, type, value.symbol, false, limits);
        },
        event);
}
} // namespace org::ttldtor::qdcxx::api::detail
