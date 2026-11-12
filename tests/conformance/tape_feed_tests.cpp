// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "tape_playback_clock.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace api = q::api;
using namespace std::chrono_literals;

const std::filesystem::path FIXTURES{ORG_TTLDTOR_QDCXX_TAPE_FEED_FIXTURES};
const auto RAW_QUOTE = FIXTURES / "protocol/raw-tape/expected/raw-quote.qtp";
const auto HISTORY_SALE = FIXTURES / "time-and-sale/wide/history.qtp";
const auto JAVA_QUOTE_TAPES = FIXTURES / "protocol/real-tape/producer/current/expected";

/** Retain one host task so the test controls execution across a source cut. */
class HeldTapeCallbacks final : public api::CallbackExecutor {
    std::mutex mutex_;
    std::condition_variable posted_;
    std::optional<api::CallbackTask> task_;

    public:
    api::CallbackPostResult post(api::CallbackTask task) override {
        {
            std::lock_guard lock(mutex_);

            if (task_) {
                return api::CallbackPostResult::REJECTED;
            }

            task_.emplace(std::move(task));
        }

        posted_.notify_one();

        return api::CallbackPostResult::ACCEPTED;
    }

    std::optional<api::CallbackTask> take() {
        std::unique_lock lock(mutex_);

        if (!posted_.wait_for(lock, 5s, [&] {
                return task_.has_value();
            })) {
            return std::nullopt;
        }

        return std::exchange(task_, std::nullopt);
    }
};

/** Keep the HISTORY source open past its first callback without changing the fixture's data. */
class PaddedHistoryTape {
    std::filesystem::path directory_;

    public:
    PaddedHistoryTape() {
        static std::atomic<unsigned> sequence = 0;

        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            auto candidate =
                std::filesystem::temp_directory_path() / ("qdcxx-history-restart-" + std::to_string(sequence++));
            std::error_code error;

            if (std::filesystem::create_directory(candidate, error)) {
                directory_ = std::move(candidate);

                return;
            }
        }

        FAIL("Could not exclusively reserve a tape fixture directory");
    }

    ~PaddedHistoryTape() {
        std::error_code error;
        std::filesystem::remove(directory_ / "history.qtp", error);
        std::filesystem::remove(directory_, error);
    }

    std::filesystem::path write() const {
        const auto path = directory_ / "history.qtp";
        REQUIRE(std::filesystem::copy_file(HISTORY_SALE, path));
        std::ofstream output(path, std::ios::binary | std::ios::app);
        constexpr std::array<char, 256> emptyEnvelopes{};
        output.write(emptyEnvelopes.data(), static_cast<std::streamsize>(emptyEnvelopes.size()));
        output.close();
        REQUIRE(output);

        return path;
    }
};

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
        CHECK(received.back().eventTime == 1600000002123);

        if (contract == api::DeliveryContract::STREAM) {
            REQUIRE(received.size() == 2);
            CHECK(received.front().bidPrice.raw == 2566530);
            CHECK(received.front().eventTime == 1600000000123);
            CHECK_FALSE(latest);
        } else {
            REQUIRE(latest);
            CHECK(latest->symbol == u"IBM");
            CHECK(latest->bidPrice.raw == received.front().bidPrice.raw);
            CHECK(latest->eventTime == received.front().eventTime);
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

TEST_CASE("tape restart cancels a queued old callback and reopens from byte zero on the same subscription") {
    using Clock = std::chrono::steady_clock;
    std::vector<q::events::Quote> quotes;
    std::optional<std::uint64_t> observedEpoch;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint =
        runtime.createEndpoint({.contract = api::DeliveryContract::STREAM, .pollInterval = 2s, .drainTimeout = 1s});
    const auto policy = std::make_shared<HeldTapeCallbacks>();
    endpoint->setEventExecutor(policy);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&quotes, &observedEpoch, owner = std::weak_ptr(subscription)](auto batch) {
        quotes.insert(quotes.end(), batch.begin(), batch.end());

        if (const auto current = owner.lock()) {
            observedEpoch = current->getStatistics().sourceEpoch;
        }
    });
    CHECK_THROWS_AS(endpoint->restartTape(), api::Error);
    CHECK_THROWS_AS(endpoint->stopTape(), api::Error);
    endpoint->connectTape(RAW_QUOTE, {.playback = api::TapePlaybackPolicy::SOURCE_TIME, .speedPercent = 1});
    auto stale = policy->take();
    REQUIRE(stale);
    CHECK(subscription->getStatistics().sourceEpoch == 1);
    endpoint->restartTape();
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, Clock::now() + 5s).status == api::WaitStatus::REACHED);
    auto first = policy->take();
    REQUIRE(first);
    CHECK(stale->run() == api::CallbackTaskRunResult::CANCELLED);
    CHECK(first->run() == api::CallbackTaskRunResult::COMPLETED);
    REQUIRE(quotes.size() == 1);
    CHECK(quotes.front().bidPrice.raw == 2566530);
    CHECK(observedEpoch == 2);

    endpoint->stopTape(api::TapeStopMode::ABORT);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.drained);
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(closed.hadGap);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(subscription->getStatistics().final);
    CHECK_THROWS_AS(endpoint->restartTape(), api::Error);
}

