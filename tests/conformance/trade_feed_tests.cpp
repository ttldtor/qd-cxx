// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone consumer: public installed headers and explicit deterministic test inputs only.
#include "connect_peer.hpp"
#include "qd/api/feed.hpp"
#include "qd/client/subscription_receiver.hpp"
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
namespace f = trade_fixture;
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
        require(unit <= 0x7f, "Trade corpus public symbol must be ASCII");
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

/** Deliver exact composite/regional physical Trade records through installed public typed and mixed subscriptions.
 * @param corpus Owned production observations.
 * @param name Numeric/raw/default profile name.
 * @param sample Captured boundary sample ID.
 * @param ticker Select actual TICKER, otherwise STREAM.
 * @param reconnect Explicit public reconnect with two finite peer sessions.
 */
void check(const std::map<std::string, f::Scenario> &corpus, const std::string &name, std::size_t sample, bool ticker,
           bool reconnect) {
    const auto &scenario = corpus.at(name);
    auto records = scenario.records;
    auto quote = q::events::defaultQuoteSchema({16, 128, 256, 128, 16384});
    auto profile = q::events::defaultProfileSchema({16, 128, 256, 128, 16384});
    require(bool(quote) && bool(profile), "Default compatible Quote/Profile refused");
    records.push_back(quote->records()[0]);
    records.back().id = 2;
    records.push_back(profile->records()[0]);
    records.back().id = 3;
    auto schema =
        q::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Actual composite/regional Trade schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::TRADE, 0},
                            q::api::EventRegistration{q::api::EventType::TRADE, 1},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 2},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 3}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Complete default finite Trade registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 10;
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
    auto typed = feed->createSubscription<q::events::Trade>();
    auto typedValues = std::make_shared<Observation<q::events::Trade>>();
    typed->addEventListener([typedValues](auto batch) {
        typedValues->append(batch);
    });
    bool refused = false;
    try {
        typed->addSymbols({"NOTREQUESTED", "AAPL&N&Q"});
    } catch (const q::api::Error &error) {
        refused = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT;
    }
    require(refused, "Noninvertible Trade selector was not synchronously refused");
    typed->addSymbols({ascii(base.symbol), ascii(logical)});
    std::shared_ptr<q::api::Subscription<q::events::Trade>> throwing;

    if (reconnect) {
        throwing = feed->createSubscription<q::events::Trade>();
        throwing->addEventListener([](auto) {
            throw std::runtime_error("deliberate Trade listener fault");
        });
        throwing->addSymbols({ascii(logical)});
    }

    const std::u16string marker = u"ZZZ144&Q";
    auto probes = std::make_shared<Observation<q::events::Trade>>();
    std::array<std::shared_ptr<q::api::Subscription<q::api::RegisteredEvent>>, 7> mixed;
    std::array<std::shared_ptr<Observation<q::api::RegisteredEvent>>, 7> values;

    for (unsigned mask = 1; mask <= 7; ++mask) {
        std::vector<q::api::EventType> kinds;

        for (unsigned kind = 0; kind < 3; ++kind)
            if (mask & (1U << kind))
                kinds.push_back(static_cast<q::api::EventType>(kind));

        mixed[mask - 1] = feed->createSubscription(kinds);
        values[mask - 1] = std::make_shared<Observation<q::api::RegisteredEvent>>();
        mixed[mask - 1]->addEventListener([observed = values[mask - 1], probes, marker](auto batch) {
            for (const auto &event : batch) {
                const auto *trade = std::get_if<q::events::Trade>(&event);

                if (trade && trade->symbol == marker)
                    probes->append({trade, 1});
                else
                    observed->append({&event, 1});
            }
        });

        // The final atomic replacement contributes distinct no-data wire tuples. ACTIVE alone
        // does not prove backend membership; their ADD gates the peer's single data batch.
        if (mask == 7)
            mixed[mask - 1]->addSymbols({ascii(logical), ascii(marker)});
        else
            mixed[mask - 1]->addSymbols({ascii(logical)});
    }

    std::vector<peer::Record> data;

    for (std::int32_t id = 0; id < 2; ++id) {
        const auto &source = scenario.samples.at({static_cast<std::size_t>(id), sample});
        const auto packed = static_cast<std::uint64_t>(source.rawEventTime);
        q::protocol::FieldEventTime metadata{std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packed >> 32)),
                                             std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(packed)),
                                             static_cast<std::uint64_t>(id)};
        data.push_back({source.symbol, source.values, 0, {}, id, metadata});
    }

    for (std::int32_t id = 2; id < 4; ++id) {
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
    const std::array<std::u16string, 7> expected{base.symbol, base.symbol, logical, logical, marker, marker, u"ZZZ144"};
    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = script.fieldMetadata = true;
    script.subscriptionRecordIds = {0, 1, 2, 3, 2, 3, 1};
    // A live marker follows the withdrawn composite in the same frame. Its callback proves
    // the late tuple has passed the receiver before the remaining subscriptions are withdrawn.
    auto probe = data[1];
    probe.symbol = u"ZZZ144";
    script.afterRemove = std::vector<peer::Record>{data[0], std::move(probe)};
    script.afterFinalRemove = std::vector<peer::Record>{};
    script.removeSequence = std::vector<peer::ExpectedSubscription>{
        {{base.symbol}, {0}}, {{base.symbol, logical, logical, marker, marker, u"ZZZ144"}, {1, 2, 3, 2, 3, 1}, true}};

    if (reconnect)
        script.afterReconnect = data;

    peer::Peer source(*schema, data, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    const auto ready = [&](unsigned round) {
        if (typedValues->count.load(std::memory_order_acquire) < 2 * round)
            return false;

        for (unsigned mask = 1; mask <= 7; ++mask)
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

    require(initialReady, "Trade typed/mixed delivery timed out");
    require(!source.failed() && source.adds() == 1, "Exact physical Trade ADD record/symbol set differs");

    if (throwing) {
        require(eventually([&] {
                    return throwing->getDiagnostic().code == q::api::ErrorCode::CALLBACK_FAILURE;
                }),
                "Trade throwing listener was not isolated");
    }

    const unsigned rounds = reconnect ? 2 : 1;

    if (reconnect) {
        endpoint->reconnect();
        require(eventually([&] {
                    return ready(2) || source.failed();
                }),
                "Public Trade reconnect delivery timed out");
        require(!source.failed() && source.adds() == 2, "Reconnect ADD identity differs");
    }

    typed->clearSymbols();
    require(eventually([&] {
                return source.removes() >= 1 || source.failed();
            }),
            "Composite Trade REMOVE timed out");
    require(eventually([&] {
                return probes->count.load(std::memory_order_acquire) == 1 || source.failed();
            }),
            "Late withdrawn Trade frame did not reach the causal marker");

    for (auto &subscription : mixed)
        subscription->clearSymbols();

    require(eventually([&] {
                return source.removes() >= 2 || source.failed();
            }),
            "Regional Trade mixed REMOVE timed out");
    require(!source.failed(), "Exact physical Trade REMOVE differs");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!closed.deadlineExpired, "Trade endpoint did not retire quiescently");
    require(reconnect ? closed.hadGap && !closed.drained : closed.drained,
            "Trade terminal lost the existing reconnect-gap/drain semantics");
    require(typedValues->events.size() == 2 * rounds, "Wrong record/symbol leaked to typed Trade");
    require(probes->events.size() == 1 && probes->events[0].symbol == marker, "Late Trade marker differs");
    auto retainedProbe = probes->events[0];
    retainedProbe.symbol = logical;
    require(f::matches(retainedProbe, regional.expected), "Owning late Trade marker fields differ");

    for (const auto &value : typedValues->events)
        require(f::matches(value, value.symbol == logical ? regional.expected : base.expected),
                "Complete owning Trade differs after source retirement");

    for (unsigned mask = 1; mask <= 7; ++mask) {
        require(values[mask - 1]->events.size() == static_cast<unsigned>(std::popcount(mask)) * rounds,
                "Mixed membership/count differs");

        for (const auto &event : values[mask - 1]->events) {
            require((mask & (1U << event.index())) != 0, "Cross-kind mixed delivery");
            std::visit(
                [&](const auto &value) {
                    require(value.symbol == logical, "Cross-route logical symbol leak");
                },
                event);

            if (const auto *trade = std::get_if<q::events::Trade>(&event))
                require(f::matches(*trade, regional.expected), "Mixed Trade differs");
        }
    }

    std::cout << "PASS scenario=" << name << " sample=" << sample << " ticker=" << ticker
              << " masks=7 typed=" << typedValues->events.size() << " adds=" << source.adds()
              << " removes=" << source.removes() << " reconnect=" << reconnect << " probes=" << probes->events.size()
              << '\n';
}
} // namespace

/** Execute finite deterministic real-wire cases, with RAII retirement of only test-owned objects.
 * @param argc Program plus one immutable oracle TSV path.
 * @param argv Borrowed process argument strings.
 * @return Zero for success; one for argument, schema, protocol, mapping, lifetime or callback mismatch.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected Trade fixture path");
        const auto corpus = f::load(argv[1]);
        std::cout << "LAYOUT Trade=" << sizeof(q::events::Trade)
                  << " RegisteredEvent=" << sizeof(q::api::RegisteredEvent)
                  << " SubscriptionProjection=" << sizeof(q::client::SubscriptionProjection) << '\n';

        for (bool ticker : {false, true})
            for (const auto &[name, sample] : std::array<std::pair<const char *, std::size_t>, 7>{{{"default120", 0},
                                                                                                   {"numeric104", 30},
                                                                                                   {"numeric24", 127},
                                                                                                   {"raw152", 6},
                                                                                                   {"id152", 6},
                                                                                                   {"missing120", 0},
                                                                                                   {"symbols120", 2}}})
                check(corpus, name, sample, ticker, !ticker && std::string_view(name) == "default120");

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';

        return 1;
    }
}
