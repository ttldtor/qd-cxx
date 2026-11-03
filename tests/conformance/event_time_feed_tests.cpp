// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone conformance consumer: public installed headers only; fixture and peer inputs are explicit.
#include "qd/api/feed.hpp"

#include "connect_peer.hpp"
#include "event_time_fixture.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace f = event_time_fixture;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** Fail the bounded standalone consumer without bypassing its RAII cleanup.
 * @param condition Observable required property.
 * @param message Stable description retained in command logs on failure.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Own callback results independently of the native frames, schema, subscription and callback captures. */
struct Observation {
    std::mutex mutex;                            ///< Protects complete owning heterogeneous event copies.
    std::vector<q::api::RegisteredEvent> events; ///< Retained values survive callback and runtime retirement.
    std::atomic<std::size_t> count{0};           ///< Release/acquire completed-copy publication.
};

/** Wait only on test-owned observations; no network, queue or native progress work is performed here.
 * @tparam Predicate Test-owned nonblocking completion/failure observation.
 * @param predicate Complete observation, borrowed until this finite wait finishes.
 * @return Whether the predicate became true within five seconds.
 */
template <typename Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Build independently encoded valid and wrong-route tuples from actual production cursor samples.
 * @param scenario Matching actual generated subset and original Java values.
 * @param sampleId Original raw cursor sample; different phases use different business values.
 * @param metadata Whether every record carries actual FIELD metadata words.
 * @return Complete selected and deliberately unselected wire tuples, with owned fields and symbols.
 */
std::vector<peer::Record> records(const f::Scenario &scenario, std::size_t sampleId, bool metadata) {
    const std::array<std::pair<std::int32_t, std::u16string_view>, 16> tuples{{{0, u"AAPL"},
                                                                               {1, u"AAPL"},
                                                                               {2, u"AAPL"},
                                                                               {1, u"AAPL{x=y}"},
                                                                               {0, u"AAPL&N"},
                                                                               {3, u"AAPL"},
                                                                               {3, u"AAPL&Q"},
                                                                               {3, u"AAPL&z"},
                                                                               {3, u"AAPL&Q{x=y}"},
                                                                               {3, u"AAPL&N"},
                                                                               {0, u"AAPL&Q"},
                                                                               {1, u"AAPL&Q"},
                                                                               {2, u"IBM"},
                                                                               {1, u"IBM"},
                                                                               {2, u"AAPL&Q"},
                                                                               {3, u"IBM"}}};
    std::vector<peer::Record> result;

    for (const auto &[id, symbol] : tuples) {
        const auto &sample = scenario.samples.at({static_cast<std::size_t>(id), sampleId});
        peer::Record record{std::u16string(symbol), sample.values, 0, {}, id};

        if (metadata) {
            record.fieldTime =
                q::protocol::FieldEventTime{sample.rawEventTime >> 32, static_cast<std::int32_t>(sample.rawEventTime)};
        }

        result.push_back(std::move(record));
    }

    return result;
}

/** Compare complete delivered logical membership and business time against independent Java observations.
 * @param values Actual owning Feed events for exactly one source phase.
 * @param scenario Original production cursor/delegate observations.
 * @param sampleId Original source sample used for this phase.
 * @param metadata Whether actual FIELD words were declared by the wire peer.
 * @param removed Whether the logical AAPL&Q selector was removed before this phase.
 */
