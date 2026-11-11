// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone public Feed consumer with finite owned peers and one explicitly held host task.
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;
constexpr q::core::SchemaLimits SCHEMA{42, 284, 724, 128, 16384};

/** @param value Required observed behavior. @param message Static diagnostic. */
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

/** @tparam Predicate Nonblocking observation. @param predicate Exact owned completion condition.
 * @return Whether the condition appeared within the finite test deadline.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 10s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** One host-owned task identity; all access is serialized independently of SDK progress. */
class HeldExecutor final : public q::api::CallbackExecutor {
    std::mutex mutex_;
    std::optional<q::api::CallbackTask> task_;

    public:
    bool reject = false;

    q::api::CallbackPostResult post(q::api::CallbackTask task) override {
        if (reject) {
            return q::api::CallbackPostResult::REJECTED;
        }

        std::lock_guard lock(mutex_);

        if (task_) {
            return q::api::CallbackPostResult::REJECTED;
        }

        task_.emplace(std::move(task));

        return q::api::CallbackPostResult::ACCEPTED;
    }

    bool ready() {
        std::lock_guard lock(mutex_);

        return task_.has_value();
    }

    q::api::CallbackTask take() {
        std::lock_guard lock(mutex_);
        auto task = std::move(*task_);
        task_.reset();

        return task;
    }
};

/** @param schema Exact source shape. @return Canonical scalar tuple, preserving full declared types. */
std::vector<q::core::FieldValue> sourceValues(const q::core::LocalSchema &schema, std::size_t record = 0) {
    std::vector<q::core::FieldValue> result;

    for (const auto &field : schema.records()[record].fields) {
        result.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
    }

    return result;
}

/** Set one real wire property without changing its full type.
 * @param schema Exact source declarations. @param values Complete owned tuple. @param key Actual property name.
 * @param raw Original declared scalar. @param record Exact local source position.
 */
void set(const q::core::LocalSchema &schema, std::vector<q::core::FieldValue> &values, std::u16string_view key,
         std::int64_t raw, std::size_t record = 0) {
    const auto found = std::ranges::find_if(schema.records()[record].lookups, [&](const auto &entry) {
        return entry.name == key;
    });
    require(found != schema.records()[record].lookups.end(), "Source property missing");
    values[found->fieldId].raw = raw;
}

/** @param value Positive test integer. @return Exact wide rank/significand source encoding. */
constexpr std::int64_t wide(std::int64_t value) {
    return value * 256 + 128;
}

