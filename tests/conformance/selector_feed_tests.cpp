// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Installed-consumer-compatible deterministic public selector/demand conformance, no SDK progress in the client.
#include "connect_peer.hpp"
#include "event_time_fixture.hpp"
#include "qd/api/feed.hpp"
#include "registered_event_test_support.hpp"
#include "summary_fixture.hpp"
#include "trade_eth_fixture.hpp"
#include "trade_fixture.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace peer = q::connect_tests;
namespace f = event_time_fixture;
using Selector = q::api::SubscriptionSelector;
using namespace std::chrono_literals;
/** Explicit local subscription cutoff scenarios, separate from ordinary endpoint drain. */
enum class CallbackFault {
    NONE,  ///< No aborted logical registration; ordinary drain stays strict.
    THROW, ///< Listener failure closes only its registration through existing local ABORT.
    CLOSE  ///< Listener explicitly closes its own logical registration, with other subscribers live.
};

/** @param condition Required public observation.
 * @param message Static failure description retained by the bounded process runner.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Poll only test-owned observations, never drive SDK progress or infer membership from elapsed time.
 * @tparam Predicate Nonblocking causal completion/failure observation.
 * @param predicate Borrowed observation.
 * @return True on observation within five seconds, false on its finite deadline.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Complete independent owning callback observations, including the original business fields. */
struct Observation {
    std::mutex mutex;
    std::vector<q::api::RegisteredEvent> values;
    std::atomic<std::size_t> count{0};

    /** @param batch Callback-only views copied before return. */
    void append(std::span<const q::api::RegisteredEvent> batch) {
        std::lock_guard lock(mutex);
        values.insert(values.end(), batch.begin(), batch.end());
        count.store(values.size(), std::memory_order_release);
    }
};

/** @param scenario Actual production Quote/regional/Profile declarations and field values.
 * @return Canonical source data, including unrequested symbols, concrete star, attributes and route negatives.
 */
std::vector<peer::Record> records(const f::Scenario &scenario) {
    const std::array<std::pair<std::int32_t, std::u16string_view>, 14> tuples{{{0, u"AAPL"},
                                                                               {1, u"AAPL"},
                                                                               {2, u"IBM"},
                                                                               {3, u"MSFT"},
                                                                               {0, u"*"},
                                                                               {1, u"*"},
                                                                               {3, u"*"},
                                                                               {0, u"MSFT"},
                                                                               {0, u"AB*"},
                                                                               {0, u"A?"},
                                                                               {1, u"MSFT"},
                                                                               {3, u"AAPL&Q"},
                                                                               {0, u"PROBE"},
                                                                               {0, u"MARKER"}}};
    std::vector<peer::Record> result;

    for (const auto &[id, symbol] : tuples) {
        result.push_back(
            {std::u16string(symbol), scenario.samples.at({static_cast<std::size_t>(id), 0}).values, 0, {}, id});
    }

    return result;
}

/** @param operation Atomic public operation expected to reject.
 * @param code Exact public category.
 * @tparam Operation Borrowed callable; exceptions are inspected, not ignored.
 */
template <class Operation> void rejects(Operation operation, q::api::ErrorCode code) {
    try {
        operation();
    } catch (const q::api::Error &error) {
        require(error.diagnostic().code == code, "Unexpected public selector refusal category");

        return;
    }
    throw std::runtime_error("Invalid selector batch was accepted");
}

/** @param scenario Actual immutable production subset.
 * @param reconnect Explicit two-session public reconnect instead of the mutation/overlap scenario.
 * @param wrongMarker One exact followup ADD negative; no weakened set/count/order matching.
 * @param fault Explicit local callback ABORT scenario, separate from strict ordinary drain.
 */