void verify(std::span<const q::api::RegisteredEvent> values, const f::Scenario &scenario, std::size_t sampleId,
            bool metadata, bool removed) {
    std::map<std::pair<bool, std::u16string>, unsigned> observed;

    for (const auto &value : values) {
        if (const auto *quote = std::get_if<q::events::Quote>(&value)) {
            const auto id = quote->symbol == u"AAPL&Q" || quote->symbol == u"AAPL&Q{x=y}" ? 1U
                            : quote->symbol == u"AAPL&z"                                  ? 2U
                                                                                          : 0U;
            const auto &expected = scenario.samples.at({id, sampleId}).expected;
            require(quote->bidTimeMs == std::stoll(expected.at(1)), "Quote bid time differs from Java");
            require(quote->askTimeMs == std::stoll(expected.at(3)), "Quote ask time differs from Java");
            require(quote->bidExchangeCode == std::stoul(expected.at(2)), "Quote bid exchange differs from Java");
            require(quote->askExchangeCode == std::stoul(expected.at(4)), "Quote ask exchange differs from Java");
            require(quote->timeMs == std::stoll(expected.at(5)), "Quote packed time differs from Java");
            require(quote->sequence == std::stoll(expected.at(6)), "Quote Sequence differs from Java");
            require(quote->timeNanoPart == std::stoll(expected.at(7)), "Quote TimeNanoPart differs from Java");
            require(quote->eventTime == (metadata ? std::stoll(expected.at(8)) : 0), "Quote metadata time differs");
            ++observed[{true, quote->symbol}];
        } else {
            const auto &profile = std::get<q::events::Profile>(value);
            const auto &expected = scenario.samples.at({3, sampleId}).expected;
            require(profile.haltStartTimeMs == std::stoll(expected.at(1)), "Profile halt start differs from Java");
            require(profile.haltEndTimeMs == std::stoll(expected.at(2)), "Profile halt end differs from Java");
            require(profile.eventTime == (metadata ? std::stoll(expected.at(3)) : 0), "Profile metadata time differs");
            const auto text =
                expected.at(4) == "null" ? std::optional<std::u16string>{} : std::optional(f::units(expected[4]));
            require(profile.description == text, "Owned Profile description differs from Java");
            ++observed[{false, profile.symbol}];
        }
    }

    for (const auto symbol : {u"AAPL", u"AAPL&Q", u"AAPL&z", u"AAPL&Q{x=y}", u"AAPL&N"}) {
        for (const bool quote : {false, true}) {
            const auto key = std::pair{quote, std::u16string(symbol)};

            if (removed && key.second == u"AAPL&Q") {
                require(!observed.contains(key), "Removed logical selector leaked a value");
            } else {
                require(observed[key] == 1, "Wrong record or physical symbol leaked into logical membership");
            }

            observed.erase(key);
        }
    }

    require(observed.empty(), "Unexpected typed event symbol reached the listener");
}

/** Exercise actual Feed subscription, wire record variants, optional metadata, removal and explicit reconnect.
 * @param corpus Immutable production observations selected by the explicit fixture argument.
 * @param ticker Use the existing TICKER family, otherwise STREAM.
 * @param metadata Declare actual physical FIELD words independently of the business fields.
 * @param reconnect Exercise one public explicit reconnect, with a second independently validated wire ADD.
 */