/** Exercise SDK defaults, physical coalescing, five-output grouping and immutable selected executor ownership. */
void quote(bool throws = false, bool cancel = false, bool rejected = false) {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    auto subscription = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::ORDER});
    auto executor = std::make_shared<HeldExecutor>();
    executor->reject = rejected;
    subscription->setEventExecutor(executor);
    std::vector<q::api::RegisteredEvent> values;
    std::atomic<std::size_t> count{0};
    subscription->addEventListener([&](auto batch) {
        values.assign(batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);

        if (throws) {
            throw std::runtime_error("Controlled group listener failure");
        }
    });
    const auto bid =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::COMPOSITE_BID.indexedSource());
    const auto ask =
        q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::COMPOSITE_ASK.indexedSource());
    const auto both = q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::COMPOSITE.indexedSource());
    subscription->setSelectors(std::array{bid, ask, both});
    auto schema = q::events::defaultQuoteSchema(SCHEMA);
    require(bool(schema), "Quote source schema refused");
    auto fields = sourceValues(*schema);
    set(*schema, fields, u"BidPrice", wide(12));
    set(*schema, fields, u"BidSize", wide(3));
    set(*schema, fields, u"AskPrice", wide(14));
    set(*schema, fields, u"AskSize", wide(5));
    const std::array records{peer::Record{u"AAPL", std::move(fields)}};
    const std::array<std::u16string, 1> expected{u"AAPL"};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.canonicalSymbols = true;
    script.waitForDataRelease = true;
    script.afterRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, records, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return source.adds() == 1 || source.failed();
            }),
            "Coalesced Quote ADD timeout");
    require(!source.failed(), "Source delegates duplicated physical Quote ADD");
    source.releaseData();
    require(eventually([&] {
                return executor->ready() || subscription->getState() == q::api::SubscriptionState::FAILED;
            }),
            "Five-output host task timeout");

    if (rejected) {
        require(eventually([&] {
                    const auto s = subscription->getStatistics();

                    return subscription->getState() == q::api::SubscriptionState::FAILED && s.inFlightEvents == 0;
                }),
                "Rejected group did not retire");
        require(count.load() == 0, "Rejected group invoked listener");
        static_cast<void>(endpoint->closeAndAwaitTermination());
        const auto final = subscription->getStatistics();
        require(final.final && final.admittedEvents.value == 5 && final.discardedEvents.value == 5 &&
                    final.deliveredEvents.value == 0,
                "Rejected group conservation failed");

        return;
    }

    require(executor->ready(), "Five-output projection failed before host admission");
    auto selected = subscription->getStatistics();
    require(selected.inFlightEvents == 5 && selected.pendingEvents == 0,
            "Business projection count lost before executor");
    require(count.load() == 0, "Held task ran before host execution");

    if (cancel) {
        auto task = executor->take();
        static_cast<void>(endpoint->closeAndAwaitTermination());
        require(task.run() == q::api::CallbackTaskRunResult::CANCELLED, "Closed group task was not inert");
        const auto final = subscription->getStatistics();
        require(final.final && final.inFlightEvents == 0 && final.pendingEvents == 0 &&
                    final.admittedEvents.value == 5 && final.discardedEvents.value == 5 && count.load() == 0,
                "Cancelled group conservation failed");

        return;
    }

    subscription->removeSelector(bid);
    const auto contracted = subscription->getSnapshot().revision;
    require(eventually([&] {
                return subscription->getStatistics().activeDesiredRevision == contracted;
            }),
            "Selected group contraction mapping timeout");
    auto task = executor->take();
    require(task.run() == q::api::CallbackTaskRunResult::COMPLETED,
            "Selected owning group was cancelled by source contraction");

    if (throws) {
        require(eventually([&] {
                    const auto s = subscription->getStatistics();

                    return s.callbackFailedEvents.value == 5 && s.inFlightEvents == 0;
                }),
                "Throwing group did not retire");
        static_cast<void>(endpoint->closeAndAwaitTermination());
        const auto final = subscription->getStatistics();
        require(final.final && final.admittedEvents.value == 5 && final.callbackFailedEvents.value == 5 &&
                    final.deliveredEvents.value == 0 && final.callbackFailures.value == 1,
                "Throwing group conservation failed");

        return;
    }

    require(eventually([&] {
                const auto s = subscription->getStatistics();

                return s.deliveredEvents.value == 5 && s.inFlightEvents == 0;
            }),
            "Five-output retirement timeout");
    require(count.load(std::memory_order_acquire) == 5, "Selected group lost an original contribution");
    require(std::holds_alternative<q::events::Quote>(values[0]), "Native Quote order changed");
    constexpr std::array ids{1, 2, 7, 7};

    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto &order = std::get<q::events::Order>(values[i + 1]);
        require(order.getSourceId() == ids[i] && order.symbol == u"AAPL", "Derived Quote identity changed");
    }

    require(task.run() == q::api::CallbackTaskRunResult::CANCELLED, "Repeated task delivered twice");
    subscription->clearSymbols();
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Final Quote REMOVE timeout");
    require(!source.failed() && source.adds() == 1, "Local source mask changed physical demand");
    require(endpoint->closeAndAwaitTermination().drained, "Quote owners did not retire");
    const auto final = subscription->getStatistics();
    require(final.final && final.admittedEvents.value == 5 && final.deliveredEvents.value == 5,
            "Five-output final conservation failed");
}

/** Verify real A..Z physical demand with SDK-owned registries and both immutable plain defaults.
 * @param explicitUnitary Select source8 explicitly instead of using the plain default.
 * @param unitaryDefault Immutable endpoint source policy, independent of explicit source selection.
 */