TEST_CASE("tape restart invalidates old HISTORY coverage and rebuilds it on a new epoch") {
    using Clock = std::chrono::steady_clock;
    const PaddedHistoryTape tape;
    const auto path = tape.write();
    std::atomic<std::size_t> events = 0;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint({.contract = api::DeliveryContract::HISTORY, .drainTimeout = 5s});
    const auto subscription = endpoint->getFeed()->createTimeSeriesSubscription<q::events::TimeAndSale>();
    subscription->setFromTime(0);
    subscription->addSymbols("AAPL");
    subscription->addEventListener([&events, owner = std::weak_ptr(endpoint)](auto batch) {
        if (const auto current = owner.lock()) {
            current->setTapePaused(true);
        }

        events += batch.size();
    });

    const auto complete = [](const api::HistorySubscriptionSnapshot &cut, std::uint64_t epoch) {
        return cut.state == api::HistoryObservationState::CURRENT && cut.sourceEpoch == epoch &&
               cut.ranges.size() == 1 && cut.ranges.front().status.completeDeliveredRange;
    };
    const auto awaitHistory = [&](std::uint64_t epoch) {
        const auto deadline = Clock::now() + 5s;

        do {
            const auto snapshot = subscription->getHistorySnapshot();

            if (complete(snapshot, epoch)) {
                return true;
            }

            std::this_thread::sleep_for(1ms);
        } while (Clock::now() < deadline);

        return false;
    };

    endpoint->connectTape(path);
    const auto firstComplete = awaitHistory(1);
    INFO("first cut state=", static_cast<unsigned>(subscription->getHistorySnapshot().state),
         ", endpoint=", static_cast<unsigned>(endpoint->getState()), ", events=", events.load());
    REQUIRE(firstComplete);
    CHECK(events.load() == 3);
    endpoint->restartTape();
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, Clock::now() + 5s).status == api::WaitStatus::REACHED);
    const auto between = subscription->getHistorySnapshot();
    CHECK_FALSE(complete(between, 1));
    CHECK_FALSE(complete(between, 2));
    CHECK(events.load() == 3);
    endpoint->setTapePaused(false);
    REQUIRE(awaitHistory(2));
    CHECK(events.load() == 6);

    endpoint->stopTape();
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(subscription->getHistorySnapshot().final);
}

TEST_CASE("a terminal stop cancels a restart selected from the last delivered callback") {
    std::vector<std::uint64_t> epochs;
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint = runtime.createEndpoint({.contract = api::DeliveryContract::STREAM});
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&, owner = std::weak_ptr(endpoint), sub = std::weak_ptr(subscription)](auto) {
        if (const auto current = sub.lock()) {
            epochs.push_back(current->getStatistics().sourceEpoch.value_or(0));
        }

        if (const auto current = owner.lock()) {
            current->restartTape();
            current->stopTape();
        }
    });
    endpoint->connectTape(RAW_QUOTE);
    expectCleanEof(endpoint);
    CHECK(epochs == std::vector<std::uint64_t>{1});
}

TEST_CASE("tape restart times out without overlapping a running old callback") {
    using Clock = std::chrono::steady_clock;
    std::promise<void> entered;
    std::promise<void> release;
    auto started = entered.get_future();
    const auto proceed = release.get_future();
    api::ApiRuntime runtime(runtimeOptions());
    const auto endpoint =
        runtime.createEndpoint({.contract = api::DeliveryContract::STREAM, .pollInterval = 2s, .drainTimeout = 250ms});
    const auto policy = std::make_shared<HeldTapeCallbacks>();
    endpoint->setEventExecutor(policy);
    const auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([&](auto) {
        entered.set_value();
        static_cast<void>(proceed.wait_for(5s));
    });
    endpoint->connectTape(RAW_QUOTE);
    auto task = policy->take();
    REQUIRE(task);
    std::jthread host([&] {
        static_cast<void>(task->run());
    });
    REQUIRE(started.wait_for(5s) == std::future_status::ready);
    endpoint->restartTape();
    const auto failed = endpoint->awaitState(api::EndpointState::FAILED, Clock::now() + 1500ms);
    CHECK(failed.status == api::WaitStatus::REACHED);
    CHECK_FALSE(subscription->getStatistics().sourceEpoch);
    CHECK_FALSE(endpoint->getStatistics().final);
    release.set_value();

    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.diagnostic.code == api::ErrorCode::TAPE_RESTART_TIMEOUT);
    CHECK(closed.deadlineExpired);
    CHECK_FALSE(closed.drained);
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
