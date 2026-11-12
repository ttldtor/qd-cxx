// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "evidence_cells.hpp"
#include "qd/api/feed.hpp"
#include "qd/api/time_series_event_model.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/option_sale.hpp"
#include "qd/events/time_and_sale.hpp"
#include "qd/protocol/frame_reader.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "recovery_test_peer.hpp"
#include "recovery_test_support.hpp"

#include <algorithm>
#include <bit>
#include <chrono>
#include <condition_variable>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace c = q::collectors;
namespace e = q::events;
namespace p = q::protocol;
namespace wire = q::encoding;
using namespace std::chrono_literals;

template <class Event> constexpr std::int32_t HISTORY_RECORD_ID = std::is_same_v<Event, e::TimeAndSale> ? 12 : 39;

struct Expected {
    std::int64_t rawCoordinate;
    std::int32_t eventFlags;
    std::int64_t index;
    std::int64_t timeSequence;
    std::int64_t time;
    std::int32_t sequence;
    std::int32_t type;
    std::int64_t priceBits;
    std::int32_t source;
};

/** Read the Java QDAgent's actual phase outputs in their original order. */
std::map<std::string, std::vector<Expected>> oracle(const std::filesystem::path &path) {
    std::ifstream input(path);
    REQUIRE(input);
    std::map<std::string, std::vector<Expected>> result;

    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        const auto cells = q::test_support::split(line);

        if (cells[0] != "output")
            continue;

        REQUIRE(cells.size() == 12);
        result[cells[1]].push_back({std::stoll(cells[3]), std::stoi(cells[4]), std::stoll(cells[5]),
                                    std::stoll(cells[6]), std::stoll(cells[7]), std::stoi(cells[8]),
                                    std::stoi(cells[9]), std::stoll(cells[10]), std::stoi(cells[11])});
    }

    return result;
}