void check(const f::Scenario &scenario, bool reconnect, bool wrongMarker, CallbackFault fault) {
    auto schema = q::core::LocalSchema::create(scenario.records, {16, 128, 256, 128, 16384},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Production selector fixture schema refused");
    const std::array registrations{q::api::EventRegistration{q::api::EventType::QUOTE, 0},
                                   q::api::EventRegistration{q::api::EventType::QUOTE, 1},
                                   q::api::EventRegistration{q::api::EventType::QUOTE, 2},
                                   q::api::EventRegistration{q::api::EventType::PROFILE, 3}};
    q::api::EventRegistryWork registryWork;
    auto registry = q::api::EventRegistry::create(*schema, registrations, {}, registryWork);
    require(bool(registry), "Production selector registry refused");
    q::api::RuntimeOptions limits;
    limits.maximumSubscriptionsPerEndpoint = 3;
    limits.maximumSymbolsPerSubscription = 4;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::STREAM;
    options.enableWildcard = true;
    options.maximumAttempts = reconnect ? 2 : 1;
    options.retryDelay = 1ms;
    options.readTimeout = 5000ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    std::shared_ptr<q::api::Subscription<q::events::Quote>> typed;
    std::shared_ptr<q::api::Subscription<q::events::Quote>> throwing;
    auto typedCount = std::make_shared<std::atomic<std::size_t>>(0);

    if (!reconnect) {
        typed = feed->createSubscription<q::events::Quote>();
        typed->addEventListener([typedCount](auto batch) {
            for (const auto &quote : batch) {
                require(quote.symbol == u"AAPL&Q" || quote.symbol == u"PROBE", "Wrong exact typed membership");
            }

            typedCount->fetch_add(batch.size(), std::memory_order_release);
        });
        const std::array initial{Selector::indexed(u"AAPL&Q", {5133398, u"name is ignored"}),
                                 Selector::plain(u"PROBE")};
        typed->setSelectors(initial);

        if (fault != CallbackFault::NONE) {
            throwing = feed->createSubscription<q::events::Quote>();
            const std::weak_ptr<q::api::Subscription<q::events::Quote>> weak = throwing;
            throwing->addEventListener([weak, fault](auto) {
                if (fault == CallbackFault::THROW) {
                    throw std::runtime_error("owned selector listener failure");
                }

                if (const auto owner = weak.lock()) {
                    owner->close();
                }
            });
            const std::array same{Selector::indexed(u"AAPL&Q", {0, std::nullopt})};
            throwing->setSelectors(same);
        }
    }

    auto mixed = feed->createSubscription({q::api::EventType::QUOTE, q::api::EventType::PROFILE});
    auto observed = std::make_shared<Observation>();
    mixed->addEventListener([observed](auto batch) {
        observed->append(batch);
    });
    const auto all = Selector::all();
    const auto plain = Selector::plain(u"AAPL&Q");
    const auto indexed = Selector::indexed(u"AAPL&Q", {-123, std::nullopt});
    const std::array initial{all, plain, indexed};

    if (reconnect) {
        mixed->setSelectors(std::span(&all, 1));
    } else {
        mixed->setSelectors(initial);
    }

    const std::array badTime{Selector::plain(u"MUTATION-LEAK"), Selector::timeSeries(u"AAPL", 0)};
    rejects(
        [&] {
            mixed->setSelectors(badTime);
        },
        q::api::ErrorCode::UNSUPPORTED_PROFILE);
    const std::array badRegion{Selector::plain(u"MUTATION-LEAK"), Selector::indexed(u"AAPL&N&Q", {0, std::nullopt})};
    rejects(
        [&] {
            mixed->setSelectors(badRegion);
        },
        q::api::ErrorCode::INVALID_ARGUMENT);
    const auto literalStar = Selector::plain(u"*");
    rejects(
        [&] {
            mixed->addSelectors(std::span(&literalStar, 1));
        },
        q::api::ErrorCode::INVALID_ARGUMENT);
    const std::array tooMany{all, all, all, all, all};
    rejects(
        [&] {
            mixed->addSelectors(tooMany);
        },
        q::api::ErrorCode::RESOURCE_LIMIT);

    peer::PeerScript script;
    script.canonicalSymbols = script.describeAllRecords = true;
    script.afterFinalRemove = std::vector<peer::Record>{};
    const auto data = records(scenario);
    std::vector<std::u16string> expected;

    if (reconnect) {
        expected = {u"*", u"*", u"*", u"*"};
        script.subscriptionRecordIds = {0, 1, 2, 3};
        script.afterReconnect = data;
        script.removeSequence = std::vector{peer::ExpectedSubscription{expected, {0, 1, 2, 3}, true}};
    } else {
        expected = {u"PROBE", u"AAPL", u"*", u"*", u"*", u"*", u"AAPL&Q"};
        script.subscriptionRecordIds = {0, 1, 0, 1, 2, 3, 3};
        script.followupAdd = peer::ExpectedSubscription{{u"MARKER", u"MARKER"}, {0, 3}};
        script.afterFollowupAdd = data;
        script.afterRemove = data;
        script.removeSequence = std::vector{peer::ExpectedSubscription{{u"*", u"*", u"*", u"*"}, {0, 1, 2, 3}, true},
                                            peer::ExpectedSubscription{{u"AAPL&Q"}, {3}},
                                            peer::ExpectedSubscription{{u"PROBE", u"AAPL"}, {0, 1}, true},
                                            peer::ExpectedSubscription{{u"MARKER", u"MARKER"}, {0, 3}, true}};
    }

    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observed->count.load() >= 14 || source.failed();
            }),
            "Initial selector delivery timed out");
    require(!source.failed() && observed->count.load() == 14, "Wildcard mixed selection duplicated or lost a record");

    if (reconnect) {
        endpoint->reconnect();
        require(eventually([&] {
                    return observed->count.load() >= 28 || source.failed();
                }),
                "Explicit All reconnect replay timed out");
        require(!source.failed() && source.adds() == 2 && observed->count.load() == 28,
                "Fresh All replay or owned delivery differed");
        mixed->clearSymbols();
        require(eventually([&] {
                    return source.removes() == 1 || source.failed();
                }),
                "Final wildcard REMOVE timed out");
    } else {
        require(typedCount->load() == 2, "Source-specific current-market subscription did not bind real regional base");

        if (fault == CallbackFault::THROW) {
            require(eventually([&] {
                        return throwing->getDiagnostic().code == q::api::ErrorCode::CALLBACK_FAILURE;
                    }),
                    "Throwing selector subscriber was not isolated");
        }

        const std::array followup{all, indexed, Selector::plain(wrongMarker ? u"WRONG" : u"MARKER")};
        mixed->setSelectors(followup);

        if (wrongMarker) {
            require(eventually([&] {
                        return source.failed();
                    }),
                    "Wrong followup tuple was not rejected by strict peer");
            static_cast<void>(endpoint->closeAndAwaitTermination());
            runtime.closeAndAwaitTermination();

            return;
        }

        require(eventually([&] {
                    return observed->count.load() >= 28 || source.failed();
                }),
                "Causal followup ADD response timed out");
        require(!source.failed() && source.adds() == 2 && source.removes() == 0,
                "Removing plain prematurely withdrew overlapping indexed demand");
        require(typedCount->load() == 4 && observed->count.load() == 28,
                "All/Exact overlap delivered duplicates or leaked mutation batch");
        mixed->removeSelector(all);
        require(eventually([&] {
                    return (typedCount->load() >= 6 && observed->count.load() >= 31) || source.failed();
                }),
                "Post-All-remove PROBE timed out");
        require(!source.failed() && source.removes() == 1 && observed->count.load() == 31,
                "All removal lost IES demand or leaked unrequested routes");
        mixed->removeSelector(indexed);
        require(eventually([&] {
                    return (typedCount->load() >= 8 && observed->count.load() >= 32) || source.failed();
                }),
                "Post-IES-remove PROBE timed out");
        require(!source.failed() && source.removes() == 2 && observed->count.load() == 32,
                "Typed identity removal did not isolate surviving demand");
        typed->clearSymbols();
        require(eventually([&] {
                    return observed->count.load() >= 33 || source.failed();
                }),
                "Surviving MARKER proof timed out");
        require(!source.failed() && source.removes() == 3 && typedCount->load() == 8,
                "Late removed typed values leaked");
        require(endpoint->getState() == q::api::EndpointState::CONNECTED &&
                    endpoint->getDiagnostic().code == q::api::ErrorCode::NONE,
                "Throw/remove affected unrelated live endpoint");
        mixed->clearSymbols();
        require(eventually([&] {
                    return source.removes() == 4 || source.failed();
                }),
                "Final selector multiset REMOVE timed out");
    }

    require(endpoint->getState() == q::api::EndpointState::CONNECTED &&
                endpoint->getDiagnostic().code == q::api::ErrorCode::NONE,
            "Live causal frontier is not healthy before explicit endpoint cutoff");

    if (fault == CallbackFault::CLOSE) {
        require(throwing->getState() == q::api::SubscriptionState::CLOSED &&
                    throwing->getDiagnostic().code == q::api::ErrorCode::NONE,
                "Callback close did not retire only its own healthy subscription");
    }

    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!source.failed() && !closed.deadlineExpired, "Selector peer/retirement failed");
    std::cerr << "CLOSE reconnect=" << reconnect << " wrongMarker=" << wrongMarker
              << " callbackFault=" << static_cast<int>(fault) << " drained=" << closed.drained
              << " gap=" << closed.hadGap << " expiry=" << closed.deadlineExpired
              << " code=" << static_cast<int>(closed.diagnostic.code) << " stage=" << closed.diagnostic.stage
              << " native=" << closed.diagnostic.nativeCode << " adds=" << source.adds()
              << " removes=" << source.removes() << " retained=" << observed->count.load()
              << " typed=" << typedCount->load() << '\n';

    if (fault == CallbackFault::NONE) {
        require(reconnect ? closed.hadGap && !closed.drained : closed.drained,
                "Ordinary drain and explicit reconnect gap were conflated");
    } else {
        require(!closed.drained && !closed.hadGap,
                "Explicit callback local ABORT history was lost or invented a data gap");
    }

    bool expectedCutoffDiagnostic = closed.diagnostic.code == q::api::ErrorCode::NONE;
