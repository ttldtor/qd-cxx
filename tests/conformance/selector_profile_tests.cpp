// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "receiving_profile.hpp"
#include <array>
#include <bit>
#include <doctest/doctest.h>
#include <iostream>
#include <limits>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace d = q::api::detail;
} // namespace

TEST_CASE("typed selector routing has a finite measured workspace and exact one-below atomic prepay") {
    a::RuntimeOptions runtime;
    runtime.maximumSubscriptionsPerEndpoint = 3;
    runtime.maximumSymbolsPerSubscription = 4;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::STREAM;
    options.enableWildcard = true;
    auto profile = d::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    CHECK(profile.recoveryOptions.selectors == q::core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(d::RECEIVING_EVENT_KINDS < std::numeric_limits<std::size_t>::digits);
    const auto nonemptyMasks = (std::size_t{1} << d::RECEIVING_EVENT_KINDS) - 1;
    CHECK(static_cast<std::size_t>(d::EventSelection::ALL) == nonemptyMasks);
    CHECK(profile.eventPlans.size() == 512);
    CHECK(profile.routing.symbols == 4);
    CHECK(profile.routing.temporaryTextUnits == 2 * 4 * 128);
    CHECK(profile.routing.frontendSelectorBytes == 4 * 4 * (sizeof(a::SubscriptionSelector) + 129 * sizeof(char16_t)));
    std::cout << "SELECTOR_RESOURCE value=" << sizeof(a::SubscriptionSelector)
              << " worker=" << sizeof(d::ReceivingSymbol) << " frontendBytes=" << profile.routing.frontendSelectorBytes
              << " temporaryBytes=" << profile.routing.temporaryBytes << " visits=" << profile.routing.work.visits
              << " units=" << profile.routing.work.textUnits << " bytes=" << profile.routing.work.bytes << '\n';
    const auto grant = profile.routing.work;

    for (unsigned coordinate = 0; coordinate < 3; ++coordinate) {
        auto below = grant;

        if (coordinate == 0) {
            REQUIRE(below.visits > 0);
            --below.visits;
        } else if (coordinate == 1) {
            REQUIRE(below.textUnits > 0);
            --below.textUnits;
        } else {
            REQUIRE(below.bytes > 0);
            --below.bytes;
        }

        const auto before = below;
        CHECK_FALSE(d::prepayReceivingRouting(profile, below));
        CHECK(below.visits == before.visits);
        CHECK(below.textUnits == before.textUnits);
        CHECK(below.bytes == before.bytes);
    }

    auto exact = grant;
    REQUIRE(d::prepayReceivingRouting(profile, exact));
    CHECK(exact.visits == 0);
    CHECK(exact.textUnits == 0);
    CHECK(exact.bytes == 0);
    auto fresh = profile.submitWork;

    for (std::size_t client = 0; client < runtime.maximumSubscriptionsPerEndpoint; ++client) {
        REQUIRE(d::prepayReceivingRouting(profile, fresh));
    }

    CHECK(fresh.operations > 0);
    CHECK(fresh.visits > 0);
    CHECK(fresh.textUnits > 0);
    CHECK(fresh.bytes > 0);
    std::array<d::ReceivingSymbol, 2> symbols{{{u"AAPL"}, {}}};
    symbols[1].selector = q::core::SubscriptionSelectorKind::ALL;
    symbols[1].physical = u"STALE";
    symbols[1].exchange = u'Q';
    const auto &plan = d::receivingEventPlan(profile, d::EventSelection::ALL);
    CHECK(plan.bindingCount == profile.bindings.size());
    std::array<bool, d::MAXIMUM_RECEIVING_BINDINGS> selected{};
    d::prepareReceivingSymbols(profile, plan, symbols);
    CHECK(symbols[1].physical.empty());
    CHECK(symbols[1].exchange == 0);

    for (std::size_t index = 0; index < plan.bindingCount; ++index) {
        REQUIRE(plan.bindingIndices[index] < profile.bindings.size());
        CHECK_FALSE(selected[plan.bindingIndices[index]]);
        selected[plan.bindingIndices[index]] = true;
        CHECK(d::receivingSymbolMatches(profile, plan.bindingIndices[index], symbols[1]));
    }
}

TEST_CASE("unrepresentable frontend selector banks refuse before allocation or native bootstrap") {
    a::RuntimeOptions runtime;
    runtime.maximumSymbolsPerSubscription = std::numeric_limits<std::size_t>::max();
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    try {
        a::ApiRuntime owner(runtime);
        FAIL("Overflowing selector bank configuration accepted");
    } catch (const a::Error &error) {
        CHECK(error.diagnostic().code == a::ErrorCode::RESOURCE_LIMIT);
        CHECK(error.diagnostic().stage == "runtime-selector-storage");
    }
}

TEST_CASE("derived routes preserve literal symbols and choose only actual source delegates") {
    a::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});
    auto profile = d::prepareReceivingProfile(runtime, {}, "127.0.0.1", 12345);
    const auto &plan = d::receivingEventPlan(profile, d::EventSelection::ORDER);
    REQUIRE(plan.bindingCount == 27);

    for (const auto source : std::array<std::optional<std::int32_t>, 7>{{{}, -1, 0, 1, 3, 7, 8}}) {
        std::array<d::ReceivingSymbol, 1> symbols{{{u"AAPL&Q"}}};
        symbols[0].sourceId = source;
        d::prepareReceivingSymbols(profile, plan, symbols);
        std::size_t physical = 0, events = 0;

        for (std::size_t i = 0; i < plan.bindingCount; ++i) {
            const auto binding = plan.bindingIndices[i];
            const auto mask = d::receivingProjectionMask(profile, binding, symbols[0]);

            if (!mask) {
                continue;
            }

            ++physical;
            events += std::popcount(mask);
            CHECK(d::receivingPhysicalSymbol(profile, binding, symbols[0]) == u"AAPL&Q");
        }

        const bool defaults = !source || *source < 0;
        CHECK(physical == (defaults ? 27 : (*source == 0 ? 0 : (*source == 3 || *source == 8 ? 26 : 1))));
        CHECK(
            events ==
            (defaults ? 54 : (*source == 0 ? 0 : (*source == 8 ? 52 : (*source == 3 ? 26 : (*source == 7 ? 2 : 1))))));
    }

    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    auto history = d::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    const auto &all = d::receivingEventPlan(history, static_cast<d::EventSelection>(512 | 1024));
    CHECK(all.bindingCount == 3);
    std::array<d::ReceivingSymbol, 1> plain{{{u"AAPL"}}};
    d::prepareReceivingSymbols(history, all, plain);
    std::size_t count = 0;

    for (std::size_t i = 0; i < all.bindingCount; ++i) {
        count += std::popcount(d::receivingProjectionMask(history, all.bindingIndices[i], plain[0]));
    }

    CHECK(count == 4); // Native Order, native MarketMaker and the separate aggregate bid/ask.
}

TEST_CASE("immutable unitary default changes plain and negative delegates without overriding explicit sources") {
    a::RuntimeOptions runtime;
    runtime.dnsServers.push_back({{127, 0, 0, 1}});

    for (const bool unitary : {false, true}) {
        a::EndpointOptions options;
        options.unitaryOrderSources = unitary;
        auto profile = d::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
        const auto &plan = d::receivingEventPlan(profile, d::EventSelection::ORDER);

        for (const auto source : std::array<std::optional<std::int32_t>, 4>{{{}, -1, 1, 7}}) {
            d::ReceivingSymbol symbol{u"AAPL"};
            symbol.sourceId = source;
            const auto composite = plan.bindingIndices[0];
            const auto mask = d::receivingProjectionMask(profile, composite, symbol);
            CHECK(mask == (!source || *source < 0 ? (unitary ? 24 : 6) : (*source == 1 ? 2 : 24)));
        }
    }
}