void check(const std::map<std::string, f::Scenario> &corpus, bool ticker, bool metadata, bool reconnect) {
    const auto &scenario = corpus.at(metadata ? "millis" : "default");
    auto schema = q::core::LocalSchema::create(scenario.records, {16, 128, 256, 128, 16384},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Actual production subset does not fit the existing finite schema");
    const std::array routes{q::api::EventRegistration{q::api::EventType::QUOTE, 0},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 1},
                            q::api::EventRegistration{q::api::EventType::QUOTE, 2},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 3}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Actual generated Quote/Profile registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 1;
    runtimeOptions.maximumSymbolsPerSubscription = 5;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = ticker ? q::api::DeliveryContract::TICKER : q::api::DeliveryContract::STREAM;
    options.maximumAttempts = reconnect ? 2 : 1;
    options.retryDelay = 1ms;
    options.readTimeout = 5000ms;
    options.drainTimeout = 1000ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto mixed = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::PROFILE});
    const auto observation = std::make_shared<Observation>();
    mixed->addEventListener([observation](std::span<const q::api::RegisteredEvent> batch) {
        std::lock_guard lock(observation->mutex);
        observation->events.insert(observation->events.end(), batch.begin(), batch.end());
        observation->count.store(observation->events.size(), std::memory_order_release);
    });
    mixed->addSymbols({"AAPL", "AAPL&Q", "AAPL&z", "AAPL&Q{x=y}", "AAPL&N"});

    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = true;
    script.fieldMetadata = metadata;
    script.subscriptionRecordIds = {0, 0, 1, 1, 2, 3, 3, 3, 3, 3};
    const std::array<std::u16string, 10> expected{u"AAPL", u"AAPL&N", u"AAPL",   u"AAPL{x=y}",   u"AAPL",
                                                  u"AAPL", u"AAPL&Q", u"AAPL&z", u"AAPL&Q{x=y}", u"AAPL&N"};
    script.removeSequence = std::vector{peer::ExpectedSubscription{{u"AAPL", u"AAPL&Q"}, {1, 3}},
                                        peer::ExpectedSubscription{{u"AAPL", u"AAPL&N", u"AAPL{x=y}", u"AAPL", u"AAPL",
                                                                    u"AAPL&z", u"AAPL&Q{x=y}", u"AAPL&N"},
                                                                   {0, 0, 1, 2, 3, 3, 3, 3}}};
    script.afterRemove = records(scenario, 2, metadata);

    if (reconnect) {
        script.afterReconnect = records(scenario, 1, metadata);
    }

    const auto initial = records(scenario, 0, metadata);
    peer::Peer source(*schema, initial, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observation->count.load(std::memory_order_acquire) >= 10 || source.failed() ||
                       mixed->getDiagnostic().code != q::api::ErrorCode::NONE;
            }),
            "Initial regional Feed delivery timed out");
    require(!source.failed() && source.adds() == 1, "Actual initial wire ADD record/base-symbol tuples differ");
    require(observation->count.load() == 10, "Initial actual Feed batch has missing or unselected records");

    for (const auto bad : {"AAPL&N&Q", "AAPL{x=y}&Q"}) {
        bool refused = false;

        try {
            mixed->setSymbols({"UNPUBLISHED", bad});
        } catch (const q::api::Error &error) {
            refused = error.diagnostic().code == q::api::ErrorCode::INVALID_ARGUMENT &&
                      error.diagnostic().stage == "subscription-symbol-representation";
        }

        require(refused, "Nonrepresentable regional selector was accepted by the active Feed");
    }

    if (reconnect) {
        endpoint->reconnect();
        const auto replayed = eventually([&] {
            return observation->count.load(std::memory_order_acquire) >= 20 || source.failed() ||
                   mixed->getDiagnostic().code != q::api::ErrorCode::NONE;
        });

        if (!replayed) {
            std::cerr << "reconnect observedAdds=" << source.adds() << " events=" << observation->count.load()
                      << " peerFailed=" << source.failed() << " endpointStage=" << endpoint->getDiagnostic().stage
                      << " subscriptionStage=" << mixed->getDiagnostic().stage << '\n';
        }

        require(replayed, "Explicit reconnect regional replay timed out");
        require(!source.failed() && source.adds() == 2, "Replayed wire ADD record/base-symbol tuples differ");
        require(observation->count.load() == 20, "Explicit reconnect leaked or lost regional records");
    }

    const auto beforeRemove = reconnect ? std::size_t{20} : std::size_t{10};
    mixed->removeSymbols("AAPL&Q");
    require(eventually([&] {
                return observation->count.load(std::memory_order_acquire) >= beforeRemove + 8 || source.failed() ||
                       mixed->getDiagnostic().code != q::api::ErrorCode::NONE;
            }),
            "Post-removal surviving route barrier timed out");
    require(!source.failed() && source.removes() == 1, "Actual regional REMOVE record/base-symbol tuples differ");
    require(observation->count.load() == beforeRemove + 8, "Removed or wrong-route records leaked after REMOVE");
    mixed->clearSymbols();
    require(eventually([&] {
                return source.removes() == 2 || source.failed();
            }),
            "Final bounded REMOVE timed out");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!source.failed(), "Controlled wire peer failed");
    require(!closed.deadlineExpired, "Feed retirement exceeded its explicit allowance");
    require(reconnect ? closed.hadGap && !closed.drained : closed.drained,
            "Feed terminal lost the existing reconnect-gap/drain semantics");
    require(mixed->getDiagnostic().code == q::api::ErrorCode::NONE, "Regional subscription failed");

    std::lock_guard lock(observation->mutex);
    require(observation->events.size() == beforeRemove + 8, "Late removed values reached a closed logical view");
    const auto values = std::span<const q::api::RegisteredEvent>(observation->events);
    verify(values.first(10), scenario, 0, metadata, false);

    if (reconnect) {
        verify(values.subspan(10, 10), scenario, 1, metadata, false);
    }

    verify(values.subspan(beforeRemove), scenario, 2, metadata, true);
    std::cout << "PASS family=" << (ticker ? "TICKER" : "STREAM") << " metadata=" << metadata
              << " explicitReconnect=" << reconnect << " events=" << observation->events.size()
              << " actualAdds=" << source.adds() << " actualRemoves=" << source.removes() << '\n';
}

/** Verify literal composite fallback or the exact maximum public symbol size through real wire commands.
 * @param corpus Original production default schema and cursor values.
 * @param logical One maximum-length symbol, or two noninvertible regional-looking composite literals.
 * @param mixed Include Profile so every registered route consumes the maximum-size command boundary.
 */
