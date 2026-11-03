// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Standalone consumer: only public installed SDK headers and explicitly supplied test inputs.
#include "qd/api/feed.hpp"
#include "qd/client/subscription_receiver.hpp"

#include "connect_peer.hpp"
#include "field_conversion_fixture.hpp"

#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace f = field_conversion_fixture;
namespace peer = q::connect_tests;
using namespace std::chrono_literals;

/** Fail with RAII cleanup of this consumer's own endpoint, runtime and peer.
 * @param condition Required observable result.
 * @param message Failure text retained in native logs.
 */
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

/** Compare one Java business getter, preserving finite/infinite/zero bits and classifying NaN.
 * @param actual Public typed event getter result.
 * @param expected Signed raw binary64 capture cell.
 * @return True for identical non-NaN bits or two NaN values.
 */
bool sameBusinessDouble(double actual, const std::string &expected) {
    const auto bits = static_cast<std::uint64_t>(std::stoll(expected));

    return std::isnan(std::bit_cast<double>(bits)) ? std::isnan(actual) : std::bit_cast<std::uint64_t>(actual) == bits;
}

/** Bounded wait observing only test-owned completion state.
 * @tparam Predicate Nonblocking observation callable.
 * @param predicate Observation borrowed until the wait returns.
 * @return Whether completion occurred within five seconds.
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

/** Retain actual callback output after all source and endpoint storage has retired. */
struct Observation {
    std::mutex mutex;                            ///< Protect complete owning result publication.
    std::vector<q::api::RegisteredEvent> events; ///< Independent owning events.
    std::atomic<std::size_t> count{0};           ///< Release/acquire result publication.
};

/** Deliver converted fields through public Feed using an exact configured registry and real TCP peer.
 * @param corpus Actual production mapping/delegate observations.
 * @param name Exact scenario to encode without changing its physical types.
 * @param sampleId Captured sign/precision/text boundary sample.
 * @param ticker Select the existing TICKER contract, otherwise STREAM.
 */
