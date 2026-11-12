// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
using namespace std::chrono_literals;

const std::filesystem::path FIXTURES{ORG_TTLDTOR_QDCXX_TAPE_FEED_FIXTURES};
const auto RAW_QUOTE = FIXTURES / "protocol/raw-tape/expected/raw-quote.qtp";
const auto HISTORY_SALE = FIXTURES / "time-and-sale/wide/history.qtp";

api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.maximumSubscriptionsPerEndpoint = 2;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

void expectCleanEof(const std::shared_ptr<api::Endpoint> &endpoint) {
    const auto state = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 8s);
    const auto closed = endpoint->closeAndAwaitTermination();
    INFO("state=", static_cast<int>(state.state), ", diagnostic=", closed.diagnostic.stage,
         ", detail=", closed.diagnostic.detailCode);
    REQUIRE(state.status == api::WaitStatus::REACHED);
    CHECK(closed.drained);
    CHECK_FALSE(closed.hadGap);
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    CHECK(endpoint->getStatistics().final);
}

TEST_CASE("real RAW tape routes IBM Quote through ordinary STREAM and TICKER subscriptions") {
    for (const auto contract : {api::DeliveryContract::STREAM, api::DeliveryContract::TICKER}) {
        CAPTURE(contract);
        std::vector<q::events::Quote> received;
        std::optional<q::events::Quote> latest;
        bool observedLatestWhileClosing = false;
        std::optional<std::uint64_t> observedSourceEpoch;
        api::ApiRuntime runtime(runtimeOptions());
        api::EndpointOptions options;
        options.contract = contract;
        const auto endpoint = runtime.createEndpoint(options);
        const auto feed = endpoint->getFeed();
        const auto subscription = feed->createSubscription<q::events::Quote>();
        subscription->addSymbols("IBM");
        subscription->addEventListener([&received, &latest, &observedLatestWhileClosing, &observedSourceEpoch,
                                        weak = std::weak_ptr(subscription), weakFeed = std::weak_ptr(feed),
                                        weakEndpoint = std::weak_ptr(endpoint)](auto batch) {
            received.insert(received.end(), batch.begin(), batch.end());

            const auto active = weakEndpoint.lock();
            const auto attachedFeed = weakFeed.lock();

            if (active && attachedFeed) {
                auto value = attachedFeed->getLastEventIfSubscribed<q::events::Quote>("IBM");

                if (active->getState() == api::EndpointState::CONNECTED) {
                    latest = std::move(value);
                } else {
                    observedLatestWhileClosing = observedLatestWhileClosing || value.has_value();
                }
            }

            if (const auto owner = weak.lock()) {
                observedSourceEpoch = owner->getStatistics().sourceEpoch;
            }
        });
        endpoint->connectTape(RAW_QUOTE);
        expectCleanEof(endpoint);

        REQUIRE_FALSE(received.empty());
        CHECK(std::ranges::all_of(received, [](const auto &value) {
            return value.symbol == u"IBM";
        }));
        CHECK(received.back().bidPrice.raw == 2617730);

        if (contract == api::DeliveryContract::STREAM) {
            REQUIRE(received.size() == 2);
            CHECK(received.front().bidPrice.raw == 2566530);
            CHECK_FALSE(latest);
        } else {
            REQUIRE(latest);
            CHECK(latest->symbol == u"IBM");
            CHECK(latest->bidPrice.raw == received.front().bidPrice.raw);
        }

        CHECK_FALSE(observedLatestWhileClosing);
        CHECK_FALSE(feed->getLastEventIfSubscribed<q::events::Quote>("IBM"));
        CHECK(subscription->getStatistics().final);
        CHECK(observedSourceEpoch == 1);
        CHECK_FALSE(subscription->getStatistics().sourceEpoch);
        CHECK_FALSE(endpoint->getEffectiveConfiguration().acceptedAddress);
        CHECK_FALSE(endpoint->getAggregationPeriodInfo());
        const auto connection = endpoint->getStatistics().connection;
        CHECK(connection.readBytes.availability == q::core::StatisticAvailability::UNSUPPORTED);
        CHECK(connection.writtenSubscriptionRecords.availability == q::core::StatisticAvailability::UNSUPPORTED);
        CHECK(connection.connectionAttempts.availability == q::core::StatisticAvailability::UNSUPPORTED);
        CHECK_FALSE(connection.sourceEpoch);
        CHECK(connection.activeConnections == 0);
    }
}