/** Read one whole composer segment without inferring or regenerating QTP contents. */
std::vector<std::byte> bytes(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input);
    const std::vector<char> raw((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const auto view = std::as_bytes(std::span(raw));

    return {view.begin(), view.end()};
}

q::test::TcpPeerScript peerScript(const std::filesystem::path &directory, std::size_t count) {
    q::test::TcpPeerScript script;
    script.initialBytes = q::test::recovery::ready();
    script.readThroughMessage = std::to_underlying(q::protocol::SubscriptionMessage::HISTORY_ADD);
    script.waitForClientClose = true;

    for (std::size_t index = 0; index < count; ++index) {
        const auto name = std::string("wire-0") + std::to_string(index) + ".qtp";
        script.stagedBytes.push_back(bytes(directory / name));
    }

    return script;
}

std::optional<std::int32_t> demandRecord(std::span<const std::byte> captured, p::SubscriptionMessage message,
                                         std::optional<std::int64_t> expectedBound) {
    p::FrameReader frames({1024 * 1024, 1024 * 1024 + 9});
    std::optional<std::int32_t> found;

    while (!captured.empty()) {
        const auto frame = frames.feed(captured);
        REQUIRE(frame.status != p::FrameStatus::FAILED);
        REQUIRE(frame.accepted > 0);
        captured = captured.subspan(frame.accepted);

        if (!frame.frame || frame.frame->type != std::to_underlying(message))
            continue;

        REQUIRE_FALSE(found);
        auto payload = std::span(frame.frame->payload);
        wire::SymbolReader symbols({8192, 128, 128}, 0);
        const auto symbol = symbols.read(payload);
        REQUIRE(symbol.status == wire::SymbolStatus::OK);
        REQUIRE(symbol.value);
        CHECK(symbol.value->units == u"IBM");
        CHECK(symbol.value->eventFlags == 0);
        payload = payload.subspan(symbol.consumed);
        const auto record = wire::readCompactInt(payload);
        REQUIRE(record.status == wire::Status::OK);
        CHECK(record.value >= 0);
        found = record.value;
        payload = payload.subspan(record.consumed);

        if (expectedBound) {
            const auto bound = wire::readCompactLong(payload);
            REQUIRE(bound.status == wire::Status::OK);
            CHECK(bound.value == *expectedBound);
            payload = payload.subspan(bound.consumed);
        }

        CHECK(payload.empty());
    }

    return found;
}

void checkDemand(std::span<const std::byte> captured, std::int32_t record, std::int64_t bound = 0) {
    const auto actual = demandRecord(captured, p::SubscriptionMessage::HISTORY_ADD, bound);
    REQUIRE(actual);
    CHECK(*actual == record);
}

template <class Event> std::int64_t timeSequence(const Event &event) {
    if constexpr (std::is_same_v<Event, e::OptionSale>)
        return event.getTimeSequence();
    else
        return event.getIndex();
}

/** Raw source coordinates and business indexes are deliberately separate for OptionSale. */
template <class Event> void checkValue(const Event &event, const Expected &expected, bool full) {
    INFO("raw coordinate=", expected.rawCoordinate, " timeSequence=", expected.timeSequence,
         " expected flags=", expected.eventFlags);
    CHECK(event.symbol == u"IBM");
    CHECK(event.eventFlags == expected.eventFlags);
    CHECK(event.getIndex() == expected.index);
    CHECK(event.getSource().id() == expected.source);

    if constexpr (std::is_same_v<Event, e::OptionSale>) {
        CHECK(static_cast<std::uint32_t>(event.getIndex()) == static_cast<std::uint64_t>(expected.rawCoordinate));
    } else {
        CHECK(event.getIndex() == expected.rawCoordinate);
    }

    if (full) {
        CHECK(timeSequence(event) == expected.timeSequence);
        CHECK(event.getTime() == expected.time);
        CHECK(event.getSequence() == expected.sequence);
        CHECK(e::marketEnumCode(event.getType()) == expected.type);
        CHECK(std::bit_cast<std::int64_t>(event.getPrice()) == expected.priceBits);
    }
}

template <class Event> class Recorder {
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<Event> events_;

    public:
    void append(std::span<const Event> batch) {
        {
            const std::lock_guard lock(mutex_);
            events_.insert(events_.end(), batch.begin(), batch.end());
        }
        changed_.notify_all();
    }

    std::vector<Event> wait(std::size_t count) {
        std::unique_lock lock(mutex_);
        REQUIRE(changed_.wait_for(lock, 5s, [&] {
            return events_.size() >= count;
        }));

        return events_;
    }
};

/** Wait for a qualified publication and apply its predicate without pumping callbacks or I/O. */
template <class Subscription, class Predicate>
a::HistorySubscriptionSnapshot observe(const std::shared_ptr<Subscription> &subscription, Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    auto cut = subscription->getHistorySnapshot();

    while (std::chrono::steady_clock::now() < deadline) {
        if (cut.state == a::HistoryObservationState::CURRENT && cut.ranges.size() == 1 &&
            cut.ranges.front().status.source && predicate(cut))
            return cut;

        std::this_thread::sleep_for(1ms);
        cut = subscription->getHistorySnapshot();
    }

    INFO("history state=", static_cast<int>(cut.state), " ranges=", cut.ranges.size());
    FAIL("Timed out waiting for a qualified HISTORY publication");

    return cut;
}

template <class Event> void verify(const std::filesystem::path &directory, bool closeWhilePending) {
    const auto expected = oracle(directory / "oracle.tsv");
    q::test::TcpPeerConfig config;
    config.sessions.push_back(peerScript(directory, 8));
    config.sessions.push_back(peerScript(directory / "fresh", 2));
    config.lifetime = 45s;
    q::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);

    auto recorder = std::make_shared<Recorder<Event>>();
    a::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 2;
    const auto endpoint = runtime.createEndpoint(options);
    const auto feed = endpoint->getFeed();
    std::shared_ptr<a::Subscription<Event>> subscription;

    if constexpr (std::is_same_v<Event, e::TimeAndSale>) {
        subscription = feed->template createTimeSeriesSubscription<Event>();
        subscription->setFromTime(0);
    } else {
        subscription = feed->template createSubscription<Event>();
    }

    subscription->addSymbols("IBM");
    subscription->addEventListener([recorder](std::span<const Event> batch) {
        recorder->append(batch);
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));

    const auto firstDemandDeadline = std::chrono::steady_clock::now() + 5s;

    while (!peer.snapshot().sessions[0].matched && std::chrono::steady_clock::now() < firstDemandDeadline)
        std::this_thread::sleep_for(1ms);

    REQUIRE(peer.snapshot().sessions[0].matched);
    checkDemand(peer.snapshot().sessions[0].received, HISTORY_RECORD_ID<Event>);

    peer.allowStages(0, 2); // BEGIN plus the record above the low bound.
    auto events = recorder->wait(2);
    REQUIRE(expected.at("low-partial").size() == 2);
    checkValue(events[0], expected.at("low-partial")[0], true);
    checkValue(events[1], expected.at("low-partial")[1], true);
    const auto partial = observe(subscription, [](const auto &cut) {
        const auto &status = cut.ranges.front().status;

        return status.source->snapshot == c::HistorySnapshotState::RECEIVING && !status.completeDeliveredRange;
    });

    peer.allowStages(0, 3);
    events = recorder->wait(3);
    REQUIRE(expected.at("low-completed").size() == 1);
    checkValue(events[2], expected.at("low-completed")[0], false);
    const auto complete = observe(subscription, [](const auto &cut) {
        return cut.ranges.front().status.completeDeliveredRange;
    });
    REQUIRE(complete.sourceEpoch);
    CHECK(complete.ranges.front().status.source->snapshot == c::HistorySnapshotState::COMPLETE);

    peer.allowStages(0, 4);
    events = recorder->wait(4);
    REQUIRE(expected.at("low-correction-pending").size() == 1);
    checkValue(events[3], expected.at("low-correction-pending")[0], true);
    CHECK(events[3].isCorrection());
    CHECK((events[3].eventFlags & e::indexed_event_flags::TX_PENDING) != 0);
    CHECK((events[3].eventFlags & e::indexed_event_flags::REMOVE_EVENT) == 0);
    const auto pending = observe(subscription, [](const auto &cut) {
        const auto &status = cut.ranges.front().status;

        return status.source->sourceTransactionPending && !status.completeDeliveredRange;
    });

    if (closeWhilePending) {
        const auto closed = endpoint->closeAndAwaitTermination();
        CHECK_FALSE(closed.deadlineExpired);
        CHECK(subscription->getStatistics().final);
        CHECK(subscription->getHistorySnapshot().state == a::HistoryObservationState::RETIRED);
        CHECK(pending.ranges.front().status.source->sourceTransactionPending);

        return;
    }

    peer.allowStages(0, 5);
    events = recorder->wait(5);
    REQUIRE(expected.at("low-cancel-committed").size() == 1);
    checkValue(events[4], expected.at("low-cancel-committed")[0], true);
    CHECK(events[4].isCancel());
    CHECK((events[4].eventFlags & e::indexed_event_flags::REMOVE_EVENT) == 0);
    CHECK(events[4].getIndex() == events[3].getIndex());
    static_cast<void>(observe(subscription, [](const auto &cut) {
        return cut.ranges.front().status.completeDeliveredRange;
    }));

    peer.allowStages(0, 6);
    events = recorder->wait(6);
    REQUIRE(expected.at("low-indexed-removal").size() == 1);
    checkValue(events[5], expected.at("low-indexed-removal")[0], false);
    CHECK((events[5].eventFlags & e::indexed_event_flags::REMOVE_EVENT) != 0);
    CHECK(events[5].getIndex() == events[4].getIndex());

    const auto &restartRows = expected.at("low-restarted");
    const auto restartedCount = 6 + restartRows.size();
    peer.allowStages(0, 7);
    events = recorder->wait(restartedCount);
    REQUIRE(events.size() == restartedCount);

    for (std::size_t index = 0; index < restartRows.size(); ++index)
        checkValue(events[6 + index], restartRows[index], true);

    static_cast<void>(observe(subscription, [](const auto &cut) {
        const auto &status = cut.ranges.front().status;

        return status.source->snapshot == c::HistorySnapshotState::RECEIVING && !status.completeDeliveredRange;
    }));

    peer.allowStages(0, 8);
    const auto &truncatedRows = expected.at("low-truncated");
    const auto truncatedCount = restartedCount + truncatedRows.size();
    events = recorder->wait(truncatedCount);
    REQUIRE(events.size() == truncatedCount);

    for (std::size_t index = 0; index < truncatedRows.size(); ++index) {
        const auto &row = truncatedRows[index];
        // A QTP removal contains only indexed coordinate metadata, even when a QDAgent retained old fields.
        checkValue(events[restartedCount + index], row, (row.eventFlags & e::indexed_event_flags::REMOVE_EVENT) == 0);
    }

    const auto truncated = observe(subscription, [](const auto &cut) {
        const auto &status = cut.ranges.front().status;

        return status.source->snapshot == c::HistorySnapshotState::TRUNCATED && !status.completeDeliveredRange;
    });
    REQUIRE(truncated.ranges.front().status.source->snipFrontier);
    CHECK(*truncated.ranges.front().status.source->snipFrontier == expected.at("low-truncated").back().rawCoordinate);
    const auto oldEpoch = truncated.sourceEpoch;
    REQUIRE(oldEpoch);

    endpoint->reconnect();
    const auto reconnectFence = subscription->getHistorySnapshot();
    CHECK((reconnectFence.state != a::HistoryObservationState::CURRENT || !reconnectFence.sourceEpoch ||
           reconnectFence.sourceEpoch != oldEpoch));

    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!peer.snapshot().sessions[1].matched && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(1ms);

    REQUIRE(peer.snapshot().sessions[1].matched);
    checkDemand(peer.snapshot().sessions[1].received, HISTORY_RECORD_ID<Event>);
    const auto beforeFresh = recorder->wait(truncatedCount);
    peer.allowStages(1, 1);
    events = recorder->wait(beforeFresh.size() + 1);
    REQUIRE(expected.at("fresh-partial").size() == 1);
    checkValue(events.back(), expected.at("fresh-partial")[0], true);
    const auto freshPartial = observe(subscription, [&](const auto &cut) {
        const auto &status = cut.ranges.front().status;

        return cut.sourceEpoch && cut.sourceEpoch != oldEpoch &&
               status.source->snapshot == c::HistorySnapshotState::RECEIVING && !status.completeDeliveredRange;
    });
    peer.allowStages(1, 2);
    events = recorder->wait(beforeFresh.size() + 2);
    REQUIRE(expected.at("fresh-completed").size() == 1);
    checkValue(events.back(), expected.at("fresh-completed")[0], false);
    const auto freshComplete = observe(subscription, [&](const auto &cut) {
        return cut.sourceEpoch && cut.sourceEpoch != oldEpoch && cut.ranges.front().status.completeDeliveredRange;
    });
    CHECK(freshComplete.sourceEpoch == freshPartial.sourceEpoch);
    CHECK(partial.ranges.front().status.source->snapshot == c::HistorySnapshotState::RECEIVING);
    CHECK(pending.ranges.front().status.source->sourceTransactionPending);
    CHECK(truncated.ranges.front().status.source->snapshot == c::HistorySnapshotState::TRUNCATED);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(subscription->getStatistics().final);
    CHECK(peer.snapshot().error == q::test::TcpPeerError::NONE);
}