void check(const std::map<std::string, f::Scenario> &corpus, const std::string &name, std::size_t sampleId,
           bool ticker) {
    const auto &scenario = corpus.at(name);
    auto schema = q::core::LocalSchema::create(scenario.records, {2, 128, 256, 128, 16384},
                                               q::core::LocalProfile::BINARY_FIELDS_V1);
    require(bool(schema), "Production conversion schema refused");
    const std::array routes{q::api::EventRegistration{q::api::EventType::QUOTE, 0},
                            q::api::EventRegistration{q::api::EventType::PROFILE, 1}};
    q::api::EventRegistryWork work;
    auto registry = q::api::EventRegistry::create(*schema, routes, {}, work);
    require(bool(registry), "Default finite conversion registry refused");
    q::api::RuntimeOptions runtimeOptions;
    runtimeOptions.maximumSubscriptionsPerEndpoint = 1;
    runtimeOptions.maximumSymbolsPerSubscription = 1;
    runtimeOptions.dnsServers.push_back({{127, 0, 0, 1}});
    q::api::ApiRuntime runtime(runtimeOptions);
    q::api::EndpointOptions options;
    options.contract = ticker ? q::api::DeliveryContract::TICKER : q::api::DeliveryContract::STREAM;
    options.maximumAttempts = 1;
    options.readTimeout = 5000ms;
    options.drainTimeout = 1000ms;
    options.eventRegistry = *registry;
    auto endpoint = runtime.createEndpoint(options);
    auto subscription = endpoint->getFeed()->createSubscription({q::api::EventType::QUOTE, q::api::EventType::PROFILE});
    auto observation = std::make_shared<Observation>();
    subscription->addEventListener([observation](std::span<const q::api::RegisteredEvent> batch) {
        std::lock_guard lock(observation->mutex);
        observation->events.insert(observation->events.end(), batch.begin(), batch.end());
        observation->count.store(observation->events.size(), std::memory_order_release);
    });
    subscription->addSymbols({"AAPL"});
    std::vector<peer::Record> records;

    for (std::int32_t id = 0; id < 2; ++id) {
        records.push_back({u"AAPL", scenario.samples.at({static_cast<std::size_t>(id), sampleId}).values, 0, {}, id});
    }

    const std::array<std::u16string, 2> symbols{u"AAPL", u"AAPL"};
    peer::PeerScript script;
    script.family = ticker ? q::core::SubscriptionFamily::TICKER : q::core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = true;
    script.subscriptionRecordIds = {0, 1};
    peer::Peer source(*schema, records, symbols, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    require(eventually([&] {
                return observation->count.load(std::memory_order_acquire) >= 2 || source.failed() ||
                       subscription->getDiagnostic().code != q::api::ErrorCode::NONE;
            }),
            "Converted wire delivery timed out");
    require(!source.failed() && source.adds() == 1, "Exact wire ADD tuples differ");
    require(observation->count.load() == 2, "Converted event count differs");
    subscription->clearSymbols();
    require(eventually([&] {
                return source.removes() == 1 || source.failed();
            }),
            "Exact wire REMOVE timed out");
    const auto closed = endpoint->closeAndAwaitTermination();
    runtime.closeAndAwaitTermination();
    require(!closed.deadlineExpired && closed.drained, "Converted endpoint retirement failed");
    require(!source.failed() && subscription->getDiagnostic().code == q::api::ErrorCode::NONE,
            "Converted subscription failed");
    std::lock_guard lock(observation->mutex);
    bool sawQuote = false, sawProfile = false;

    for (const auto &event : observation->events) {
        if (const auto *quote = std::get_if<q::events::Quote>(&event)) {
            const auto &expected = scenario.samples.at({0, sampleId}).expected;
            require(quote->bidPrice.raw == std::stoll(expected.at(0)) &&
                        quote->askSize.raw == std::stoll(expected.at(3)),
                    "Converted Quote decimals differ");
            require(quote->sequence == std::stoll(expected.at(9)) && quote->timeNanoPart == std::stoll(expected.at(10)),
                    "Converted Quote physical slots differ");
            const std::array values{quote->getBidPrice(), quote->getBidSizeAsDouble(), quote->getAskPrice(),
                                    quote->getAskSizeAsDouble()};

            for (std::size_t i = 0; i < values.size(); ++i) {
                require(sameBusinessDouble(values[i], expected.at(11 + i)), "Converted Quote business getter differs");
            }

            sawQuote = true;
        } else {
            const auto &profile = std::get<q::events::Profile>(event);
            const auto &expected = scenario.samples.at({1, sampleId}).expected;
            require(profile.beta.raw == std::stoll(expected.at(0)) &&
                        profile.freeFloat.raw == std::stoll(expected.at(7)),
                    "Converted Profile decimals differ");
            require(profile.haltStartTimeMs == std::stoll(expected.at(11)) &&
                        profile.flags == std::stoll(expected.at(13)),
                    "Converted Profile physical slots differ");
            const auto description =
                expected.at(14) == "null" ? std::optional<std::u16string>{} : std::optional(f::units(expected.at(14)));
            require(profile.description == description, "Converted nullable String differs");
            const std::array values{profile.getBeta(),
                                    profile.getEarningsPerShare(),
                                    profile.getDividendFrequency(),
                                    profile.getExDividendAmount(),
                                    profile.getHigh52WeekPrice(),
                                    profile.getLow52WeekPrice(),
                                    profile.getShares(),
                                    profile.getFreeFloat(),
                                    profile.getHighLimitPrice(),
                                    profile.getLowLimitPrice()};

            for (std::size_t i = 0; i < values.size(); ++i) {
                require(sameBusinessDouble(values[i], expected.at(16 + i)),
                        "Converted Profile business getter differs");
            }

            sawProfile = true;
        }
    }

    require(sawQuote && sawProfile, "Mixed converted event membership differs");
    std::cout << "PASS scenario=" << name << " sample=" << sampleId << " ticker=" << ticker
              << " events=2 actualAdds=" << source.adds() << " actualRemoves=" << source.removes() << '\n';
}
} // namespace

/** Consume an explicit offline oracle fixture; all test-owned processes and sockets retire automatically.
 * @param argc Program name plus one fixture pathname.
 * @param argv Borrowed process arguments.
 * @return Zero on all cases; one on invalid input, mapping, peer or delivery failure.
 */
int main(int argc, char **argv) {
    try {
        require(argc == 2, "Expected conversion fixture path");
        const auto corpus = f::load(argv[1]);
        std::cout << "LAYOUT Quote=" << sizeof(q::events::Quote) << " Profile=" << sizeof(q::events::Profile)
                  << " RegisteredEvent=" << sizeof(q::api::RegisteredEvent)
                  << " SubscriptionProjection=" << sizeof(q::client::SubscriptionProjection)
                  << " QuoteSnapshot=" << sizeof(decltype(q::events::Quote{}.businessNumericSnapshot))
                  << " ProfileSnapshot=" << sizeof(decltype(q::events::Profile{}.businessNumericSnapshot)) << '\n';

        for (bool ticker : {false, true}) {
            for (const auto &[name, sample] :
                 std::array<std::pair<const char *, std::size_t>, 6>{{{"numeric24", 127},
                                                                      {"numeric104", 30},
                                                                      {"numeric4", 0},
                                                                      {"raw152", 6},
                                                                      {"text137", 2},
                                                                      {"missing120", 0}}}) {
                check(corpus, name, sample, ticker);
            }
        }

        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << error.what() << '\n';

        return 1;
    }
}
