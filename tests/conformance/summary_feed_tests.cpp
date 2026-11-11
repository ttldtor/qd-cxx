// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone consumer: public installed headers and explicit deterministic test inputs only.
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/client/subscription_receiver.hpp"
#include "registered_event_test_support.hpp"
#include "summary_fixture.hpp"
#include "trade_eth_fixture.hpp"
#include "trade_fixture.hpp"
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace f = summary_fixture;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** @param value Required observable condition.
 * @param message Failure retained by the outer bounded process runner.
 */
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

/** Wait finitely without controlling any process or socket owned by someone else.
 * @tparam Predicate Nonblocking observation.
 * @param predicate Completion, including observable fault conditions.
 * @return False after five seconds, otherwise true.
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

/** @param input ASCII-only symbol from this corpus.
 * @return Explicit public UTF-8 argument, preserving every byte.
 */
std::string ascii(std::u16string_view input) {
    std::string result;
    result.reserve(input.size());

    for (const auto unit : input) {
        require(unit <= 0x7f, "Summary corpus public symbol must be ASCII");
        result.push_back(static_cast<char>(unit));
    }

    return result;
}

/** Thread-safe owning observations; no callback spans are retained. */
template <class Event> struct Observation {
    std::mutex mutex;
    std::vector<Event> events;
    std::atomic<std::size_t> count{0};

    /** @param batch Callback-only values; copied into independent owned storage before return. */
    void append(std::span<const Event> batch) {
        std::lock_guard lock(mutex);
        events.insert(events.end(), batch.begin(), batch.end());
        count.store(events.size(), std::memory_order_release);
    }
};

/** Copy one actual cursor observation into the existing deterministic wire-peer input.
 * @param source Complete source values and original FIELD time words, alive during the call.
 * @param localId Exact local record identity used by this peer schema.
 * @return Independently owned field values with the original two FIELD time words.
 */
peer::Record wireRecord(const event_time_fixture::Sample &source, std::int32_t localId) {
    const auto packed = static_cast<std::uint64_t>(source.rawEventTime);

    return {source.symbol,
            source.values,
            0,
            {},
            localId,
            q::protocol::FieldEventTime{std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> 32)),
                                        std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packed)),
                                        static_cast<std::uint64_t>(localId)}};
}

/** Deliver exact composite/regional physical Summary records through installed public typed and mixed subscriptions.
 * @param corpus Owned production observations.
 * @param regular Actual ordinary Trade schema and independently observed values for interop.
 * @param eth Actual extended-hours Trade schema and independently observed values for interop.
 * @param name Numeric/raw/default profile name.
 * @param sample Captured boundary sample ID.
 * @param ticker Select actual TICKER, otherwise STREAM.
 * @param reconnect Explicit public reconnect with two finite peer sessions.
 * @param callbackClose Close the typed subscription from its callback after both Summary values arrive.
 */
