// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "callback_execution.hpp"
#include "connect_peer.hpp"
#include "receiving_profile.hpp"
#include <doctest/doctest.h>
#include <iostream>
#include <limits>
#include <tuple>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace p = q::protocol;

/** One actual immutable prefix admission, not a reimplementation of snapshot preparation. */
struct Result {
    bool complete = false;
    p::LiveError error = p::LiveError::NONE;
    std::size_t text = 0;
    std::size_t descriptions = 0;
    std::size_t publications = 0;
};

/** Feed the real complete default schema descriptors through the exact SDK-selected session profile.
 * @param profile Actual immutable receiving profile. @param prefix Complete owned source bytes.
 * @param text Remaining whole-prefix text seed. @return Actual publications/error/consumed work.
 */
Result run(const q::api::detail::ReceivingProfile &profile, std::span<const std::byte> prefix, std::size_t text) {
    const auto &budgets = profile.endpoint.settings().budgets;
    auto writerWork = budgets.writerPreparation;
    auto writer =
        p::SubscriptionWriter::create(profile.schema, profile.blueprint, budgets.layout, budgets.writer, writerWork);
    REQUIRE(writer);
    auto copy = profile.schema;
    auto initialization = budgets.protocolTurn;
    auto session = p::LiveSession::create(std::move(copy), std::move(*writer), budgets.session, initialization);
    REQUIRE(session);
    auto work = budgets.protocolTurn;
    work.textUnits = text;
    std::size_t offset = 0;
    Result observed;

    while (offset < prefix.size()) {
        auto admitted = session->feed(prefix.subspan(offset), work);
        offset += admitted.accepted;

        if (admitted.event) {
            ++observed.publications;
        }

        if (admitted.status == p::LiveStatus::FAILED || admitted.accepted == 0) {
            break;
        }
    }

    observed.error = session->diagnostic().error;
    observed.text = text - work.textUnits;
    observed.descriptions = session->peer().descriptions().size();
    observed.complete = offset == prefix.size() && observed.error == p::LiveError::NONE;

    return observed;
}
} // namespace

TEST_CASE("full TICKER description generation has complete finite text funding and exact one-under refusal") {
    q::api::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    const auto profile = q::api::detail::prepareReceivingProfile(runtime, {}, "127.0.0.1", 12345);
    const auto prefix =
        q::connect_tests::fixturePeerPrefix(profile.schema, q::core::SubscriptionFamily::TICKER, true, false);
    const auto &budgets = profile.endpoint.settings().budgets;
    const auto funded = run(profile, prefix, budgets.protocolTurn.textUnits);
    REQUIRE(funded.complete);
    CHECK(funded.descriptions == profile.schema.records().size());
    CHECK(funded.publications == 2);
    CHECK(funded.text > 10000000);
    const auto exact = run(profile, prefix, funded.text);
    CHECK(exact.complete);
    CHECK(exact.text == funded.text);
    const auto below = run(profile, prefix, funded.text - 1);
    CHECK_FALSE(below.complete);
    CHECK(below.error == p::LiveError::WORK_LIMIT);
    CHECK(below.descriptions == 0);
    CHECK(below.publications == 1);
    std::cout << "DERIVED_CONTEXT_TEXT consumed=" << funded.text << " seed=" << budgets.protocolTurn.textUnits
              << " snapshot=" << budgets.session.snapshotBytes << '\n';
}

/** Prepare explicit compatible regional pairs without relying on business-type inference from names.
 * @param count Finite schema context count. @param split Split contexts across Q/R exchange buckets.
 * @return Immutable owning custom registry.
 */