TEST_CASE("active latest requests refuse a selected tape while passive TICKER playback still reads the cache") {
    std::optional<q::events::Quote> passive;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto feed = endpoint->getFeed();
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    const auto pending = feed->requestLastEvent<q::events::Quote>("IBM", deadline);
    CHECK_FALSE(pending.isDone());
    CHECK(endpoint->getStatistics().connection.connectionAttempts.value == 0);

    const auto subscription = feed->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&passive, weak = std::weak_ptr(feed)](std::span<const q::events::Quote>) {
        if (const auto owner = weak.lock()) {
            if (auto value = owner->getLastEventIfSubscribed<q::events::Quote>("IBM"))
                passive = std::move(value);
        }
    });
    const auto diagnostic = [](auto operation) {
        try {
            operation();
        } catch (const api::Error &error) {
            return error.diagnostic();
        }

        return api::Diagnostic{};
    };

    endpoint->connectTape(RAW_QUOTE);
    CHECK(pending.isDone());
    const auto selected = diagnostic([&] {
        static_cast<void>(pending.await());
    });
    const auto fixed = diagnostic([&] {
        static_cast<void>(feed->requestLastEvent<q::events::Quote>("IBM", deadline));
    });
    CHECK(selected.code == api::ErrorCode::UNSUPPORTED_PROFILE);
    CHECK(fixed.code == api::ErrorCode::UNSUPPORTED_PROFILE);
    CHECK(selected.stage == "latest-event-request-tape");
    CHECK(fixed.stage == selected.stage);
    CHECK_FALSE(pending.cancel());

    expectCleanEof(endpoint);
    REQUIRE(passive);
    CHECK(passive->symbol == u"IBM");
    CHECK(passive->bidPrice.raw > 0);
    CHECK(diagnostic([&] {
              static_cast<void>(pending.await());
          }).code == api::ErrorCode::UNSUPPORTED_PROFILE);
}

TEST_CASE("explicit HISTORY tape preserves typed trades and does not produce STREAM events") {
    for (const auto contract : {api::DeliveryContract::HISTORY, api::DeliveryContract::STREAM}) {
        CAPTURE(contract);
        std::vector<q::events::TimeAndSale> received;
        api::ApiRuntime runtime(runtimeOptions());
        api::EndpointOptions options;
        options.contract = contract;
        const auto endpoint = runtime.createEndpoint(options);
        const auto feed = endpoint->getFeed();
        const auto subscription = contract == api::DeliveryContract::HISTORY
                                      ? feed->createTimeSeriesSubscription<q::events::TimeAndSale>()
                                      : feed->createSubscription<q::events::TimeAndSale>();

        if (contract == api::DeliveryContract::HISTORY) {
            subscription->setFromTime(0);
        }

        subscription->addSymbols("AAPL");
        subscription->addEventListener([&received](auto batch) {
            received.insert(received.end(), batch.begin(), batch.end());
        });
        endpoint->connectTape(HISTORY_SALE);
        expectCleanEof(endpoint);

        if (contract == api::DeliveryContract::HISTORY) {
            REQUIRE(received.size() == 3);
            CHECK(std::ranges::any_of(received, [](const auto &sale) {
                return sale.symbol == u"AAPL" && sale.tradeId == 9007199254740993LL && sale.getTime() == 2001;
            }));
        } else {
            CHECK(received.empty());
        }
    }
}

TEST_CASE("local tape path and quotas reject cleanly and malformed input is not clean EOF") {
    struct Refusal {
        std::filesystem::path path;
        api::TapeSourceOptions options;
        api::ErrorCode expected;
    };

    const Refusal refusals[] = {
        {FIXTURES / "nonexistent-tape-feed-file.qtp", {}, api::ErrorCode::TAPE_IO_FAILURE},
        {FIXTURES, {}, api::ErrorCode::TAPE_IO_FAILURE},
        {RAW_QUOTE, {1, 65'536}, api::ErrorCode::RESOURCE_LIMIT},
        {RAW_QUOTE, {64 * 1024 * 1024, 1}, api::ErrorCode::RESOURCE_LIMIT},
        {FIXTURES / "protocol/raw-tape/expected/timestamp-truncated-1.qtp", {}, api::ErrorCode::PROTOCOL_FAILURE}};

    for (const auto &refusal : refusals) {
        api::ApiRuntime runtime(runtimeOptions());
        const auto endpoint = runtime.createEndpoint();
        const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("IBM");
        endpoint->connectTape(refusal.path, refusal.options);
        const auto state = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 8s);
        const auto closed = endpoint->closeAndAwaitTermination();
        INFO("state=", static_cast<int>(state.state), ", stage=", closed.diagnostic.stage,
             ", code=", static_cast<int>(closed.diagnostic.code));
        REQUIRE(state.status == api::WaitStatus::REACHED);
        CHECK_FALSE(closed.drained);
        CHECK(closed.diagnostic.code == refusal.expected);
        CHECK(endpoint->getStatistics().final);
    }

    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    CHECK_THROWS_AS(endpoint->connectTape({}, {}), api::Error);
    CHECK_THROWS_AS(endpoint->connectTape(RAW_QUOTE, {0, 1}), api::Error);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
}
} // namespace