void regional(bool explicitUnitary, bool unitaryDefault) {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.unitaryOrderSources = unitaryDefault;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Order>();
    std::mutex mutex;
    std::vector<q::events::Order> values;
    std::atomic<std::size_t> count{0};
    subscription->addEventListener([&](auto batch) {
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    });

    if (explicitUnitary) {
        subscription->setSelectors(std::array{
            q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::REGIONAL.indexedSource())});
    } else {
        subscription->addSymbols("AAPL");
    }

    auto original = q::events::defaultQuoteSchema(SCHEMA);
    require(bool(original), "Regional source base refused");
    std::vector<q::core::LocalRecord> records;
    std::vector<std::u16string> expected;
    std::vector<std::int32_t> outgoing;

    if (!explicitUnitary) {
        records.push_back(original->records()[0]);
        expected.push_back(u"AAPL");
        outgoing.push_back(0);
    }

    std::int32_t selected = 0;

    for (char16_t exchange = u'A'; exchange <= u'Z'; ++exchange) {
        q::core::LocalRecord record{
            static_cast<std::int32_t>(records.size()), u"Quote&" + std::u16string(1, exchange), false, {}, {}};

        for (std::size_t field = 0; field < original->records()[0].fields.size(); ++field) {
            if (field == 1 || field == 5) {
                continue;
            }

            const auto target = record.fields.size();
            record.fields.push_back(original->records()[0].fields[field]);

            for (const auto &lookup : original->records()[0].lookups) {
                if (lookup.fieldId != field) {
                    continue;
                }

                auto name = lookup.name;

                if (name.starts_with(u"Quote.")) {
                    name = record.name + name.substr(5);
                }

                record.lookups.push_back({std::move(name), target});
            }
        }

        if (exchange == u'Q') {
            selected = record.id;
        }

        records.push_back(std::move(record));
        expected.push_back(u"AAPL");
        outgoing.push_back(9 + exchange - u'A');
    }

    auto schema = q::core::LocalSchema::create(records, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Complete A..Z source schema refused");
    auto fields = sourceValues(*schema, selected);
    set(*schema, fields, u"BidPrice", wide(12), selected);
    set(*schema, fields, u"BidSize", wide(3), selected);
    set(*schema, fields, u"AskPrice", wide(14), selected);
    set(*schema, fields, u"AskSize", wide(5), selected);
    const std::array sourceRecords{peer::Record{u"AAPL", std::move(fields), 0, {}, selected}};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = outgoing;
    script.afterRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, sourceRecords, expected, 8192, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto delivered = eventually([&] {
        return count.load(std::memory_order_acquire) >= 2 || source.failed() ||
               subscription->getState() == q::api::SubscriptionState::FAILED;
    });

    if (!delivered) {
        const auto diag = subscription->getDiagnostic();
        const auto ed = endpoint->getDiagnostic();
        const auto stats = subscription->getStatistics();
        std::fprintf(stderr,
                     "regional explicit=%d unitary=%d count=%zu adds=%u peerfailed=%d substate=%d subdiag=%d/%.*s "
                     "endpointstate=%d endpointdiag=%d/%.*s selectors=%zu active=%zu admitted=%llu pending=%zu\n",
                     explicitUnitary, unitaryDefault, count.load(), source.adds(), source.failed(),
                     static_cast<int>(subscription->getState()), static_cast<int>(diag.code),
                     static_cast<int>(diag.stage.size()), diag.stage.data(), static_cast<int>(endpoint->getState()),
                     static_cast<int>(ed.code), static_cast<int>(ed.stage.size()), ed.stage.data(),
                     stats.desiredSelectors, stats.activeSelectors,
                     static_cast<unsigned long long>(stats.admittedEvents.value), stats.pendingEvents);
    }

    require(delivered, "SDK A..Z delivery timeout");
    require(!source.failed() && count.load() == 2, "SDK A..Z source demand/delivery changed");
    {
        std::lock_guard lock(mutex);
        require(values.size() == 2, "Regional group split/lost outputs");
        const bool pair = explicitUnitary || unitaryDefault;
        require(values[0].getSourceId() == (pair ? 8 : 3) && values[1].getSourceId() == (pair ? 8 : 4),
                "Immutable default overrode explicit regional source");
        require(values[0].getExchangeCode() == u'Q' && values[0].symbol == u"AAPL",
                "Regional Order physical coordinates changed");
    }
    subscription->clearSymbols();
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "A..Z final REMOVE timeout");
    require(!source.failed() && source.adds() == 1, "A..Z physical demand duplicated or omitted a record");
    require(endpoint->closeAndAwaitTermination().drained, "Regional owners did not retire");
    const auto final = subscription->getStatistics();
    require(final.final && final.admittedEvents.value == 2 && final.deliveredEvents.value == 2 &&
                final.pendingEvents == 0 && final.inFlightEvents == 0,
            "Regional weighted delivery conservation failed");
}