void verifyHigherTimeRange(const std::filesystem::path &directory) {
    const auto expected = oracle(directory / "oracle.tsv");
    REQUIRE(expected.at("high-covered").size() == 2);
    const auto physicalBound = expected.at("high-covered").back().rawCoordinate;
    const auto fromTime = expected.at("high-covered").back().time;
    q::test::TcpPeerConfig config;
    config.sessions.push_back(peerScript(directory, 2));
    config.lifetime = 30s;
    q::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    a::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createTimeSeriesSubscription<e::TimeAndSale>();
    const auto recorder = std::make_shared<Recorder<e::TimeAndSale>>();
    subscription->setFromTime(fromTime);
    subscription->addSymbols("IBM");
    subscription->addEventListener([recorder](std::span<const e::TimeAndSale> batch) {
        recorder->append(batch);
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    const auto demandDeadline = std::chrono::steady_clock::now() + 5s;

    while (!peer.snapshot().sessions[0].matched && std::chrono::steady_clock::now() < demandDeadline)
        std::this_thread::sleep_for(1ms);

    REQUIRE(peer.snapshot().sessions[0].matched);
    checkDemand(peer.snapshot().sessions[0].received, HISTORY_RECORD_ID<e::TimeAndSale>, physicalBound);
    peer.allowStages(0, 1);
    auto values = recorder->wait(1);
    checkValue(values[0], expected.at("high-covered")[0], true);
    const auto partial = observe(subscription, [](const auto &cut) {
        return cut.ranges.front().status.source->snapshot == c::HistorySnapshotState::RECEIVING &&
               !cut.ranges.front().status.completeDeliveredRange;
    });
    CHECK(partial.ranges.front().status.requestedFrom == physicalBound);
    CHECK(partial.ranges.front().status.source->requestedFrom == physicalBound);
    peer.allowStages(0, 2);
    values = recorder->wait(2);
    checkValue(values[1], expected.at("high-covered")[1], true);
    CHECK((values[1].eventFlags & e::indexed_event_flags::SNAPSHOT_END) != 0);
    const auto covered = observe(subscription, [](const auto &cut) {
        return cut.ranges.front().status.completeDeliveredRange;
    });
    CHECK(covered.ranges.front().status.requestedFrom == physicalBound);
    CHECK(covered.ranges.front().status.source->requestedFrom == physicalBound);
    CHECK(covered.ranges.front().status.source->completeRequestedRange);
    CHECK(covered.sourceEpoch == partial.sourceEpoch);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(peer.snapshot().error == q::test::TcpPeerError::NONE);
}

struct CallbackGate {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    bool timedOut = false;
};

template <class Event> void verifyRemovePending(const std::filesystem::path &directory) {
    const auto expected = oracle(directory / "oracle.tsv");
    q::test::TcpPeerConfig config;
    config.sessions.push_back(peerScript(directory, 4));
    config.lifetime = 30s;
    q::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);
    a::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    const auto endpoint = runtime.createEndpoint(options);
    const auto feed = endpoint->getFeed();
    std::shared_ptr<a::Subscription<Event>> subscription;

    if constexpr (std::is_same_v<Event, e::TimeAndSale>) {
        subscription = feed->template createTimeSeriesSubscription<Event>();
        subscription->setFromTime(0);
    } else {
        subscription = feed->template createSubscription<Event>();
    }

    const auto recorder = std::make_shared<Recorder<Event>>();
    const auto gate = std::make_shared<CallbackGate>();
    subscription->addSymbols("IBM");
    subscription->addEventListener([recorder, gate](std::span<const Event> batch) {
        recorder->append(batch);

        if (std::ranges::any_of(batch, [](const auto &event) {
                return event.isCorrection();
            })) {
            std::unique_lock lock(gate->mutex);
            gate->entered = true;
            gate->changed.notify_all();
            gate->timedOut = !gate->changed.wait_for(lock, 10s, [&] {
                return gate->released;
            });
        }
    });
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    const auto demandDeadline = std::chrono::steady_clock::now() + 5s;

    while (!peer.snapshot().sessions[0].matched && std::chrono::steady_clock::now() < demandDeadline)
        std::this_thread::sleep_for(1ms);

    REQUIRE(peer.snapshot().sessions[0].matched);
    const auto addRecord = demandRecord(peer.snapshot().sessions[0].received, p::SubscriptionMessage::HISTORY_ADD, 0);
    REQUIRE(addRecord);
    CHECK(*addRecord == HISTORY_RECORD_ID<Event>);
    peer.allowStages(0, 3);
    recorder->wait(3);
    const auto complete = observe(subscription, [](const auto &cut) {
        return cut.ranges.front().status.completeDeliveredRange;
    });
    peer.allowStages(0, 4);
    {
        std::unique_lock lock(gate->mutex);
        REQUIRE(gate->changed.wait_for(lock, 5s, [&] {
            return gate->entered;
        }));
    }
    const auto values = recorder->wait(4);
    REQUIRE(expected.at("low-correction-pending").size() == 1);
    checkValue(values[3], expected.at("low-correction-pending")[0], true);
    CHECK((values[3].eventFlags & e::indexed_event_flags::TX_PENDING) != 0);
    subscription->removeSymbols("IBM");
    const auto removedRevision = subscription->getSnapshot().revision;
    CHECK(removedRevision > complete.logicalRevision);
    {
        const std::lock_guard lock(gate->mutex);
        gate->released = true;
    }
    gate->changed.notify_all();
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    std::optional<std::int32_t> removeRecord;

    while (std::chrono::steady_clock::now() < deadline) {
        removeRecord = demandRecord(peer.snapshot().sessions[0].received, p::SubscriptionMessage::HISTORY_REMOVE, {});

        if (removeRecord)
            break;

        std::this_thread::sleep_for(1ms);
    }

    REQUIRE(removeRecord);
    CHECK(*removeRecord == *addRecord);
    const auto retirementDeadline = std::chrono::steady_clock::now() + 5s;
    auto retired = subscription->getHistorySnapshot();

    while (std::chrono::steady_clock::now() < retirementDeadline &&
           !(retired.state == a::HistoryObservationState::NOT_MAPPED && retired.ranges.empty() &&
             retired.logicalRevision == removedRevision && retired.mappingLogicalRevision == removedRevision &&
             retired.callbackRetirementRevision > complete.callbackRetirementRevision)) {
        std::this_thread::sleep_for(1ms);
        retired = subscription->getHistorySnapshot();
    }

    CHECK(retired.state == a::HistoryObservationState::NOT_MAPPED);
    CHECK(retired.ranges.empty());
    CHECK(retired.mappingLogicalRevision == removedRevision);
    CHECK(retired.logicalRevision == removedRevision);
    CHECK(retired.callbackRetirementRevision > complete.callbackRetirementRevision);
    {
        const std::lock_guard lock(gate->mutex);
        CHECK_FALSE(gate->timedOut);
    }
    const auto closed = endpoint->closeAndAwaitTermination();
    INFO("deadline=", closed.deadlineExpired, ", gap=", closed.hadGap, ", stage=", closed.diagnostic.stage);
    CHECK(closed.drained);
    CHECK(peer.snapshot().error == q::test::TcpPeerError::NONE);
}
} // namespace

