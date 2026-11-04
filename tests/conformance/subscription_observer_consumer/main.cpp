// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Installed/relocated CTest consumer: public headers only, no peer, SDK internals or caller progress loop.
#include <qd/api/feed_filter.hpp>

#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
namespace q = org::ttldtor::qdcxx;

/** @param condition Public behavior required by this independent consumer. */
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Public subscription observer consumer failed");
    }
}
} // namespace

int main() {
    try {
        q::api::SubscriptionSnapshot retained;
        unsigned additions = 0;
        unsigned removals = 0;
        unsigned closed = 0;
        q::api::RuntimeOptions limits;
        limits.dnsServers.push_back({{127, 0, 0, 1}});
        q::api::ApiRuntime runtime(limits);
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        auto configuration = endpoint->getFeed()->createSubscription<q::events::TextConfiguration>();
        require(configuration->getEventTypes() == std::vector{q::api::EventType::TEXT_CONFIGURATION});
        configuration->addSymbols("CONFIGURATION");
        subscription->addChangeListener([&](const auto &change) {
            if (change.kind == q::api::SubscriptionChangeKind::ADDED) {
                ++additions;
            } else if (change.kind == q::api::SubscriptionChangeKind::REMOVED) {
                ++removals;
            } else {
                ++closed;
            }
        });
        subscription->addSymbols("AAPL");
        retained = subscription->getSnapshot();
        require(retained.revision == 1 && retained.symbols.size() == 1);
        require(subscription->containsEventType(q::api::EventType::QUOTE));
        require(!subscription->containsEventType(q::api::EventType::PROFILE));
        subscription->setSymbols({"IBM"});
        require(additions == 2 && removals == 1);
        require(subscription->getSnapshot().revision == 2);
        require(retained.symbols[0].eventSymbol() == u"AAPL");
        const std::array categories{q::api::FeedFilterCategory{u"primary", u"A*"},
                                    q::api::FeedFilterCategory{u"remaining", u"*"}};
        auto classifier = endpoint->createFilter(categories);
        const auto symbol = q::api::SubscriptionSelector::plain(u"AAPL");
        require(classifier.accept(q::api::EventType::QUOTE, symbol));
        require(classifier.accept(q::api::EventType::TEXT_CONFIGURATION, symbol));
        require(classifier.getCategory(q::api::EventType::QUOTE, symbol) == u"primary");
        bool rejected = false;

        try {
            static_cast<void>(endpoint->createFilter({}));
        } catch (const q::api::Error &error) {
            rejected = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT;
        }

        require(rejected);
        subscription->close();
        require(closed == 0);
        require(endpoint->closeAndAwaitTermination().drained);
        require(closed == 1);
        require(subscription->getSnapshot().revision == 2);
        require(retained.symbols[0].eventSymbol() == u"AAPL");

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