#ifdef _WIN32
    // The live frontier above precedes the explicit owner cutoff. Preserve the separately observed
    // known native read-cancellation result without accepting an arbitrary transport failure.
    expectedCutoffDiagnostic =
        expectedCutoffDiagnostic || (closed.diagnostic.code == q::api::ErrorCode::TRANSPORT_FAILURE &&
                                     closed.diagnostic.stage == "socket-read" && closed.diagnostic.nativeCode == 1236);
#else
    // In HOLD the peer closes only after the client's explicit cutoff/EOF, not a scripted server end.
    expectedCutoffDiagnostic = expectedCutoffDiagnostic ||
                               (closed.diagnostic.code == q::api::ErrorCode::TRANSPORT_FAILURE &&
                                closed.diagnostic.stage == "recovery-peer-end" && closed.diagnostic.nativeCode == 0);
#endif
    require(expectedCutoffDiagnostic, "Unexpected final diagnostic after explicit owner cutoff");
    require(mixed->getDiagnostic().code == q::api::ErrorCode::NONE, "Healthy selector subscription failed");
    std::lock_guard lock(observed->mutex);
    std::map<std::u16string, std::size_t> symbols;

    for (const auto &value : observed->values) {
        ++symbols[std::u16string{q::feed_tests::registeredEventSymbol(value)}];
    }

    require(symbols.contains(u"*") && symbols.contains(u"*&Q") && symbols.contains(u"AB*") && symbols.contains(u"A?"),
            "Concrete star/non-glob canonical DATA identity was lost");
    require(observed->values.size() == (reconnect ? 28 : 33),
            "Retained owning selector values changed after native retirement");
}

