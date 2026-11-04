// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone consumer: public installed headers and explicit deterministic test inputs only.
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/client/subscription_receiver.hpp"
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
namespace f = trade_eth_fixture;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** @param value Required observable condition.
 * @param message Failure retained by the outer bounded process runner.
 */
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}

/** Wait finitely without controlling any process or socket owned by someone else.
 * @tparam Predicate Nonblocking observation.
 * @param predicate Completion, including observable fault conditions.
 * @return False after five seconds, otherwise true.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto end = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= end)
            return false;

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
        require(unit <= 0x7f, "TradeETH corpus public symbol must be ASCII");
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

/** Deliver exact composite/regional physical TradeETH records through installed public typed and mixed subscriptions.
 * @param corpus Owned production observations.
 * @param regular Actual ordinary Trade schema and independently observed values for interop.
 * @param name Numeric/raw/default profile name.
 * @param sample Captured boundary sample ID.
 * @param ticker Select actual TICKER, otherwise STREAM.
 * @param reconnect Explicit public reconnect with two finite peer sessions.
 */
void check(const std::map<std::string, f::Scenario> &corpus, const trade_fixture::Scenario &regular,
           const std::string &name, std::size_t sample, bool ticker, bool reconnect) {
    const auto &scenario = corpus.at(name);
    auto records = scenario.records;

    for (auto record : regular.records) {
        record.id += 2;
        records.push_back(std::move(record));
    }

    auto quote = q::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto profile = q::events::defaultProfileSchema({16, 128, 256, 128, 16384});
    require(bool(quote) && bool(profile), "Default compatible Quote/Profile refused");
    records.push_back(quote->records()[0]);
    records.back().id = 4;
    records.push_back(profile->records()[0]);
    records.back().id = 5;
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Actual composite/regional TradeETH schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::TRADE_ETH, 0},
                            q::api::EventRegistration{q::api::EventType::TRADE_ETH, 1},
                            q::api::EventRegistration{q::api::EventType::TRADE, 2},
                            q::api::EventRegistration{q::api::EventType::TRADE, 3},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 4},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 5}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Complete default finite TradeETH registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 17;
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
    const auto finalSample = reconnect && name == "transition120" ? std::size_t{1} : sample;
    const auto &finalBase = scenario.samples.at({0, finalSample});
    const auto &finalRegional = scenario.samples.at({1, finalSample});
    const auto &regularValue = regular.samples.at({1, name == "symbols120" ? sample : 7});
    require(trade_fixture::units(regularValue.expected[0]) == logical, "Interop Trade logical symbol differs");
    auto typed = feed->createSubscription<q::events::TradeETH>();
    auto typedValues = std::make_shared<Observation<q::events::TradeETH>>();
    typed->addEventListener([typedValues](auto batch) {
        typedValues->append(batch);
    });
    bool refused = false;
    try {
        typed->addSymbols({"NOTREQUESTED", "AAPL&N&Q"});
    } catch (const q::api::Error &error) {
        refused = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT;
    }
    require(refused, "Noninvertible TradeETH selector was not synchronously refused");
    typed->addSymbols({ascii(base.symbol), ascii(logical)});
    std::shared_ptr<q::api::Subscription<q::events::TradeETH>> throwing;

    if (reconnect) {
        throwing = feed->createSubscription<q::events::TradeETH>();
        throwing->addEventListener([](auto) {
            throw std::runtime_error("deliberate TradeETH listener fault");
        });
        throwing->addSymbols({ascii(logical)});
    }

    const std::u16string marker = u"ZZZ145&Q";
    auto probes = std::make_shared<Observation<q::events::TradeETH>>();
    std::array<std::shared_ptr<q::api::Subscription<q::api::RegisteredEvent>>, 15> mixed;
    std::array<std::shared_ptr<Observation<q::api::RegisteredEvent>>, 15> values;

    for (unsigned mask = 1; mask <= 15; ++mask) {
        std::vector<q::api::EventType> kinds;

        for (unsigned kind = 0; kind < 4; ++kind)
            if (mask & (1U << kind))
                kinds.push_back(static_cast<q::api::EventType>(kind));

        mixed[mask - 1] = feed->createSubscription(kinds);
        values[mask - 1] = std::make_shared<Observation<q::api::RegisteredEvent>>();
        mixed[mask - 1]->addEventListener([observed = values[mask - 1], probes, marker](auto batch) {
            for (const auto &event : batch) {
                const auto *trade = std::get_if<q::events::TradeETH>(&event);

                if (trade && trade->symbol == marker)
                    probes->append({trade, 1});
                else
                    observed->append({&event, 1});
            }
        });

        // The final atomic replacement contributes distinct no-data wire tuples. ACTIVE alone
        // does not prove backend membership; their ADD gates the peer's single data batch.
        if (mask == 15)
            mixed[mask - 1]->addSymbols({ascii(logical), ascii(marker)});
        else
            mixed[mask - 1]->addSymbols({ascii(logical)});
    }

    std::vector<peer::Record> data;

    for (std::int32_t id = 0; id < 2; ++id) {
        data.push_back(wireRecord(scenario.samples.at({static_cast<std::size_t>(id), sample}), id));
    }

    data.push_back(wireRecord(regularValue, 3));

    for (std::int32_t id = 4; id < 6; ++id) {
        std::vector<q::core::FieldValue> fields;

        for (const auto &field : records[id].fields)
            fields.push_back({static_cast<q::core::SerialFieldType>(field.serialTypeId), 0});

        data.push_back({logical, std::move(fields), 0, {}, id, q::protocol::FieldEventTime{0, 0, 0}});
    }

    data.push_back(
        {logical, base.values, 0, {}, 0, q::protocol::FieldEventTime{0, 0, 0}}); // Wrong composite tuple must not
    // leak into &Q.
    data.push_back(
        {u"MSFT", regional.values, 0, {}, 1, q::protocol::FieldEventTime{0, 0, 0}}); // Unsubscribed physical symbol.
    const auto &regularComposite = regular.samples.at({0, name == "symbols120" ? sample : 7});
    data.push_back({logical, regularComposite.values, 0, {}, 2, q::protocol::FieldEventTime{0, 0, 0}});
    // The regular composite record cannot satisfy the selected regular regional route.
    const std::array<std::u16string, 9> expected{base.symbol, base.symbol, logical,   logical,  base.symbol,
                                                 marker,      marker,      u"ZZZ145", u"ZZZ145"};
    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = script.fieldMetadata = true;
    script.subscriptionRecordIds = {0, 1, 4, 5, 3, 4, 5, 3, 1};
    // A live marker follows the withdrawn composite in the same frame. Its callback proves
    // the late tuple has passed the receiver before the remaining subscriptions are withdrawn.
    auto probe = wireRecord(finalRegional, 1);
    probe.symbol = u"ZZZ145";
    script.afterRemove = std::vector<peer::Record>{data[0], std::move(probe)};
    script.afterFinalRemove = std::vector<peer::Record>{};
    script.removeSequence = std::vector<peer::ExpectedSubscription>{
        {{base.symbol}, {0}},
        {{base.symbol, logical, logical, base.symbol, marker, marker, u"ZZZ145", u"ZZZ145"},
         {1, 4, 5, 3, 4, 5, 3, 1},
         true}};

    if (reconnect) {
        auto replay = data;
        replay[0] = wireRecord(finalBase, 0);
        replay[1] = wireRecord(finalRegional, 1);
        script.afterReconnect = std::move(replay);
    }

    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto ready = [&](unsigned round) {
        if (typedValues->count.load(std::memory_order_acquire) < 2 * round)
            return false;

        for (unsigned mask = 1; mask <= 15; ++mask)
            if (values[mask - 1]->count.load(std::memory_order_acquire) <
                static_cast<unsigned>(std::popcount(mask)) * round)
                return false;

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

    require(initialReady, "TradeETH typed/mixed delivery timed out");
    require(!source.failed() && source.adds() == 1, "Exact physical TradeETH ADD record/symbol set differs");

    if (throwing) {
        require(eventually([&] {
                    return throwing->getDiagnostic().code == q::api::ErrorCode::CALLBACK_FAILURE;
                }),
                "TradeETH throwing listener was not isolated");
    }

    const unsigned rounds = reconnect ? 2 : 1;

    if (reconnect) {
        endpoint->reconnect();
        require(eventually([&] {
                    return ready(2) || source.failed();
                }),
                "Public TradeETH reconnect delivery timed out");
        require(!source.failed() && source.adds() == 2, "Reconnect ADD identity differs");
    }

    typed->clearSymbols();
    require(eventually([&] {
                return source.removes() >= 1 || source.failed();
            }),
            "Composite TradeETH REMOVE timed out");
    require(eventually([&] {
                return probes->count.load(std::memory_order_acquire) == 1 || source.failed();
            }),
            "Late withdrawn TradeETH frame did not reach the causal marker");

    for (auto &subscription : mixed)
        subscription->clearSymbols();

    require(eventually([&] {
                return source.removes() >= 2 || source.failed();
            }),
            "Regional TradeETH mixed REMOVE timed out");
    require(!source.failed(), "Exact physical TradeETH REMOVE differs");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!closed.deadlineExpired, "TradeETH endpoint did not retire quiescently");
    require(reconnect ? closed.hadGap && !closed.drained : closed.drained,
            "TradeETH terminal lost the existing reconnect-gap/drain semantics");
    require(typedValues->events.size() == 2 * rounds, "Wrong record/symbol leaked to typed TradeETH");
    require(probes->events.size() == 1 && probes->events[0].symbol == marker, "Late TradeETH marker differs");
    auto retainedProbe = probes->events[0];
    retainedProbe.symbol = logical;
    require(f::matches(retainedProbe, finalRegional.expected), "Owning late TradeETH marker fields differ");

    for (std::size_t index = 0; index < typedValues->events.size(); ++index) {
        const auto &value = typedValues->events[index];
        const auto &expected = index < 2 ? (value.symbol == logical ? regional.expected : base.expected)
                                         : (value.symbol == logical ? finalRegional.expected : finalBase.expected);
        require(f::matches(value, expected), "Complete owning TradeETH differs after source retirement");
    }

    if (reconnect && name == "transition120") {
        require(typedValues->events[0].isExtendedTradingHours() && !typedValues->events[2].isExtendedTradingHours(),
                "Actual ETH-to-RTH flag transition was lost");
    }

    for (unsigned mask = 1; mask <= 15; ++mask) {
        require(values[mask - 1]->events.size() == static_cast<unsigned>(std::popcount(mask)) * rounds,
                "Mixed membership/count differs");

        unsigned ethObservations = 0;

        for (const auto &event : values[mask - 1]->events) {
            require((mask & (1U << event.index())) != 0, "Cross-kind mixed delivery");
            std::visit(
                [&](const auto &value) {
                    require(value.symbol == logical, "Cross-route logical symbol leak");
                },
                event);

            if (const auto *trade = std::get_if<q::events::TradeETH>(&event)) {
                const auto &expected = ethObservations++ == 0 ? regional.expected : finalRegional.expected;
                require(f::matches(*trade, expected), "Mixed TradeETH differs");
            }

            if (const auto *regularTrade = std::get_if<q::events::Trade>(&event))
                require(trade_fixture::matches(*regularTrade, regularValue.expected), "Regular Trade interop differs");
        }
    }

    std::cout << "PASS scenario=" << name << " sample=" << sample << " ticker=" << ticker
              << " masks=15 typed=" << typedValues->events.size() << " adds=" << source.adds()
              << " removes=" << source.removes() << " reconnect=" << reconnect << " probes=" << probes->events.size()
              << '\n';
}
} // namespace

