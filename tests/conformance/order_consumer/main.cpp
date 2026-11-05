// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Installed/relocated consumer: public package only, no helper paths, network or caller progress loop.
#include <qd/events/order.hpp>
#if ORG_TTLDTOR_QDCXX_ORDER_CONSUMER_USE_FEED
#    include <qd/api/feed.hpp>
#endif
#include <array>
#include <cstdio>
#include <stdexcept>

namespace {
namespace q = org::ttldtor::qdcxx;

/** @param condition Required public observable behavior. */
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("Installed Order consumer failed");
    }
}
} // namespace

int main() {
    try {
        auto schema = q::events::defaultOrderSchema({1, 16, 32, 64, 2048});
        require(bool(schema));
        auto mapper = q::events::PreparedOrder::prepare(*schema, 0, {16, 32, 32, 64, 128, 64, 8192, 100000});
        require(bool(mapper));
        std::vector<q::core::FieldValue> fields;

        for (const auto &field : schema->records()[0].fields) {
            fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
        }

        fields[0].raw = -1;
        auto event = mapper->map(u"AAPL", {schema->recordContext(0), fields}, {128, 8, 136}, 0, 3);
        require(bool(event) && event->getIndex() == 4294967295LL && event->getSourceId() == 0 &&
                event->eventFlags == 3);
        require(!mapper->map(u"AAPL", {{}, fields}, {128, 8, 136}));
#if ORG_TTLDTOR_QDCXX_ORDER_CONSUMER_USE_FEED
        q::api::RuntimeOptions limits;
        limits.maximumSubscriptionsPerEndpoint = 1;
        limits.dnsServers.push_back({{127, 0, 0, 1}});
        q::api::ApiRuntime runtime(limits);
        auto ticker = runtime.createEndpoint();
        bool rejected = false;
        try {
            static_cast<void>(ticker->getFeed()->createSubscription<q::events::Order>());
        } catch (const q::api::Error &error) {
            rejected = error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
        }
        require(rejected);
        auto quote = ticker->getFeed()->createSubscription<q::events::Quote>();
        quote->addSymbols("AAPL");
        require(ticker->closeAndAwaitTermination().drained);
        q::api::EndpointOptions options;
        options.contract = q::api::DeliveryContract::HISTORY;
        auto history = runtime.createEndpoint(options);
        rejected = false;
        try {
            static_cast<void>(
                history->getFeed()->createSubscription({q::api::EventType::ORDER, q::api::EventType::QUOTE}));
        } catch (const q::api::Error &error) {
            rejected = error.diagnostic().code == q::api::ErrorCode::UNSUPPORTED_PROFILE;
        }
        require(rejected);
        auto order = history->getFeed()->createSubscription<q::events::Order>();
        require(order->getEventTypes() == std::vector{q::api::EventType::ORDER});
        const auto plain = q::api::SubscriptionSelector::plain(u"AAPL");
        const auto indexed =
            q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
        const std::array selectors{plain, indexed};
        order->addSelectors(selectors);
        const auto retained = order->getSnapshot();
        require(retained.symbols.size() == 2);
        order->removeSelector(plain);
        require(order->getSnapshot().symbols == std::vector{indexed});
        require(history->closeAndAwaitTermination().drained);
        require(retained.symbols.size() == 2);
#endif

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