/** One regional native delegate and its derived family use distinct physical coordinates for one selector. */
void regionalCoordinates() {
    q::api::RuntimeOptions limits;
    limits.maximumSymbolsPerSubscription = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    auto original = q::events::defaultQuoteSchema(SCHEMA);
    require(bool(original), "Regional coordinate source base refused");
    q::core::LocalRecord record{0, u"Quote&Q", false, {}, {}};

    for (std::size_t field = 0; field < original->records()[0].fields.size(); ++field) {
        if (field == 1 || field == 5) {
            continue;
        }

        const auto target = record.fields.size();
        record.fields.push_back(original->records()[0].fields[field]);

        for (const auto &lookup : original->records()[0].lookups) {
            if (lookup.fieldId == field) {
                auto name = lookup.name;

                if (name.starts_with(u"Quote.")) {
                    name = record.name + name.substr(5);
                }

                record.lookups.push_back({std::move(name), target});
            }
        }
    }

    auto schema = q::core::LocalSchema::create(std::array{record}, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Regional coordinate schema refused");
    q::api::EventRegistration derivedRoute{q::api::EventType::ORDER, 0};
    derivedRoute.projection = q::events::EventProjection::QUOTE_ORDERS;
    const std::array registrations{q::api::EventRegistration{q::api::EventType::QUOTE, 0}, derivedRoute};
    q::api::EventRegistryWork work;
    work.preparation = 200000000;
    auto registry = q::api::EventRegistry::create(*schema, registrations, {}, work);
    require(bool(registry), "Regional coordinate registry refused");
    q::api::EndpointOptions options;
    options.eventRegistry = std::move(*registry);
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::ORDER});
    std::mutex mutex;
    std::vector<q::api::RegisteredEvent> values;
    std::atomic<std::size_t> count{0};
    subscription->addEventListener([&](auto batch) {
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    });
    subscription->setSelectors(
        std::array{q::api::SubscriptionSelector::indexed(u"AAPL&Q", q::events::OrderSource::REGIONAL.indexedSource())});
    auto native = sourceValues(*schema);
    auto derived = sourceValues(*schema);
    set(*schema, native, u"BidPrice", wide(12));
    set(*schema, native, u"AskPrice", wide(14));
    set(*schema, derived, u"BidPrice", wide(22));
    set(*schema, derived, u"AskPrice", wide(24));
    const std::array records{peer::Record{u"AAPL", std::move(native)}, peer::Record{u"AAPL&Q", std::move(derived)}};
    const std::array<std::u16string, 2> expected{u"AAPL", u"AAPL&Q"};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::TICKER;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = {0, 0};
    script.afterRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, records, expected, 8192, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return count.load(std::memory_order_acquire) >= 3 || source.failed() ||
                       subscription->getState() == q::api::SubscriptionState::FAILED;
            }),
            "Regional coordinate delivery timeout");
    const auto diagnostic = subscription->getDiagnostic();
    std::fprintf(stderr, "REGIONAL_COORDINATES count=%zu adds=%u peerfailed=%d state=%d diagnostic=%d/%.*s\n",
                 count.load(), source.adds(), source.failed(), static_cast<int>(subscription->getState()),
                 static_cast<int>(diagnostic.code), static_cast<int>(diagnostic.stage.size()), diagnostic.stage.data());
    require(!source.failed() && count.load() == 3, "Regional coordinate capacity or routing refused");
    {
        std::lock_guard lock(mutex);
        std::size_t quotes = 0;
        std::size_t orders = 0;
        bool bidSeen = false;
        bool askSeen = false;

        for (const auto &value : values) {
            if (const auto *quote = std::get_if<q::events::Quote>(&value)) {
                ++quotes;
                require(quote->symbol == u"AAPL&Q" && quote->getBidPrice() == 12,
                        "Native regional coordinate leaked literal source");
            } else {
                const auto &order = std::get<q::events::Order>(value);
                require(order.symbol == u"AAPL&Q" && order.getSourceId() == 8 &&
                            (order.getPrice() == 22 || order.getPrice() == 24),
                        "Derived literal coordinate leaked native source");

                if (order.getOrderSide() == q::events::Side::BUY) {
                    require(!bidSeen && order.getPrice() == 22, "Derived bid coordinate changed");
                    bidSeen = true;
                } else {
                    require(order.getOrderSide() == q::events::Side::SELL && !askSeen && order.getPrice() == 24,
                            "Derived ask coordinate changed");
                    askSeen = true;
                }

                ++orders;
            }
        }

        require(quotes == 1 && orders == 2 && bidSeen && askSeen, "Coordinate masks were conflated");
    }
    require(endpoint->closeAndAwaitTermination().drained, "Regional coordinate owners did not retire");
}