void check(const std::map<std::string, f::Scenario> &corpus, const trade_fixture::Scenario &regular,
           const trade_eth_fixture::Scenario &eth, const std::string &name, std::size_t sample, bool ticker,
           bool reconnect, bool callbackClose) {
    const auto &scenario = corpus.at(name);
    auto records = scenario.records;

    for (auto record : eth.records) {
        record.id += 2;
        records.push_back(std::move(record));
    }

    for (auto record : regular.records) {
        record.id += 4;
        records.push_back(std::move(record));
    }

    auto quote = q::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto profile = q::events::defaultProfileSchema({16, 128, 256, 128, 16384});
    require(bool(quote) && bool(profile), "Default compatible Quote/Profile refused");
    records.push_back(quote->records()[0]);
    records.back().id = 6;
    records.push_back(profile->records()[0]);
    records.back().id = 7;
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Actual composite/regional Summary schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::SUMMARY, 0},
                            q::api::EventRegistration{q::api::EventType::SUMMARY, 1},
                            q::api::EventRegistration{q::api::EventType::TRADE_ETH, 2},
                            q::api::EventRegistration{q::api::EventType::TRADE_ETH, 3},
                            q::api::EventRegistration{q::api::EventType::TRADE, 4},
                            q::api::EventRegistration{q::api::EventType::TRADE, 5},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 6},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 7}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Complete default finite Summary registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 33;
    runtimeOptions.maximumSymbolsPerSubscription = 2;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = ticker ? q::api::DeliveryContract::TICKER : q::api::DeliveryContract::STREAM;
    options.maximumAttempts = reconnect ? 2 : 1;
    options.readTimeout = 5000ms;
    options.drainTimeout = 1000ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    const auto &base = scenario.samples.at({0, sample});
    const auto &regional = scenario.samples.at({1, sample});
    const auto logical = f::units(regional.expected[0]);
    const auto finalSample = sample;
    const auto &finalBase = scenario.samples.at({0, finalSample});
    const auto &finalRegional = scenario.samples.at({1, finalSample});
    const auto &regularValue = regular.samples.at({1, name == "symbols120" ? sample : 7});
    require(trade_fixture::units(regularValue.expected[0]) == logical, "Interop Trade logical symbol differs");
    const auto &ethValue = eth.samples.at({1, name == "symbols120" ? sample : 7});
    require(trade_eth_fixture::units(ethValue.expected[0]) == logical, "Interop ETH logical symbol differs");
    auto typed = feed->createSubscription<q::events::Summary>();
    auto typedValues = std::make_shared<Observation<q::events::Summary>>();
    auto weakTyped = std::weak_ptr(typed);
    auto callbackClosed = std::make_shared<std::atomic<bool>>(false);
    typed->addEventListener([typedValues, weakTyped, callbackClosed, callbackClose](auto batch) {
        typedValues->append(batch);

        if (callbackClose && typedValues->count.load(std::memory_order_acquire) >= 2) {
            if (auto value = weakTyped.lock()) {
                value->close();
            }

            callbackClosed->store(true, std::memory_order_release);
        }
    });
    bool refused = false;
    try {
        typed->addSymbols({"NOTREQUESTED", "AAPL&N&Q"});
    } catch (const q::api::Error &error) {
        refused = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT;
    }
    require(refused, "Noninvertible Summary selector was not synchronously refused");
    typed->addSymbols({ascii(base.symbol), ascii(logical)});
    std::shared_ptr<q::api::Subscription<q::events::Summary>> throwing;

    if (reconnect) {
        throwing = feed->createSubscription<q::events::Summary>();
        throwing->addEventListener([](auto) {
            throw std::runtime_error("deliberate Summary listener fault");
        });
        throwing->addSymbols({ascii(logical)});
    }

    auto regularExpected = regularValue.expected;
    auto ethExpected = ethValue.expected;

    if (name == "no-metadata120") {
        // FIELD presence is frame-wide; unrelated business fields retain their production values.
        regularExpected[1] = ethExpected[1] = "0";
    }

    const std::u16string marker = u"ZZZ146&Q";
    auto probes = std::make_shared<Observation<q::events::Summary>>();
    std::array<std::shared_ptr<q::api::Subscription<q::api::RegisteredEvent>>, 31> mixed;
    std::array<std::shared_ptr<Observation<q::api::RegisteredEvent>>, 31> values;

    for (unsigned mask = 1; mask <= 31; ++mask) {
        std::vector<q::api::EventType> kinds;

        for (unsigned kind = 0; kind < 5; ++kind) {
            if (mask & (1U << kind)) {
                kinds.push_back(static_cast<q::api::EventType>(kind));
            }
        }

        mixed[mask - 1] = feed->createSubscription(kinds);
        values[mask - 1] = std::make_shared<Observation<q::api::RegisteredEvent>>();
        mixed[mask - 1]->addEventListener([observed = values[mask - 1], probes, marker](auto batch) {
            for (const auto &event : batch) {
                const auto *trade = std::get_if<q::events::Summary>(&event);

                if (trade && trade->symbol == marker) {
                    probes->append({trade, 1});
                } else {
                    observed->append({&event, 1});
                }
            }
        });

        // The final atomic replacement contributes distinct no-data wire tuples. ACTIVE alone
        // does not prove backend membership; their ADD gates the peer's single data batch.
        if (mask == 31) {
            mixed[mask - 1]->addSymbols({ascii(logical), ascii(marker)});
        } else {
            mixed[mask - 1]->addSymbols({ascii(logical)});
        }
    }

    std::vector<peer::Record> data;

    for (std::int32_t id = 0; id < 2; ++id) {
        data.push_back(wireRecord(scenario.samples.at({static_cast<std::size_t>(id), sample}), id));
    }

    data.push_back(wireRecord(regularValue, 5));
    data.push_back(wireRecord(ethValue, 3));

    for (std::int32_t id = 6; id < 8; ++id) {
        std::vector<q::core::FieldValue> fields;

        for (const auto &field : records[id].fields) {
            fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});
        }

        data.push_back({logical, std::move(fields), 0, {}, id, q::protocol::FieldEventTime{0, 0, 0}});
    }

    data.push_back(
        {logical, base.values, 0, {}, 0, q::protocol::FieldEventTime{0, 0, 0}}); // Wrong composite tuple must not
    // leak into &Q.
    data.push_back(
        {u"MSFT", regional.values, 0, {}, 1, q::protocol::FieldEventTime{0, 0, 0}}); // Unsubscribed physical symbol.
    const auto &regularComposite = regular.samples.at({0, name == "symbols120" ? sample : 7});
    data.push_back({logical, regularComposite.values, 0, {}, 4, q::protocol::FieldEventTime{0, 0, 0}});
    // The regular composite record cannot satisfy the selected regular regional route.
    const auto &ethComposite = eth.samples.at({0, name == "symbols120" ? sample : 7});
    data.push_back({logical, ethComposite.values, 0, {}, 2, q::protocol::FieldEventTime{0, 0, 0}});
    const std::array<std::u16string, 11> expected{base.symbol, base.symbol, logical,  logical,
                                                  base.symbol, base.symbol, marker,   marker,
                                                  u"ZZZ146",   u"ZZZ146",   u"ZZZ146"};
    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = script.fieldMetadata = true;
    script.subscriptionRecordIds = {0, 1, 6, 7, 5, 3, 6, 7, 5, 3, 1};
    // A live marker follows the withdrawn composite in the same frame. Its callback proves
    // the late tuple has passed the receiver before the remaining subscriptions are withdrawn.
    auto probe = wireRecord(finalRegional, 1);
    probe.symbol = u"ZZZ146";
    script.afterRemove = callbackClose ? std::vector<peer::Record>{data[0], data[1], data[2],         data[3],
                                                                   data[4], data[5], std::move(probe)}
                                       : std::vector<peer::Record>{data[0], std::move(probe)};
    script.afterFinalRemove = std::vector<peer::Record>{};
    script.removeSequence = std::vector<peer::ExpectedSubscription>{
        {{base.symbol}, {0}},
        {{base.symbol, logical, logical, base.symbol, base.symbol, marker, marker, u"ZZZ146", u"ZZZ146", u"ZZZ146"},
         {1, 6, 7, 5, 3, 6, 7, 5, 3, 1},
         true}};

    if (reconnect) {
        auto replay = data;
        replay[0] = wireRecord(finalBase, 0);
        replay[1] = wireRecord(finalRegional, 1);
        script.afterReconnect = std::move(replay);
    }

    if (name == "no-metadata120") {
        script.fieldMetadata = false;

        for (auto &record : data) {
            record.fieldTime.reset();
        }

        for (auto &record : *script.afterRemove) {
            record.fieldTime.reset();
        }

        if (script.afterReconnect) {
            for (auto &record : *script.afterReconnect) {
                record.fieldTime.reset();
            }
        }
    }

    const auto peerStarted = std::chrono::steady_clock::now();
    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    const auto frontier = [&](const char *stage) {
        const auto diagnostic = endpoint->getDiagnostic();
        std::cout << "FRONTIER case=" << name << " ticker=" << ticker << " stage=" << stage << " peerMs="
                  << std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                                           peerStarted)
                         .count()
                  << " peerDeadlineMs=10000 endpoint=" << static_cast<int>(endpoint->getState())
                  << " error=" << static_cast<int>(diagnostic.code) << ':' << diagnostic.stage
                  << " adds=" << source.adds() << " removes=" << source.removes() << " peerFailure=" << source.failed()
                  << '\n';
    };
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto ready = [&](unsigned round) {
        if (typedValues->count.load(std::memory_order_acquire) < 2 * round) {
            return false;
        }

        for (unsigned mask = 1; mask <= 31; ++mask) {
            if (values[mask - 1]->count.load(std::memory_order_acquire) <
                static_cast<unsigned>(std::popcount(mask)) * round) {
                return false;
            }
        }

        return true;
    };
    const auto initialReady = eventually([&] {
        return ready(1) || source.failed();
    });

    if (!initialReady) {
        const auto endpointError = endpoint->getDiagnostic();
        const auto typedError = typed->getDiagnostic();
        std::cerr << "DETAIL adds=" << source.adds() << " peer=" << source.failed()
                  << " sent=" << source.initialDataSent() << " endpointState=" << static_cast<int>(endpoint->getState())
                  << " typedState=" << static_cast<int>(typed->getState()) << " typed=" << typedValues->count.load()
                  << " typedError=" << static_cast<int>(typedError.code) << ':' << typedError.stage
                  << " endpointError=" << static_cast<int>(endpointError.code) << ':' << endpointError.stage << ':'
                  << endpointError.nativeCode << ':' << endpointError.detailCode;

        for (std::size_t index = 0; index < mixed.size(); ++index) {
            const auto error = mixed[index]->getDiagnostic();
            std::cerr << " mask" << index + 1 << '=' << values[index]->count.load() << '/'
                      << static_cast<int>(error.code) << ':' << error.stage;
        }

        std::cerr << '\n';
    }

    frontier("initial-ready");
    require(initialReady, "Summary typed/mixed delivery timed out");
    require(!source.failed() && source.adds() == 1, "Exact physical Summary ADD record/symbol set differs");

    if (throwing) {
        require(eventually([&] {
                    return throwing->getDiagnostic().code == q::api::ErrorCode::CALLBACK_FAILURE;
                }),
                "Summary throwing listener was not isolated");
    }

    const unsigned rounds = reconnect ? 2 : 1;

    if (reconnect) {
        endpoint->reconnect();
        require(eventually([&] {
                    return ready(2) || source.failed();
                }),
                "Public Summary reconnect delivery timed out");
        require(!source.failed() && source.adds() == 2, "Reconnect ADD identity differs");
    }

    if (callbackClose) {
        require(callbackClosed->load(std::memory_order_acquire), "Summary callback close did not execute");
    } else {
        typed->clearSymbols();
    }

    require(eventually([&] {
                return source.removes() >= 1 || source.failed();
            }),
            "Composite Summary REMOVE timed out");
    require(eventually([&] {
                return probes->count.load(std::memory_order_acquire) == 1 || source.failed();
            }),
            "Late withdrawn Summary frame did not reach the causal marker");

    if (callbackClose) {
        require(eventually([&] {
                    for (unsigned mask = 1; mask <= 31; ++mask) {
                        if (values[mask - 1]->count.load(std::memory_order_acquire) <
                            2U * static_cast<unsigned>(std::popcount(mask))) {
                            return false;
                        }
                    }

                    return true;
                }),
                "Other31 subscribers missed the causal post-close business probe round");
    }

    if (callbackClose) {
        require(eventually([&] {
                    return typed->getState() == q::api::SubscriptionState::CLOSED;
                }),
                "Callback-closed typed subscription did not retire");
        const auto health = endpoint->getDiagnostic();
        require(endpoint->getState() == q::api::EndpointState::CONNECTED && health.code == q::api::ErrorCode::NONE,
                "Subscription callback close cut off the live underlying endpoint");
        std::cout << "CALLBACK_CLOSE owner=subscription typed=CLOSED endpoint=CONNECTED diagnostic=NONE probes="
                  << probes->count.load() << " masks=31 postCloseRounds=1 peer=" << source.failed() << '\n';
    }

    for (auto &subscription : mixed) {
        if (subscription) {
            subscription->clearSymbols();
        }
    }

    require(eventually([&] {
                return source.removes() >= 2 || source.failed();
            }),
            "Regional Summary mixed REMOVE timed out");
    require(!source.failed(), "Exact physical Summary REMOVE differs");
    frontier("final-remove-before-close");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();

    if (callbackClose || !closed.drained) {
        std::cerr << "CLOSE scenario=" << name << " ticker=" << ticker << " callbackClose=" << callbackClose
                  << " drained=" << closed.drained << " gap=" << closed.hadGap << " expired=" << closed.deadlineExpired
                  << " code=" << static_cast<int>(closed.diagnostic.code) << " stage=" << closed.diagnostic.stage
                  << " native=" << closed.diagnostic.nativeCode << " detail=" << closed.diagnostic.detailCode << '\n';
    }

    require(!closed.deadlineExpired, "Summary endpoint did not retire quiescently");

    if (callbackClose) {
        // Subscription::close requests local ABORT. The queue preserves aborted-registration history;
        // endpoint health and all31 post-close probe deliveries above establish that the socket stayed live.
        require(!closed.drained && !closed.hadGap, "Explicit subscription ABORT history was misclassified");
    } else {
        require(reconnect ? closed.hadGap && !closed.drained : closed.drained,
                "Summary terminal lost the existing reconnect-gap/drain semantics");
    }

    require(typedValues->events.size() == 2 * rounds, "Wrong record/symbol leaked to typed Summary");
    require(probes->events.size() == 1 && probes->events[0].symbol == marker, "Late Summary marker differs");
    auto retainedProbe = probes->events[0];
    retainedProbe.symbol = logical;
    require(f::matches(retainedProbe, finalRegional.expected), "Owning late Summary marker fields differ");

    for (std::size_t index = 0; index < typedValues->events.size(); ++index) {
        const auto &value = typedValues->events[index];
        const auto &expected = index < 2 ? (value.symbol == logical ? regional.expected : base.expected)
                                         : (value.symbol == logical ? finalRegional.expected : finalBase.expected);
        require(f::matches(value, expected), "Complete owning Summary differs after source retirement");
    }

    for (unsigned mask = 1; mask <= 31; ++mask) {
        require(values[mask - 1]->events.size() ==
                    static_cast<unsigned>(std::popcount(mask)) * (rounds + (callbackClose ? 1U : 0U)),
                "Mixed membership/count differs");

        unsigned ethObservations = 0;

        for (const auto &event : values[mask - 1]->events) {
            require((mask & (1U << event.index())) != 0, "Cross-kind mixed delivery");
            require(q::feed_tests::registeredEventSymbol(event) == logical, "Cross-route logical symbol leak");

            if (const auto *trade = std::get_if<q::events::Summary>(&event)) {
                const auto &expected = ethObservations++ == 0 ? regional.expected : finalRegional.expected;
                require(f::matches(*trade, expected), "Mixed Summary differs");
            }

            if (const auto *extended = std::get_if<q::events::TradeETH>(&event)) {
                require(trade_eth_fixture::matches(*extended, ethExpected), "ETH interop differs");
            }

            if (const auto *regularTrade = std::get_if<q::events::Trade>(&event)) {
                require(trade_fixture::matches(*regularTrade, regularExpected), "Regular Trade interop differs");
            }
        }
    }

    std::cout << "PASS scenario=" << name << " sample=" << sample << " ticker=" << ticker << " masks=31"
              << " typed=" << typedValues->events.size() << " adds=" << source.adds() << " removes=" << source.removes()
              << " reconnect=" << reconnect << " probes=" << probes->events.size() << '\n';
}
} // namespace

