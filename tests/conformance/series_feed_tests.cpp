// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/events/event_flags.hpp"
#include "qd/events/series.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include "recovery_test_peer.hpp"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <variant>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace a = q::api;
namespace c = q::core;
namespace e = q::events;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;
constexpr c::SchemaLimits SCHEMA{16, 128, 256, 128, 16384};
constexpr std::int32_t DEFAULT_SERIES_RECORD_ID = 6;

/** @param success Exact expected test condition.
 * @param message Static test diagnostic.
 */
void require(bool success, const char *message) {
    if (!success) {
        throw std::runtime_error(message);
    }
}

/** Own independent copies across callback retirement. */
class Observation {
    std::mutex mutex_;
    std::condition_variable changed_;
    std::vector<e::Series> values_;

    public:
    /** @param batch Callback-owned views borrowed through return. */
    void append(std::span<const e::Series> batch) {
        {
            std::lock_guard guard(mutex_);
            values_.insert(values_.end(), batch.begin(), batch.end());
        }

        changed_.notify_all();
    }

    /** @param count Minimum expected number of delivered records.
     * @return Independent copy after the requested records arrive.
     */
    std::vector<e::Series> snapshot(std::size_t count = 2) {
        std::unique_lock guard(mutex_);
        require(changed_.wait_for(guard, 10s,
                                  [&] {
                                      return values_.size() >= count;
                                  }),
                "Series snapshot callback timeout");

        return values_;
    }
};

/** Recreate source physical storage for the deterministic peer only.
 * @param logical Existing standard validated native Series schema.
 * @return Peer-owned exact high VOID/Index slots and physical wide tails.
 */