void checkCompositeBoundary(const std::map<std::string, f::Scenario> &corpus, const std::vector<std::string> &logical,
                            bool mixed) {
    const auto &scenario = corpus.at("default");
    auto schema = q::core::LocalSchema::create(scenario.records, {16, 128, 256, 128, 16384},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Composite boundary schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::QUOTE, 0},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 3}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Composite boundary registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 1;
    runtimeOptions.maximumSymbolsPerSubscription = logical.size();
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = q::api::DeliveryContract::STREAM;
    options.maximumAttempts = 1;
    options.drainTimeout = 1000ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription =
        mixed ? endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::PROFILE})
              : endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE});
    const auto observation = std::make_shared<Observation>();
    subscription->addEventListener([observation](std::span<const q::api::RegisteredEvent> batch) {
        std::lock_guard lock(observation->mutex);
        observation->events.insert(observation->events.end(), batch.begin(), batch.end());
        observation->count.store(observation->events.size(), std::memory_order_release);
    });

    if (logical.size() == 1) {
        subscription->addSymbols(logical[0]);
    } else {
        require(logical.size() == 2, "Composite boundary input inventory differs");
        subscription->addSymbols({logical[0], logical[1]});
    }

    peer::PeerScript script;
    script.canonicalSymbols = script.describeAllRecords = true;
    std::vector<peer::Record> tuples;
    std::vector<std::u16string> expected;

    for (const auto id : {0, 3}) {
        if (id == 3 && !mixed) {
            continue;
        }

        for (const auto &symbol : logical) {
            const std::u16string units(symbol.begin(), symbol.end());
            tuples.push_back({units, scenario.samples.at({static_cast<std::size_t>(id), 0}).values, 0, {}, id});
            expected.push_back(units);
            script.subscriptionRecordIds.push_back(id);
        }
    }

    peer::Peer source(*schema, tuples, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observation->count.load(std::memory_order_acquire) >= expected.size() || source.failed() ||
                       subscription->getDiagnostic().code != q::api::ErrorCode::NONE;
            }),
            "Composite/max-unit Feed delivery timed out");
    require(!source.failed() && source.adds() == 1, "Composite/max-unit exact ADD differs");
    require(observation->count.load() == expected.size(), "Composite/max-unit values refused or misrouted");
    subscription->clearSymbols();
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Composite/max-unit REMOVE timed out");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(closed.drained && !closed.deadlineExpired && !source.failed(), "Composite/max-unit retirement failed");
    std::lock_guard lock(observation->mutex);
    std::map<std::pair<bool, std::u16string>, unsigned> received;

    for (const auto &value : observation->events) {
        const auto symbol = std::visit(
            [](const auto &event) {
                return event.symbol;
            },
            value);
        ++received[{std::holds_alternative<q::events::Quote>(value), symbol}];
    }

    for (const auto &symbol : logical) {
        const std::u16string units(symbol.begin(), symbol.end());
        require(received[{true, units}] == 1, "Composite Quote changed its exact logical symbol");

        if (mixed) {
            require(received[{false, units}] == 1, "Composite Profile changed its maximum-unit symbol");
        }
    }

    require(received.size() == expected.size(), "Composite fallback leaked an unrequested logical symbol");
    std::cout << "PASS compositeBoundary mixed=" << mixed << " logicalSymbols=" << logical.size()
              << " units=" << logical[0].size() << " actualAdds=" << source.adds() << '\n';
}
} // namespace

/** Run the standalone actual Feed consumer with a sole explicit immutable oracle corpus.
 * @param argc Program name and fixture filename; no network work starts for invalid arguments.
 * @param argv Borrowed command-line arguments.
 * @return Zero after all observations,2 for usage,1 for a diagnosed test failure with owned RAII cleanup.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }

    try {
        const auto corpus = f::read(argv[1]);

        for (const bool ticker : {false, true}) {
            for (const bool metadata : {false, true}) {
                check(corpus, ticker, metadata, false);
            }
        }

        check(corpus, false, true, true);
        checkCompositeBoundary(corpus, {std::string(128, 'A')}, true);
        checkCompositeBoundary(corpus, {"AAPL&N&Q", "AAPL{x=y}&Q"}, false);

        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';

        return 1;
    }
}