/** Exercise default HISTORY mapping of one raw MarketMaker plus four derived Order outputs. */
void maker() {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::HISTORY;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription =
        endpoint->getFeed()->createSubscription({q::api::EventType::ORDER, q::api::EventType::MARKET_MAKER});
    std::mutex mutex;
    std::vector<q::api::RegisteredEvent> values;
    std::atomic<std::size_t> count{0};
    std::atomic<bool> callbackFenced{false};
    subscription->addEventListener([&](auto batch) {
        const auto cut = subscription->getHistorySnapshot();
        callbackFenced.store(cut.state != q::api::HistoryObservationState::CURRENT &&
                                 std::ranges::none_of(cut.ranges,
                                                      [](const auto &range) {
                                                          return range.status.completeDeliveredRange;
                                                      }),
                             std::memory_order_release);
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    });
    subscription->setSelectors(
        std::array{q::api::SubscriptionSelector::plain(u"AAPL"),
                   q::api::SubscriptionSelector::indexed(u"AAPL", q::events::OrderSource::AGGREGATE.indexedSource())});
    auto order = q::events::defaultOrderSchema(SCHEMA);
    auto maker = q::events::defaultMarketMakerSchema(SCHEMA);
    require(order && maker, "Indexed source declarations refused");
    std::array sourceRecords{order->records()[0], maker->records()[0]};
    sourceRecords[1].id = 1;
    auto schema = q::core::LocalSchema::create(sourceRecords, SCHEMA, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Combined source schema refused");
    auto fields = sourceValues(*schema, 1);
    set(*schema, fields, u"MMExchange", u'Q', 1);
    set(*schema, fields, u"MMID", 0x41424344, 1);
    set(*schema, fields, u"MMBid.Price", wide(12), 1);
    set(*schema, fields, u"MMBid.Size", wide(3), 1);
    set(*schema, fields, u"MMBid.Count", wide(7), 1);
    set(*schema, fields, u"MMAsk.Price", wide(14), 1);
    set(*schema, fields, u"MMAsk.Size", wide(5), 1);
    set(*schema, fields, u"MMAsk.Count", wide(9), 1);
    const std::array records{peer::Record{u"AAPL", std::move(fields), 12, {}, 1}};
    const std::array<std::u16string, 2> expected{u"AAPL", u"AAPL"};
    peer::PeerScript script;
    script.family = q::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.describeAllRecords = true;
    script.canonicalSymbols = true;
    script.subscriptionRecordIds = {0, 1};
    script.afterRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, records, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return count.load(std::memory_order_acquire) >= 5 ||
                       subscription->getState() == q::api::SubscriptionState::FAILED || source.failed();
            }),
            "MarketMaker five-output timeout");
    require(!source.failed() && count.load() == 5, "MarketMaker physical/default mapping failed");
    {
        std::lock_guard lock(mutex);
        constexpr std::array ids{5, 6, 9, 9};

        for (std::size_t i = 0; i < 4; ++i) {
            const auto &value = std::get<q::events::Order>(values[i]);
            require(value.getSourceId() == ids[i] && value.marketMaker == u"ABCD", "Aggregate Order identity changed");
        }

        const auto &raw = std::get<q::events::MarketMaker>(values[4]);
        require(raw.getSourceId() == 0 && raw.getMarketMaker() == u"ABCD" && raw.getBidCount() == 7,
                "Native MarketMaker identity/count changed");
    }
    require(eventually([&] {
                const auto s = subscription->getStatistics();

                return s.deliveredEvents.value == 5 && s.inFlightEvents == 0;
            }),
            "MarketMaker retirement timeout");
    require(callbackFenced.load(std::memory_order_acquire), "MarketMaker callback getter asserted false completion");
    require(eventually([&] {
                return subscription->getHistorySnapshot().state == q::api::HistoryObservationState::CURRENT;
            }),
            "MarketMaker AS-OF mapping publication timeout");
    const auto historical = subscription->getHistorySnapshot();
    require(historical.ownerIdentity == subscription->getStatistics().ownerIdentity && historical.sourceEpoch &&
                historical.ranges.size() == 2,
            "MarketMaker owning history identity lost");

    for (const auto &range : historical.ranges) {
        require(range.symbol == u"AAPL" && range.status.requestedFrom == 0,
                "MarketMaker public history did not retain raw bound0");
    }

    subscription->clearSymbols();
    require(historical.ranges.size() == 2 && historical.logicalRevision != subscription->getSnapshot().revision,
            "Old MarketMaker owning cut mutated with new logical intent");
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "MarketMaker final REMOVE timeout");
    require(!source.failed(), "Aggregate/native overlap duplicated physical demand");
    require(endpoint->closeAndAwaitTermination().drained, "MarketMaker owners did not retire");
}
} // namespace

int main() {
    try {
        regionalCoordinates();
        quote();
        quote(true);
        quote(false, true);
        quote(false, false, true);
        regional(true, false);
        regional(false, false);
        regional(false, true);
        maker();

        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());

        return 1;
    }
}