/** Deliver every current business kind through one actual eight-binding explicit All subscription.
 * @param summary Actual corrected-reference Summary production subset.
 * @param trade Actual ordinary Trade production subset.
 * @param eth Actual TradeETH production subset.
 */
void allFive(const f::Scenario &summary, const f::Scenario &trade, const f::Scenario &eth) {
    auto declarations = summary.records;

    for (auto record : eth.records) {
        record.id += 2;
        declarations.push_back(std::move(record));
    }

    for (auto record : trade.records) {
        record.id += 4;
        declarations.push_back(std::move(record));
    }

    auto quote = q::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto profile = q::events::defaultProfileSchema({16, 128, 256, 128, 16384});
    require(bool(quote) && bool(profile), "Current Quote/Profile schemas refused");
    declarations.push_back(quote->records()[0]);
    declarations.back().id = 6;
    declarations.push_back(profile->records()[0]);
    declarations.back().id = 7;
    auto schema =
        q::core::LocalSchema::create(declarations, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Five-kind All schema exceeded existing caps");
    const std::array routes{q::api::EventRegistration{q::api::EventType::SUMMARY, 0},
                            q::api::EventRegistration{q::api::EventType::SUMMARY, 1},
                            q::api::EventRegistration{q::api::EventType::TRADE_ETH, 2},
                            q::api::EventRegistration{q::api::EventType::TRADE_ETH, 3},
                            q::api::EventRegistration{q::api::EventType::TRADE, 4},
                            q::api::EventRegistration{q::api::EventType::TRADE, 5},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 6},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 7}};
    q::api::EventRegistryWork registryWork;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, registryWork);
    require(bool(registry), "Five-kind All registry refused");
    q::api::RuntimeOptions limits;
    limits.maximumSubscriptionsPerEndpoint = 1;
    limits.maximumSymbolsPerSubscription = 1;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::STREAM;
    options.enableWildcard = true;
    options.maximumAttempts = 1;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto mixed = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::PROFILE,
                                                          q::api::EventType::TRADE, q::api::EventType::TRADE_ETH,
                                                          q::api::EventType::SUMMARY});
    auto observed = std::make_shared<Observation>();
    mixed->addEventListener([observed](auto batch) {
        observed->append(batch);
    });
    const auto all = Selector::all();
    mixed->setSelectors(std::span(&all, 1));
    std::vector<peer::Record> data;

    for (std::int32_t id = 0; id < 6; ++id) {
        const auto &scenario = id < 2 ? summary : id < 4 ? eth : trade;
        const auto &sample = scenario.samples.at({static_cast<std::size_t>(id % 2), 0});
        data.push_back({sample.symbol, sample.values, 0, {}, id, q::protocol::FieldEventTime{0, 0, 0}});
    }

    for (std::int32_t id = 6; id < 8; ++id) {
        std::vector<q::core::FieldValue> fields;

        for (const auto &field : declarations[static_cast<std::size_t>(id)].fields) {
            fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
        }

        data.push_back({u"AAPL", std::move(fields), 0, {}, id, q::protocol::FieldEventTime{0, 0, 0}});
    }

    const std::array<std::u16string, 8> expected{u"*", u"*", u"*", u"*", u"*", u"*", u"*", u"*"};
    peer::PeerScript script;
    script.canonicalSymbols = script.describeAllRecords = script.fieldMetadata = true;
    script.subscriptionRecordIds = {6, 7, 4, 5, 2, 3, 0, 1};
    script.removeSequence =
        std::vector{peer::ExpectedSubscription{{expected.begin(), expected.end()}, script.subscriptionRecordIds, true}};
    script.afterFinalRemove = std::vector<peer::Record>{};
    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observed->count.load() >= 8 || source.failed();
            }),
            "Five-kind All delivery timed out");
    require(!source.failed() && observed->count.load() == 8, "Five-kind All lost/duplicated a source record");
    require(endpoint->getState() == q::api::EndpointState::CONNECTED &&
                endpoint->getDiagnostic().code == q::api::ErrorCode::NONE,
            "Five-kind live endpoint failed");
    mixed->clearSymbols();
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Five-kind All removal timed out");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!source.failed() && closed.drained && !closed.hadGap && !closed.deadlineExpired,
            "Five-kind ordinary All drain failed");
    std::array<std::size_t, 5> counts{};
    std::lock_guard lock(observed->mutex);

    for (const auto &value : observed->values) {
        ++counts[value.index()];

        if (const auto *event = std::get_if<q::events::Summary>(&value)) {
            const auto id = event->symbol == u"AAPL&Q" ? 1U : 0U;
            auto expectedValue = summary.samples.at({id, 0}).expected;
            expectedValue[1] = "0";
            require(summary_fixture::matches(*event, expectedValue),
                    "All Summary business fields differ from production reference");
        } else if (const auto *event = std::get_if<q::events::Trade>(&value)) {
            const auto id = event->symbol == u"AAPL&Q" ? 1U : 0U;
            auto expectedValue = trade.samples.at({id, 0}).expected;
            expectedValue[1] = "0";
            require(trade_fixture::matches(*event, expectedValue),
                    "All Trade business fields differ from production reference");
        } else if (const auto *event = std::get_if<q::events::TradeETH>(&value)) {
            const auto id = event->symbol == u"AAPL&Q" ? 1U : 0U;
            auto expectedValue = eth.samples.at({id, 0}).expected;
            expectedValue[1] = "0";
            require(trade_eth_fixture::matches(*event, expectedValue),
                    "All TradeETH business fields differ from production reference");
        }
    }

    require(counts == std::array<std::size_t, 5>{1, 1, 2, 2, 2}, "All business type identity/membership differs");
}