TEST_CASE("TimeAndSale keeps correction, cancel, removal and coverage distinct through reconnect") {
    verify<e::TimeAndSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "TimeAndSale", false);
}

TEST_CASE("OptionSale preserves signed index independently of time through reconnect") {
    verify<e::OptionSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "OptionSale", false);
}

TEST_CASE("TimeAndSale higher bound follows the Java covered-range projection") {
    verifyHigherTimeRange(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "TimeAndSale");
}

TEST_CASE("Removing sale demand during a pending callback retires that callback") {
    SUBCASE("TimeAndSale") {
        verifyRemovePending<e::TimeAndSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "TimeAndSale");
    }
    SUBCASE("OptionSale") {
        verifyRemovePending<e::OptionSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "OptionSale");
    }
}

TEST_CASE("Closing either sale subscription with an open transaction retires callbacks") {
    SUBCASE("TimeAndSale") {
        verify<e::TimeAndSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "TimeAndSale", true);
    }
    SUBCASE("OptionSale") {
        verify<e::OptionSale>(std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "OptionSale", true);
    }
}

TEST_CASE("an explicit time-series model follows qualified HISTORY replay across a real reconnect") {
    constexpr std::size_t OLD_COMPLETE_COUNT = 3;
    constexpr std::size_t OLD_PENDING_COUNT = 4;
    const auto directory = std::filesystem::path(QDCXX_SALE_LIFECYCLE_FIXTURES) / "TimeAndSale";
    q::test::TcpPeerConfig config;
    config.sessions.push_back(peerScript(directory, OLD_PENDING_COUNT));
    config.sessions.push_back(peerScript(directory / "fresh", 2));
    config.lifetime = 30s;
    q::test::OwnedTcpPeer peer(std::move(config));
    REQUIRE(peer.port() != 0);

    const auto recorder = std::make_shared<Recorder<e::TimeAndSale>>();
    a::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(limits);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 2;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createTimeSeriesSubscription<e::TimeAndSale>();
    subscription->setFromTime(0);
    subscription->addSymbols("IBM");
    subscription->addEventListener([recorder](std::span<const e::TimeAndSale> batch) {
        recorder->append(batch);
    });

    a::TimeSeriesEventModel<e::TimeAndSale> model;
    REQUIRE(model.setSymbol(u"IBM"));
    const auto range = model.setFromTime(0);
    REQUIRE(range);
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    peer.allowStages(0, OLD_COMPLETE_COUNT);
    const auto oldValues = recorder->wait(OLD_COMPLETE_COUNT);
    REQUIRE(oldValues.size() == OLD_COMPLETE_COUNT);
    const auto oldSource = observe(subscription, [](const auto &cut) {
        return cut.sourceEpoch && cut.ranges.front().status.completeDeliveredRange;
    });
    REQUIRE(oldSource.sourceEpoch);

    // The staged peer keeps the old and fresh batches separate. Qualify each owned group
    // with the epoch observed at its own delivery barrier, before releasing the next group.
    const auto oldEpoch = model.beginSourceEpoch(*oldSource.sourceEpoch);
    REQUIRE(oldEpoch);
    REQUIRE(model.processSourceEvents(*oldEpoch, *range, oldValues));
    REQUIRE(model.confirmSourceEpoch(*oldEpoch, *range));
    const auto completedOld = model.getSourceSnapshot();
    REQUIRE(completedOld);
    REQUIRE_FALSE(completedOld->snapshot.events.empty());

    peer.allowStages(0, OLD_PENDING_COUNT);
    const auto pendingValues = recorder->wait(OLD_PENDING_COUNT);
    REQUIRE(pendingValues.size() == OLD_PENDING_COUNT);
    const auto pending = observe(subscription, [&](const auto &cut) {
        return cut.sourceEpoch == oldSource.sourceEpoch && cut.ranges.front().status.source->sourceTransactionPending;
    });
    REQUIRE(pending.sourceEpoch == oldSource.sourceEpoch);
    REQUIRE(model.processSourceEvents(*oldEpoch, *range, std::span(pendingValues).last(1)));

    REQUIRE(model.markSourceGap(*oldEpoch, a::ModelRetention::RETAIN_COMPLETED));
    const auto historical = model.getSourceSnapshot();
    REQUIRE(historical);
    CHECK(historical->source.status == a::ModelSourceStatus::GAP);
    REQUIRE(historical->snapshot.events.size() == completedOld->snapshot.events.size());
    CHECK(historical->snapshot.events.front().getIndex() == completedOld->snapshot.events.front().getIndex());
    REQUIRE_FALSE(model.getSnapshot());

    endpoint->reconnect();
    peer.allowStages(1, 1);
    const auto freshStart = recorder->wait(OLD_PENDING_COUNT + 1);
    REQUIRE(freshStart.size() == OLD_PENDING_COUNT + 1);
    const auto freshSource = observe(subscription, [&](const auto &cut) {
        return cut.sourceEpoch && *cut.sourceEpoch > *oldSource.sourceEpoch &&
               cut.ranges.front().status.source->snapshot == c::HistorySnapshotState::RECEIVING;
    });
    REQUIRE(freshSource.sourceEpoch);
    const auto freshEpoch = model.beginSourceEpoch(*freshSource.sourceEpoch);
    REQUIRE(freshEpoch);
    REQUIRE(model.processSourceEvents(*freshEpoch, *range, std::span(freshStart).last(1)));
    const auto premature = model.confirmSourceEpoch(*freshEpoch, *range);
    REQUIRE_FALSE(premature);
    CHECK(premature.error() == a::ModelSourceError::REPLAY_INCOMPLETE);
    const auto delayedOld = model.processSourceEvents(*oldEpoch, *range, std::span(pendingValues).last(1));
    REQUIRE_FALSE(delayedOld);
    CHECK(delayedOld.error() == a::TimeSeriesEventModelError::SOURCE_EPOCH_MISMATCH);

    peer.allowStages(1, 2);
    const auto freshEnd = recorder->wait(OLD_PENDING_COUNT + 2);
    const auto freshComplete = observe(subscription, [&](const auto &cut) {
        return cut.sourceEpoch == freshSource.sourceEpoch && cut.ranges.front().status.completeDeliveredRange;
    });
    CHECK(freshComplete.sourceEpoch == freshSource.sourceEpoch);
    REQUIRE(model.processSourceEvents(*freshEpoch, *range, std::span(freshEnd).last(1)));
    REQUIRE(model.confirmSourceEpoch(*freshEpoch, *range));
    const auto current = model.getSourceSnapshot();
    REQUIRE(current);
    CHECK(current->source.status == a::ModelSourceStatus::CURRENT);
    CHECK(current->source.epochOrdinal == *freshSource.sourceEpoch);
    REQUIRE(current->snapshot.events.size() == 1);
    CHECK(current->snapshot.events.front().getIndex() == freshStart.back().getIndex());

    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK_FALSE(closed.deadlineExpired);
    CHECK(subscription->getHistorySnapshot().state == a::HistoryObservationState::RETIRED);
    REQUIRE(model.closeSource(*freshEpoch, a::ModelRetention::RETAIN_COMPLETED));
    const auto final = model.getSourceSnapshot();
    REQUIRE(final);
    CHECK(final->source.status == a::ModelSourceStatus::CLOSED);
    CHECK(final->snapshot.events.size() == 1);
    CHECK(current->source.status == a::ModelSourceStatus::CURRENT);
    CHECK(peer.snapshot().error == q::test::TcpPeerError::NONE);
}
