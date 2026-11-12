// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "tape_playback_clock.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <future>
#include <limits>
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
const auto JAVA_QUOTE_TAPES = FIXTURES / "protocol/real-tape/producer/current/expected";

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
        std::optional<std::uint64_t> observedSourceEpoch;
        api::ApiRuntime runtime(runtimeOptions());
        api::EndpointOptions options;
        options.contract = contract;
        const auto endpoint = runtime.createEndpoint(options);
        const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols("IBM");
        subscription->addEventListener(
            [&received, &observedSourceEpoch, weak = std::weak_ptr(subscription)](auto batch) {
                received.insert(received.end(), batch.begin(), batch.end());

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
        CHECK(received.back().eventTime == 1600000002123);

        if (contract == api::DeliveryContract::STREAM) {
            REQUIRE(received.size() == 2);
            CHECK(received.front().bidPrice.raw == 2566530);
            CHECK(received.front().eventTime == 1600000000123);
        }

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
                return sale.symbol == u"AAPL" && sale.tradeId == 9007199254740993LL && sale.getTime() == 2001 &&
                       sale.eventTime == 7200;
            }));
        } else {
            CHECK(received.empty());
        }
    }
}

TEST_CASE("relative tape timeline scales from its first timestamp and freezes the pending deadline while paused") {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::time_point{} + 1s;
    const api::TapeSourceOptions options{.playback = api::TapePlaybackPolicy::SOURCE_TIME, .speedPercent = 200};
    api::detail::TapePlaybackClock clock(options);
    CHECK(clock.schedule(std::nullopt, start));
    CHECK_FALSE(clock.deadline());
    CHECK(clock.schedule(1000, start));
    CHECK(clock.ready(start));
    CHECK(clock.schedule(1001, start));
    CHECK(clock.deadline() == start + 1ms);
    CHECK(clock.schedule(2000, start));
    CHECK(clock.deadline() == start + 500ms);
    CHECK_FALSE(clock.ready(start + 499ms));
    CHECK(clock.setPaused(true, start + 100ms));
    CHECK_FALSE(clock.ready(start + 30s));
    CHECK_FALSE(clock.deadline());
    CHECK(clock.setPaused(false, start + 1100ms));
    CHECK(clock.deadline() == start + 1500ms);
    CHECK(clock.ready(start + 1500ms));
    clock.delivered();
    CHECK(clock.schedule(1500, start + 1500ms));
    CHECK(clock.deadline() == start + 1500ms);

    api::detail::TapePlaybackClock extreme(options);
    CHECK(extreme.schedule(std::numeric_limits<std::int64_t>::min(), start));
    CHECK_FALSE(extreme.schedule(std::numeric_limits<std::int64_t>::max(), start));
}

TEST_CASE("initially paused Feed resumes paced Quote delivery with the original Java event times") {
    using Clock = std::chrono::steady_clock;
    std::vector<q::events::Quote> received;
    std::atomic<std::size_t> callbacks = 0;
    Clock::time_point lastCallback;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    CHECK_THROWS_AS(endpoint->setTapePaused(true), api::Error);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&](auto batch) {
        received.insert(received.end(), batch.begin(), batch.end());
        lastCallback = Clock::now();
        ++callbacks;
    });

    const api::TapeSourceOptions options{
        .playback = api::TapePlaybackPolicy::SOURCE_TIME, .speedPercent = 1000, .initiallyPaused = true};
    endpoint->connectTape(RAW_QUOTE, options);
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, Clock::now() + 5s).status == api::WaitStatus::REACHED);
    CHECK(callbacks.load() == 0);
    const auto resume = Clock::now();
    endpoint->setTapePaused(false);
    expectCleanEof(endpoint);
    REQUIRE(received.size() == 2);
    CHECK(received[0].eventTime == 1600000000123);
    CHECK(received[1].eventTime == 1600000002123);
    CHECK(lastCallback >= resume + 200ms);
    CHECK_THROWS_AS(endpoint->setTapePaused(true), api::Error);
}

TEST_CASE("tape EOF without times and a malformed timed suffix retain different outcomes") {
    for (const auto malformed : {false, true}) {
        CAPTURE(malformed);
        std::vector<q::events::Quote> received;
        api::ApiRuntime runtime(runtimeOptions());
        const auto endpoint = runtime.createEndpoint(api::EndpointOptions{.contract = api::DeliveryContract::STREAM});
        const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        subscription->addSymbols({"IBM", "MSFT"});
        subscription->addEventListener([&](auto batch) {
            received.insert(received.end(), batch.begin(), batch.end());
        });
        const auto name = malformed ? "quote-truncated.qtp" : "quote-none.qtp";
        const api::TapeSourceOptions options{.playback = api::TapePlaybackPolicy::SOURCE_TIME,
                                             .speedPercent = 10'000,
                                             .maximumSourceSpan = malformed ? 24h : 1ms};
        endpoint->connectTape(JAVA_QUOTE_TAPES / name, options);

        if (malformed) {
            const auto result = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 5s);
            REQUIRE(result.status == api::WaitStatus::REACHED);
            const auto close = endpoint->closeAndAwaitTermination();
            CHECK(close.diagnostic.code == api::ErrorCode::PROTOCOL_FAILURE);
            CHECK(close.hadGap);
            REQUIRE(received.size() == 2);
            CHECK(received[0].eventTime == 1600000000123);
            CHECK(received[1].eventTime == 1600000001123);
        } else {
            expectCleanEof(endpoint);
            REQUIRE(received.size() == 3);
            CHECK(std::ranges::all_of(received, [](const auto &value) {
                return value.eventTime == 0;
            }));
        }
    }
}

TEST_CASE("closing a paced Feed does not wait for a future envelope") {
    std::promise<void> first;
    auto delivered = first.get_future();
    std::size_t callbacks = 0;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&](auto batch) {
        if (callbacks == 0) {
            first.set_value();
        }

        callbacks += batch.size();
    });
    endpoint->connectTape(RAW_QUOTE, {.playback = api::TapePlaybackPolicy::SOURCE_TIME, .speedPercent = 1});
    REQUIRE(delivered.wait_for(5s) == std::future_status::ready);
    const auto close = endpoint->closeAndAwaitTermination();
    CHECK(close.drained);
    CHECK_FALSE(close.deadlineExpired);
    CHECK(callbacks == 1);
}

TEST_CASE("relative tape source span refusal preserves the admitted prefix") {
    std::vector<q::events::Quote> received;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint();
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&](auto batch) {
        received.insert(received.end(), batch.begin(), batch.end());
    });
    CHECK_THROWS_AS(
        endpoint->connectTape(RAW_QUOTE, {.playback = api::TapePlaybackPolicy::SOURCE_TIME, .speedPercent = 0}),
        api::Error);
    endpoint->connectTape(RAW_QUOTE, {.playback = api::TapePlaybackPolicy::SOURCE_TIME, .maximumSourceSpan = 1ms});
    const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 5s);
    REQUIRE(failed.status == api::WaitStatus::REACHED);
    const auto close = endpoint->closeAndAwaitTermination();
    CHECK(close.diagnostic.code == api::ErrorCode::TAPE_TIMING_RANGE);
    CHECK(close.hadGap);
    REQUIRE(received.size() == 1);
    CHECK(received.front().eventTime == 1600000000123);
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