c::LocalSchema physicalSchema(const c::LocalSchema &logical) {
    using Type = c::SerialFieldType;
    c::LocalRecord record{0, u"Series", false, {}, {}};
    const auto append = [&](std::u16string name, Type type) {
        record.lookups.push_back({std::move(name), record.fields.size()});
        record.fields.push_back({std::to_underlying(type)});
    };
    append(u"Void", Type::VOID_FIELD);

    for (const auto &lookup : logical.records()[0].lookups) {
        const auto type = static_cast<Type>(logical.records()[0].fields[lookup.fieldId].serialTypeId);
        append(lookup.name, type);

        if (type == Type::WIDE_DECIMAL) {
            append(lookup.name + u"$VoidTail", Type::VOID_FIELD);
        }
    }

    auto result = c::LocalSchema::create(std::array{record}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    require(bool(result), "Series physical test schema refused");

    return std::move(*result);
}

/** @param physical Peer-owned exact physical Series declarations.
 * @return One whole snapshot, with high unsigned collector index and terminal zero removal.
 */
std::array<peer::Record, 2> records(const c::LocalSchema &physical) {
    std::vector<c::FieldValue> fields;

    for (const auto &key : physical.records()[0].lookups) {
        const auto type = static_cast<c::SerialFieldType>(physical.records()[0].fields[key.fieldId].serialTypeId);
        std::int64_t raw = 0;

        if (key.name == u"Index") {
            raw = -1;
        } else if (key.name == u"Time" || key.name == u"Sequence") {
            raw = -1;
        } else if (key.name == u"Expiration") {
            raw = 20452;
        } else if (key.name == u"Volatility") {
            raw = 0x180;
        }

        fields.push_back({type, raw});
    }

    using namespace e::indexed_event_flags;

    return {peer::Record{u"AAPL&Q", fields, SNAPSHOT_BEGIN, {}, 0, q::protocol::FieldEventTime{7, 0x7fffffff, 0}},
            peer::Record{u"AAPL&Q", std::move(fields), REMOVE_EVENT | SNAPSHOT_END, 0, 0,
                         q::protocol::FieldEventTime{8, 0, 1}}};
}

/** @param received Owning public callback copies after synchronous snapshot.
 */
void verify(const std::vector<e::Series> &received) {
    using namespace e::indexed_event_flags;
    require(received.size() == 2, "Series selector overlap duplicated callbacks");
    const auto &begin = received.front();
    const auto &end = received.back();
    require(begin.symbol == u"AAPL&Q" && begin.getSource().id() == 0 && begin.getIndex() == -1 &&
                begin.eventFlags == SNAPSHOT_BEGIN && begin.eventTime == 7511 && begin.getTime() == 23 &&
                begin.getSequence() == 4194303 && begin.getExpiration() == 20452 && begin.getVolatility() == 1,
            "Series owning business/indexed/FIELD snapshot values differ");
    require(end.symbol == u"AAPL&Q" && end.getSource().id() == 0 && end.getIndex() == 0 && end.eventTime == 0 &&
                end.eventFlags == (REMOVE_EVENT | SNAPSHOT_END),
            "Series synthetic snapshot boundary was lost");
}

void run() {
    const auto logical = e::defaultSeriesSchema(SCHEMA);
    require(bool(logical), "Series standard logical schema refused");
    const auto physical = physicalSchema(*logical);
    a::EventRegistryWork work;
    const std::array routes{a::EventRegistration{a::EventType::SERIES, 0}};
    auto registry = a::EventRegistry::create(*logical, routes, {}, work);
    require(bool(registry), "Series standard route registration refused");

    auto invalidRecord = logical->records()[0];
    // Both business fields form a valid core layout, but neither is the native Series Index.
    invalidRecord.indexedTimeLayout = c::IndexedTimePairInt32{1, 2};
    const auto invalidSchema =
        c::LocalSchema::create(std::array{invalidRecord}, SCHEMA, c::LocalProfile::BINARY_FIELDS_V1);
    require(bool(invalidSchema), "Series layout test did not reach the public registry guard");
    const auto invalidRegistry = a::EventRegistry::create(*invalidSchema, routes, {}, work);
    require(!invalidRegistry && std::get_if<a::EventRegistryError>(&invalidRegistry.error().cause) &&
                std::get<a::EventRegistryError>(invalidRegistry.error().cause) == a::EventRegistryError::INVALID_INPUT,
            "Series registry accepted business Time/Sequence as its indexed coordinate");

    const auto plain = a::SubscriptionSelector::plain(u"AAPL&Q");
    const auto exact = a::SubscriptionSelector::indexed(u"AAPL&Q", e::IndexedEventSource::DEFAULT);
    const auto unsupported = a::SubscriptionSelector::indexed(u"SPY", e::OrderSource::NTV.indexedSource());
    const auto filter =
        a::FeedFilter::Builder(*registry, a::DeliveryContract::HISTORY).withCategory(u"all", u"*").build();
    require(filter.accept(a::EventType::SERIES, plain) && filter.accept(a::EventType::SERIES, exact) &&
                !filter.accept(a::EventType::SERIES, unsupported) &&
                !filter.accept(a::EventType::SERIES, a::SubscriptionSelector::all()),
            "Series filter selected a nondefault source or wildcard HISTORY route");

    a::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 4;
    runtimeOptions.maximumSymbolsPerSubscription = 4;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    options.readTimeout = 15000ms;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto typed = feed->createSubscription<e::Series>();
    auto mixed = feed->createSubscription({a::EventType::SERIES, a::EventType::SERIES});

    bool rejectedTimeSeries = false;

    try {
        static_cast<void>(feed->createTimeSeriesSubscription<e::Series>());
    } catch (const a::Error &failure) {
        rejectedTimeSeries = failure.diagnostic().code == a::ErrorCode::UNSUPPORTED_PROFILE;
    }

    require(rejectedTimeSeries, "Indexed Series acquired unsupported time-series activation");
    auto typedValues = std::make_shared<Observation>();
    auto mixedValues = std::make_shared<Observation>();
    typed->addEventListener([typedValues](auto batch) {
        typedValues->append(batch);
    });
    mixed->addEventListener([mixedValues](auto batch) {
        for (const auto &event : batch) {
            const auto *series = std::get_if<e::Series>(&event);
            require(series != nullptr, "Mixed Series used the wrong owning value alternative");
            mixedValues->append(std::span(series, 1));
        }
    });
    typed->setSelectors(std::array{plain, exact, unsupported});
    mixed->setSelectors(std::array{exact});

    peer::PeerScript script;
    script.family = c::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.subscriptionRecordId = DEFAULT_SERIES_RECORD_ID;
    script.fieldMetadata = true;
    script.canonicalSymbols = true;
    const std::array<std::u16string, 1> expected{u"AAPL&Q"};
    const auto initial = records(physical);
    peer::Peer source(physical, initial, expected, 31, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    verify(typedValues->snapshot());
    verify(mixedValues->snapshot());
    require(source.adds() == 1 && !source.failed(), "Series physical HISTORY ADD identity changed");
    require(endpoint->closeAndAwaitTermination().drained, "Series callbacks did not drain on close");
    runtime.closeAndAwaitTermination();
}

/** @param boundary END, SNIP, or a live removal that leaves TX_PENDING set. */
void verifyTransactionBoundary(std::int32_t boundary) {
    using namespace e::indexed_event_flags;
    using q::collectors::HistorySnapshotState;
    const auto logical = e::defaultSeriesSchema(SCHEMA);
    require(bool(logical), "Series standard logical schema refused");
    const auto physical = physicalSchema(*logical);
    auto events = records(physical);
    const bool liveRemoval = boundary == TX_PENDING;
    const bool truncated = boundary == SNAPSHOT_SNIP;
    auto reply = events.back();
    reply.flags = REMOVE_EVENT | boundary;
    // Series uses a zero high word and an unsigned 32-bit collector coordinate.
    constexpr std::int64_t highIndex = std::numeric_limits<std::uint32_t>::max();
    reply.removalTime = liveRemoval ? highIndex : truncated ? 1 : 0;

    if (!liveRemoval)
        events.front().flags |= TX_PENDING;

    a::RuntimeOptions runtimeOptions;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    a::ApiRuntime runtime(runtimeOptions);
    q::test::TcpPeerScript script;
    script.initialBytes = peer::fixturePeerPrefix(physical, c::SubscriptionFamily::HISTORY, false, true);
    const std::size_t initialCount = liveRemoval ? 2 : 1;
    const auto initial =
        peer::fixturePeerData(std::span(events).first(initialCount), c::SubscriptionFamily::HISTORY, true);
    script.initialBytes.insert(script.initialBytes.end(), initial.begin(), initial.end());
    script.replyBytes = peer::fixturePeerData(std::span(&reply, 1), c::SubscriptionFamily::HISTORY, true);
    script.readThroughMessage = std::to_underlying(q::protocol::SubscriptionMessage::HISTORY_ADD);
    script.waitForControllerReply = true;
    script.waitForClientClose = true;
    q::test::OwnedTcpPeer source({{std::move(script)}, 1024 * 1024, 25s});
    require(source.port() != 0, "Series transaction peer failed to listen");

    a::EndpointOptions options;
    options.contract = a::DeliveryContract::HISTORY;
    options.maximumAttempts = 1;
    options.readTimeout = 20000ms;
    const auto endpoint = runtime.createEndpoint(options);
    const auto subscription = endpoint->getFeed()->createSubscription<e::Series>();
    const auto values = std::make_shared<Observation>();
    subscription->addEventListener([values](auto batch) {
        values->append(batch);
    });
    subscription->setSelectors(std::array{a::SubscriptionSelector::plain(u"AAPL&Q")});
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));

    const auto observe = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + 3s;

        do {
            auto cut = subscription->getHistorySnapshot();

            if (cut.state == a::HistoryObservationState::CURRENT && cut.ranges.size() == 1 &&
                cut.ranges.front().status.source && predicate(cut.ranges.front().status)) {
                return cut;
            }

            std::this_thread::sleep_for(1ms);
        } while (std::chrono::steady_clock::now() < deadline);

        throw std::runtime_error("Series transaction HISTORY observation timeout");
    };

    const auto initialValues = values->snapshot(initialCount);
    require(initialValues.size() == initialCount &&
                initialValues.front().eventFlags == (liveRemoval ? SNAPSHOT_BEGIN : SNAPSHOT_BEGIN | TX_PENDING),
            "Public Series lost snapshot BEGIN flags");
    const auto beforeReply = observe([&](const auto &status) {
        return status.source->sourceTransactionPending == !liveRemoval &&
               status.source->snapshot ==
                   (liveRemoval ? HistorySnapshotState::COMPLETE : HistorySnapshotState::RECEIVING);
    });
    require(beforeReply.ranges.front().status.completeDeliveredRange == liveRemoval,
            "Public HISTORY misclassified initial range coverage");

    source.allowReply(0);
    const auto finished = values->snapshot(initialCount + 1);
    require(finished.size() == initialCount + 1 && finished.back().eventFlags == reply.flags &&
                finished.back().getIndex() == (liveRemoval ? -1 : *reply.removalTime),
            "Public Series lost the transaction boundary or its removal coordinate");
    const auto settled = observe([&](const auto &status) {
        return status.source->sourceTransactionPending == liveRemoval &&
               status.source->snapshot ==
                   (truncated ? HistorySnapshotState::TRUNCATED : HistorySnapshotState::COMPLETE);
    });
    const auto &status = settled.ranges.front().status;
    require(status.completeDeliveredRange == (!truncated && !liveRemoval) &&
                (!truncated || status.source->snipFrontier == reply.removalTime),
            "Public HISTORY confused truncation or transaction state with range coverage");
    require(beforeReply.ranges.front().status.source->sourceTransactionPending == !liveRemoval,
            "Later source progress mutated the retained observation");
    const auto closed = endpoint->closeAndAwaitTermination();
    require(!closed.deadlineExpired, "Series HISTORY close exceeded its drain deadline");
    require(subscription->getHistorySnapshot().state == a::HistoryObservationState::RETIRED,
            "Closed Series subscription retained current HISTORY coverage");
    require(!status.completeDeliveredRange || closed.drained, "Series HISTORY did not drain a covered range");
    runtime.closeAndAwaitTermination();
    const auto peerStatus = source.snapshot();
    require(peerStatus.error == q::test::TcpPeerError::NONE && peerStatus.sessions.front().matched,
            "Series transaction peer did not receive HISTORY ADD");
}
} // namespace

int main() {
    try {
        run();
        verifyTransactionBoundary(e::indexed_event_flags::SNAPSHOT_END);
        verifyTransactionBoundary(e::indexed_event_flags::SNAPSHOT_SNIP);
        verifyTransactionBoundary(e::indexed_event_flags::TX_PENDING);

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
