// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/core/subscription.hpp"
#include "qd/events/order.hpp"
#include "qd/events/quote.hpp"
#include "qd/mux/interest.hpp"
#include <array>
#include <doctest/doctest.h>

namespace {
namespace q = org::ttldtor::qdcxx;
constexpr q::core::SchemaLimits SCHEMA{4, 128, 256, 128, 16384};
constexpr q::core::SubscriptionLimits LIMITS{4, 4, 8, 32, 8, 64, 8};

/** @return Independent finite command credits. */
q::core::SubscriptionWork coreWork() {
    return {100, 1000000, 1000000};
}

/** @return Independent finite authoritative command/query credits. */
q::mux::MuxWork muxWork() {
    return {1000, 1000000, 1000000, 16 * 1024 * 1024};
}
} // namespace

TEST_CASE("captured core bits retain only their original Exact All contributions") {
    auto schema = q::events::defaultQuoteSchema(SCHEMA);
    REQUIRE(schema);
    auto owner = q::core::SubscriptionState::create(*schema, q::core::SubscriptionFamily::STREAM, LIMITS,
                                                    q::core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(owner);
    const auto all = q::core::allSymbols(schema->recordContext(0));
    const q::core::SubscriptionKey exact{schema->recordContext(0), u"AAPL"};
    auto work = coreWork();
    REQUIRE(owner->replace(
        std::array{q::core::SubscriptionRequest{all, {}, 2}, q::core::SubscriptionRequest{exact, {}, 4}}, work));
    auto captured = q::core::captureDeliveryProjections(owner->entries()[0], &owner->entries()[1]);
    CHECK(captured.count() == 2);
    CHECK(owner->currentProjectionMask(captured) == 6);
    REQUIRE(owner->remove(std::array{all}, work));
    CHECK(owner->currentProjectionMask(captured) == 4);
    REQUIRE(owner->add(std::array{q::core::SubscriptionRequest{all, {}, 2}}, work));
    CHECK(owner->currentProjectionMask(captured) == 4);
    REQUIRE(owner->replace(
        std::array{q::core::SubscriptionRequest{all, {}, 2}, q::core::SubscriptionRequest{exact, {}, 2}}, work));
    auto shared = q::core::captureDeliveryProjections(owner->entries()[0], &owner->entries()[1]);
    CHECK(shared.count() == 1);
    REQUIRE(owner->remove(std::array{all}, work));
    CHECK(owner->currentProjectionMask(shared) == 2);
    REQUIRE(owner->add(std::array{q::core::SubscriptionRequest{exact, {}, 6}}, work));
    CHECK(owner->currentProjectionMask(shared) == 2);
    auto both = q::core::captureDeliveryProjections(owner->entries()[0]);
    REQUIRE(owner->add(std::array{q::core::SubscriptionRequest{exact, {}, 4}}, work));
    CHECK(owner->currentProjectionMask(shared) == 0);
    CHECK(owner->currentProjectionMask(both) == 4);
    REQUIRE(owner->add(std::array{q::core::SubscriptionRequest{exact, {}, 6}}, work));
    CHECK(owner->currentProjectionMask(both) == 4);
    const auto revision = owner->revision();
    CHECK_FALSE(owner->replace(std::array{q::core::SubscriptionRequest{exact, {}, 32}}, work));
    CHECK(owner->revision() == revision);
    CHECK(owner->currentProjectionMask(both) == 4);
    CHECK_FALSE(owner->replace(
        std::array{q::core::SubscriptionRequest{exact, {}, 2}, q::core::SubscriptionRequest{exact, {}, 4}}, work));
    CHECK(owner->revision() == revision);
}

TEST_CASE("raw HISTORY bound renewal retires all captured bits without changing selector identity") {
    auto schema = q::events::defaultOrderSchema(SCHEMA);
    REQUIRE(schema);
    auto owner = q::core::SubscriptionState::create(*schema, q::core::SubscriptionFamily::HISTORY, LIMITS);
    REQUIRE(owner);
    const q::core::SubscriptionKey exact{schema->recordContext(0), u"AAPL"};
    auto work = coreWork();
    REQUIRE(owner->add(std::array{q::core::SubscriptionRequest{exact, 0, 6}}, work));
    auto captured = q::core::captureDeliveryProjections(owner->entries()[0]);
    REQUIRE(owner->replace(std::array{q::core::SubscriptionRequest{exact, 1, 6}}, work));
    CHECK(owner->entries().size() == 1);
    CHECK(owner->currentProjectionMask(captured) == 0);
}

TEST_CASE("authoritative per-bit changes preserve physical union and never resurrect old bits") {
    auto schema = q::events::defaultQuoteSchema(SCHEMA);
    REQUIRE(schema);
    auto work = muxWork();
    auto owner =
        q::mux::MuxState::create(*schema, q::core::SubscriptionFamily::STREAM, {4, 2, 8, 8, 8, 64, 8, 1024 * 1024},
                                 work, q::core::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(owner);
    auto client = owner->createClient(work);
    REQUIRE(client);
    const auto all = q::core::allSymbols(schema->recordContext(0));
    const q::core::SubscriptionKey exact{schema->recordContext(0), u"AAPL"};
    REQUIRE(owner->replace(
        *client, std::array{q::core::SubscriptionRequest{all, {}, 2}, q::core::SubscriptionRequest{exact, {}, 4}},
        work));
    auto captured = owner->selectProjections(*client, exact, {}, work);
    REQUIRE(captured);
    CHECK(captured->count() == 2);
    REQUIRE(owner->remove(*client, std::array{all}, work));
    auto current = owner->currentProjectionMask(*client, *captured, work);
    REQUIRE(current);
    CHECK(*current == 4);
    REQUIRE(owner->add(*client, std::array{q::core::SubscriptionRequest{all, {}, 2}}, work));
    current = owner->currentProjectionMask(*client, *captured, work);
    REQUIRE(current);
    CHECK(*current == 4);
    REQUIRE(owner->replace(*client, std::array{q::core::SubscriptionRequest{exact, {}, 6}}, work));
    CHECK(owner->aggregateEntries().size() == 1);
    CHECK(owner->aggregateEntries()[0].projectionMask == q::core::NATIVE_DELIVERY_PROJECTION);
    auto stable = owner->selectProjections(*client, exact, {}, work);
    REQUIRE(stable);
    REQUIRE(owner->replace(*client, std::array{q::core::SubscriptionRequest{exact, {}, 4}}, work));
    current = owner->currentProjectionMask(*client, *stable, work);
    REQUIRE(current);
    CHECK(*current == 4);
    REQUIRE(owner->replace(*client, std::array{q::core::SubscriptionRequest{exact, {}, 6}}, work));
    current = owner->currentProjectionMask(*client, *stable, work);
    REQUIRE(current);
    CHECK(*current == 4);
    const auto revision = owner->revision();
    CHECK_FALSE(owner->replace(*client, std::array{q::core::SubscriptionRequest{exact, {}, 0}}, work));
    CHECK(owner->revision() == revision);
    CHECK(owner->aggregateEntries().size() == 1);
}
