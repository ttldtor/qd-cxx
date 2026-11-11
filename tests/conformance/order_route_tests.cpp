// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "event_registry_defaults.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/events/order.hpp"
#include "receiving_profile.hpp"
#include <array>
#include <doctest/doctest.h>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace d = q::api::detail;
constexpr q::core::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};

/** @return Complete explicit default and source-specific Order registry, without runtime work. */
q::api::EventRegistry registry() {
    auto defaults = q::events::defaultOrderSchema(SCHEMA);
    REQUIRE(defaults);
    std::array records{defaults->records()[0], defaults->records()[0], defaults->records()[0]};
    records[1].id = 1;
    records[1].name = u"Order#NTV";
    records[2].id = 2;
    records[2].name = u"Order#BZX";
    auto schema = q::core::LocalSchema::create(records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    const std::array routes{q::api::EventRegistration{q::api::EventType::ORDER, 0},
                            q::api::EventRegistration{q::api::EventType::ORDER, 1},
                            q::api::EventRegistration{q::api::EventType::ORDER, 2}};
    q::api::EventRegistryWork work;
    auto result = q::api::EventRegistry::create(*schema, routes, {}, work);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

TEST_CASE("Order registry rejects a collector contributor that differs from the mapped Index") {
    auto defaults = q::events::defaultOrderSchema(SCHEMA);
    REQUIRE(defaults);
    auto record = defaults->records()[0];
    record.indexedTimeLayout = q::core::IndexedTimeLowInt32{6};
    auto schema = q::core::LocalSchema::create(std::array{record}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    q::api::EventRegistryWork work;
    const std::array routes{q::api::EventRegistration{q::api::EventType::ORDER, 0}};
    auto result = q::api::EventRegistry::create(*schema, routes, {}, work);
    REQUIRE_FALSE(result);
    CHECK(std::get<q::api::EventRegistryError>(result.error().cause) == q::api::EventRegistryError::INVALID_INPUT);
    CHECK(result.error().registrationIndex == 0);
}

TEST_CASE("Order HISTORY defaults preserve the existing ordinary built-in catalog") {
    q::api::EventRegistryWork ordinaryWork;
    auto ordinary = q::api::EventRegistry::builtIn({}, ordinaryWork);
    REQUIRE(ordinary);
    CHECK(ordinary->registrations().size() == 9);
    q::api::EventRegistryWork historyWork;
    auto history = d::defaultHistoryEventRegistry({}, historyWork);
    REQUIRE(history);
    REQUIRE(history->registrations().size() == 3);
    CHECK(history->registrations()[1].type == q::api::EventType::MARKET_MAKER);
    CHECK(history->registrations()[2].projection == q::events::EventProjection::MARKET_MAKER_ORDERS);
    CHECK(history->registrations()[0].type == q::api::EventType::ORDER);
    CHECK(history->schema().records()[0].hasTime);
    q::api::EventRegistryWork emptyWork{0, 0};
    CHECK_FALSE(d::defaultHistoryEventRegistry({}, emptyWork));
}

TEST_CASE("Order plain indexed and negative source selectors route independently of business time") {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::EndpointOptions options;
    options.eventRegistry = registry();
    options.contract = q::api::DeliveryContract::HISTORY;
    auto profile = d::prepareReceivingProfile(limits, options, "127.0.0.1", 1);
    CHECK(profile.family == q::core::SubscriptionFamily::HISTORY);
    const auto &plan = d::receivingEventPlan(profile, d::EventSelection::ORDER);
    REQUIRE(plan.bindingCount == 3);
    const std::array ids{0, q::events::OrderSource::NTV.id(), q::events::OrderSource::BZX.id()};

    for (auto selected : std::array<std::optional<std::int32_t>, 6>{
             {{}, 0, q::events::OrderSource::NTV.id(), q::events::OrderSource::BZX.id(), -1, 123456789}}) {
        d::ReceivingSymbol symbol;
        symbol.logical = u"AAPL&Q{x=y}";
        symbol.sourceId = selected;
        std::array symbols{symbol};
        d::prepareReceivingSymbols(profile, plan, symbols);

        for (std::size_t i = 0; i < plan.bindingCount; ++i) {
            const auto index = plan.bindingIndices[i];
            CHECK(profile.bindingSources[index] == ids[i]);
            CHECK(profile.bindingExchanges[index] == 0);
            CHECK(d::receivingSymbolMatches(profile, index, symbols[0]) ==
                  (!selected || *selected < 0 || *selected == ids[i]));
            CHECK(symbols[0].logical == u"AAPL&Q{x=y}");
        }
    }
}

TEST_CASE("Order filter coordinates select the actual source for indexed demand and owning values") {
    auto catalog = registry();
    auto filter = q::api::FeedFilter::Builder(catalog, q::api::DeliveryContract::HISTORY)
                      .withCategory(u"ntv", u":Order#NTV")
                      .build();
    const auto plain = q::api::SubscriptionSelector::plain(u"AAPL");
    const auto ntv = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::NTV.indexedSource());
    const auto bzx = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::BZX.indexedSource());
    const auto other =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::IndexedEventSource(123456789, std::nullopt));
    CHECK(filter.accept(q::api::EventType::ORDER, plain));
    CHECK(filter.accept(q::api::EventType::ORDER, ntv));
    CHECK_FALSE(filter.accept(q::api::EventType::ORDER, bzx));
    CHECK_FALSE(filter.accept(q::api::EventType::ORDER, other));
    q::events::Order event;
    event.symbol = u"AAPL";
    event.index = static_cast<std::int64_t>(q::events::OrderSource::NTV.id()) << 32;
    CHECK(filter.acceptEvent(q::api::RegisteredEvent{event}));
    event.index = static_cast<std::int64_t>(q::events::OrderSource::BZX.id()) << 32;
    CHECK_FALSE(filter.acceptEvent(q::api::RegisteredEvent{event}));
    event.index = -1;
    CHECK_FALSE(filter.acceptEvent(q::api::RegisteredEvent{event}));
}