/** Execute finite deterministic real-wire cases, with RAII retirement of only test-owned objects.
 * @param argc Program plus the immutable TradeETH and regular Trade oracle TSV paths.
 * @param argv Borrowed process argument strings.
 * @return Zero for success; one for argument, schema, protocol, mapping, lifetime or callback mismatch.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 3, "Expected TradeETH and ordinary Trade fixture paths");
        const auto corpus = f::load(argv[1]);
        const auto regular = trade_fixture::load(argv[2]);
        std::cout << "LAYOUT TradeETH=" << sizeof(q::events::TradeETH)
                  << " RegisteredEvent=" << sizeof(q::api::RegisteredEvent)
                  << " SubscriptionProjection=" << sizeof(q::client::SubscriptionProjection) << '\n';

        for (bool ticker : {false, true})
            for (const auto &[name, sample] :
                 std::array<std::pair<const char *, std::size_t>, 11>{{{"default120", 0},
                                                                       {"numeric104", 30},
                                                                       {"numeric24", 127},
                                                                       {"raw152", 6},
                                                                       {"id152", 6},
                                                                       {"missing120", 0},
                                                                       {"symbols120", 2},
                                                                       {"flags120", 1},
                                                                       {"extra-tick120", 3},
                                                                       {"nano-key120", 0},
                                                                       {"transition120", 0}}})
                check(corpus, regular.at(std::string_view(name) == "symbols120" ? "symbols120" : "default120"), name,
                      sample, ticker, !ticker && std::string_view(name) == "transition120");

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';

        return 1;
    }
}