/** Exercise disabled permission and non-STREAM refusal without starting any native work. */
void disabled() {
    q::api::RuntimeOptions limits;
    limits.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    auto subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
    const auto all = Selector::all();
    rejects(
        [&] {
            subscription->setSelectors(std::span(&all, 1));
        },
        q::api::ErrorCode::UNSUPPORTED_PROFILE);
    q::api::EndpointOptions invalid;
    invalid.enableWildcard = true;
    rejects(
        [&] {
            static_cast<void>(runtime.createEndpoint(invalid));
        },
        q::api::ErrorCode::UNSUPPORTED_PROFILE);
    runtime.closeAndAwaitTermination();
}
} // namespace

/** @param argc Four explicit production corpus paths are required.
 * @param argv Explicit production schema/value corpus; no source-tree paths are compiled into this consumer.
 * @return Zero only for all public demand/negative/reconnect/ownership cases; nonzero on any bounded failure.
 */
int main(int argc, char **argv) {
    if (argc != 5) {
        return 2;
    }

    try {
        disabled();
        const auto corpus = f::read(argv[1]);
        const auto summaries = summary_fixture::load(argv[2]);
        const auto trades = trade_fixture::load(argv[3]);
        const auto eths = trade_eth_fixture::load(argv[4]);
        allFive(summaries.at("default120"), trades.at("default120"), eths.at("default120"));
        check(corpus.at("default"), false, false, CallbackFault::NONE);
        check(corpus.at("default"), false, false, CallbackFault::THROW);
        check(corpus.at("default"), false, false, CallbackFault::CLOSE);
        check(corpus.at("default"), true, false, CallbackFault::NONE);
        check(corpus.at("default"), false, true, CallbackFault::NONE);
        std::cout << "Selector Feed: disabled/atomic/mixed/IES/regional/All/overlap/remove/throw/reconnect/strict-peer "
                     "PASS\n";

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