q::api::EventRegistry regionalPairs(std::size_t count, bool split) {
    constexpr q::core::SchemaLimits schemaLimits{42, 284, 724, 128, 16384};
    auto original = q::events::defaultQuoteSchema(schemaLimits);
    REQUIRE(original);
    std::vector<q::core::LocalRecord> records;
    std::vector<q::api::EventRegistration> routes;

    for (std::size_t id = 0; id < count; ++id) {
        auto name = u"Regional" + std::u16string(1, static_cast<char16_t>(u'A' + id / 26)) +
                    std::u16string(1, static_cast<char16_t>(u'A' + id % 26)) + u"&" +
                    std::u16string(1, split && id % 2 ? u'R' : u'Q');
        q::core::LocalRecord record{static_cast<std::int32_t>(id), std::move(name), false, {}, {}};

        for (std::size_t field = 0; field < original->records()[0].fields.size(); ++field) {
            if (field == 1 || field == 5) {
                continue;
            }

            const auto target = record.fields.size();
            record.fields.push_back(original->records()[0].fields[field]);

            for (const auto &lookup : original->records()[0].lookups) {
                if (lookup.fieldId == field && lookup.name.find(u'.') == std::u16string::npos) {
                    record.lookups.push_back({lookup.name, target});
                }
            }
        }

        records.push_back(std::move(record));
        routes.push_back({q::api::EventType::QUOTE, static_cast<std::int32_t>(id)});
        q::api::EventRegistration derived{q::api::EventType::ORDER, static_cast<std::int32_t>(id)};
        derived.projection = q::events::EventProjection::QUOTE_ORDERS;
        routes.push_back(derived);
    }

    auto schema = q::core::LocalSchema::create(records, schemaLimits, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    q::api::EventRegistryLimits limits;
    limits.schema = schemaLimits;
    limits.registrations = 84;
    q::api::EventRegistryWork work;
    work.preparation = 1000000000;
    auto registry = q::api::EventRegistry::create(*schema, routes, limits, work);
    REQUIRE(registry);

    return std::move(*registry);
}

TEST_CASE("regional compatible pairs fund physical coordinate domains rather than only schema contexts") {
    q::api::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    const auto defaults = q::api::detail::prepareReceivingProfile(runtime, {}, "127.0.0.1", 12345);
    CHECK(defaults.physicalRouteCount == 42);
    CHECK(defaults.endpoint.settings().budgets.desired.entries == runtime.maximumSymbolsPerSubscription *
                                                                      runtime.maximumSubscriptionsPerEndpoint *
                                                                      defaults.physicalRouteCount);

    for (const auto [contexts, split, expected] :
         {std::tuple<std::size_t, bool, std::size_t>{1, false, 2}, {42, false, 84}, {42, true, 63}}) {
        q::api::EndpointOptions options;
        options.eventRegistry = regionalPairs(contexts, split);
        const auto profile = q::api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
        CHECK(profile.physicalRouteCount == expected);
        CHECK(profile.endpoint.settings().budgets.desired.entries ==
              runtime.maximumSymbolsPerSubscription * runtime.maximumSubscriptionsPerEndpoint * expected);
        CHECK(profile.schema.records().size() == contexts);
        CHECK(profile.bindings.size() == 2 * contexts);
        auto exact = profile.routing.work;
        CHECK(q::api::detail::prepayReceivingRouting(profile, exact));
        CHECK(exact.visits == 0);
        CHECK(exact.textUnits == 0);
        CHECK(exact.bytes == 0);

        for (const auto coordinate : {0, 1, 2}) {
            auto below = profile.routing.work;

            if (coordinate == 0)
                --below.visits;

            if (coordinate == 1)
                --below.textUnits;

            if (coordinate == 2)
                --below.bytes;

            const auto before = below;
            CHECK_FALSE(q::api::detail::prepayReceivingRouting(profile, below));
            CHECK(below.visits == before.visits);
            CHECK(below.textUnits == before.textUnits);
            CHECK(below.bytes == before.bytes);
        }
    }

    q::api::EndpointOptions options;
    options.eventRegistry = regionalPairs(42, false);
    runtime.maximumSymbolsPerSubscription = std::numeric_limits<std::size_t>::max();
    CHECK_THROWS(static_cast<void>(q::api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345)));
}

TEST_CASE("full twenty-output callback packet has exact pre-admission byte credit and one-under refusal") {
    using Pool = q::api::detail::CallbackSlotPool;

    struct Executor final : q::api::CallbackExecutor {
        q::api::CallbackPostResult post(q::api::CallbackTask) override {
            return q::api::CallbackPostResult::REJECTED;
        }
    };

    CHECK(Pool::MAXIMUM_EVENTS == 20);
    const auto storage = Pool::storageBytes(2);
    const auto exact = storage + Pool::PACKET_BYTES;

    for (const bool below : {false, true}) {
        q::api::RuntimeOptions options;
        options.maximumEndpoints = 1;
        options.maximumSubscriptionsPerEndpoint = 2;
        options.maximumCallbackBytes = exact - (below ? 1 : 0);
        options.dnsServers.push_back({{127, 0, 0, 1}});
        q::api::ApiRuntime runtime(options);
        auto endpoint = runtime.createEndpoint();
        auto first = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::ORDER});
        auto second = endpoint->getFeed()->createSubscription<q::events::Order>();
        auto executor = std::make_shared<Executor>();

        if (below) {
            CHECK_THROWS_AS(first->setEventExecutor(executor), q::api::Error);
            CHECK_FALSE(first->getEventExecutor());
        } else {
            CHECK_NOTHROW(first->setEventExecutor(executor));
            CHECK_THROWS_AS(second->setEventExecutor(executor), q::api::Error);
            CHECK_FALSE(second->getEventExecutor());
            CHECK(first->getEventExecutor() == executor);
            first->setEventExecutor(nullptr);
            CHECK_NOTHROW(second->setEventExecutor(executor));
        }

        CHECK(endpoint->closeAndAwaitTermination().drained);
    }

    std::cout << "DERIVED_PACKET RegisteredEvent=" << sizeof(q::api::RegisteredEvent)
              << " MarketMaker=" << sizeof(q::events::MarketMaker)
              << " PreparedMarketMaker=" << sizeof(q::events::PreparedMarketMaker)
              << " PreparedDerivedOrder=" << sizeof(q::events::PreparedDerivedOrder) << " packet=" << Pool::PACKET_BYTES
              << " slots2=" << storage << " exact=" << exact << '\n';
}