/** Execute finite deterministic real-wire cases, with RAII retirement of only test-owned objects.
 * @param argc Program plus three immutable oracle paths and an optional exact scenario filter for focused replay.
 * @param argv Borrowed process argument strings.
 * @return Zero for success; one for argument, schema, protocol, mapping, lifetime or callback mismatch.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 4 || argc == 5, "Expected Summary, ordinary Trade and TradeETH fixture paths");
        std::cout << std::unitbuf;
        std::size_t executed = 0;
        const std::string_view filter = argc >= 5 ? argv[4] : "";
        const auto corpus = f::load(argv[1]);
        const auto regular = trade_fixture::load(argv[2]);
        const auto eth = trade_eth_fixture::load(argv[3]);
        std::cout << "LAYOUT Summary=" << sizeof(q::events::Summary)
                  << " RegisteredEvent=" << sizeof(q::api::RegisteredEvent)
                  << " SubscriptionProjection=" << sizeof(q::client::SubscriptionProjection) << '\n';

        for (bool ticker : {false, true}) {
            for (const auto &[name, sample] :
                 std::array<std::pair<const char *, std::size_t>, 18>{{{"oi-long120", 14},
                                                                       {"oi-long120", 16},
                                                                       {"oi-long104", 14},
                                                                       {"oi-long24", 14},
                                                                       {"default120", 0},
                                                                       {"numeric104", 30},
                                                                       {"numeric24", 127},
                                                                       {"raw152", 6},
                                                                       {"interest120", 2},
                                                                       {"interest120", 950},
                                                                       {"interest104", 1},
                                                                       {"missing120", 0},
                                                                       {"symbols120", 2},
                                                                       {"flags120", 31},
                                                                       {"no-metadata120", 0},
                                                                       {"scheme-tiny120", 0},
                                                                       {"scheme-int120", 0},
                                                                       {"missing-Flags120", 0}}}) {
                if (!filter.empty() && filter != name) {
                    continue;
                }

                ++executed;
                const auto profile = std::string_view(name) == "symbols120" ? "symbols120" : "default120";
                check(corpus, regular.at(profile), eth.at(profile), name, sample, ticker,
                      !ticker && std::string_view(name) == "default120",
                      !ticker && std::string_view(name) == "missing-Flags120");
            }
        }

        require(executed > 0, "No matching Summary wire case");

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';

        return 1;
    }
}
