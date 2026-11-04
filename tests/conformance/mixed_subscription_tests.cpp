// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: finite explicit type lists, controlled native peers and owning callback observations.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed.hpp"

#include "connect_peer.hpp"
#include "receiving_profile.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace api = root::api;
namespace core = root::core;
namespace events = root::events;
namespace peer = root::connect_tests;
using namespace std::chrono_literals;
constexpr core::SchemaLimits SCHEMA_LIMITS{16, 128, 256, 128, 16384}; ///< Existing qualified receiving ceiling.

/** Owning callback state remains alive through any failed assertion and runtime retirement. */
struct Observation {
    std::mutex mutex;                        ///< Protects complete owning event copies.
    std::vector<api::RegisteredEvent> mixed; ///< Actual heterogeneous values, never callback borrows.
    std::atomic<unsigned> quotes{0};         ///< Independent concrete-only callback observation.
    std::atomic<unsigned> mixedCount{0};     ///< Complete mixed event copies published by the callback.
    std::atomic<unsigned> wrongThread{0};    ///< Actual creator-affinity refusal observed inside a worker listener.
};

/** Test-owned capture action runs exactly when its final shared listener capture retires. */
struct CaptureAction {
    std::function<void()> action; ///< Complete nonthrowing observation, retained independently of its listener.

    /** Execute the final lifetime observation outside SDK control locks. */
    ~CaptureAction() {
        action();
    }
};

/** Keep runtime setup independent of ambient system resolver discovery.
 * @return Finite ordinary options with one explicit numeric resolver snapshot.
 */
api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

/** Use one finite attempt and the actual selected existing source contract.
 * @param family Existing STREAM/TICKER source family.
 * @return Explicit endpoint policy without changing schema or queue limits.
 */
api::EndpointOptions endpointOptions(core::SubscriptionFamily family) {
    api::EndpointOptions result;
    result.contract =
        family == core::SubscriptionFamily::TICKER ? api::DeliveryContract::TICKER : api::DeliveryContract::STREAM;
    result.maximumAttempts = 1;
    result.readTimeout = 5000ms;
    result.drainTimeout = 100ms;

    return result;
}

/** Observe detached state without progressing the SDK from the application thread.
 * @tparam Predicate Atomic/locked test-owned observation.
 * @param predicate Complete observation callable.
 * @return Whether the observation became true within five seconds.
 */
template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Observe synchronous refusal without retaining exception text or endpoint resources.
 * @param action One test-owned producer action.
 * @return Detached rejection category, or NONE if accepted.
 */
api::ErrorCode errorCode(const std::function<void()> &action) {
    try {
        action();
    } catch (const api::Error &error) {
        return error.diagnostic().code;
    }

    return api::ErrorCode::NONE;
}

/** Construct full standard adapters at custom dense IDs after an unrelated generic record.
 * @return Validated renamed Quote1/Profile2 declarations, with reversed complete Profile field order.
 */
core::LocalSchema customSchema() {
    auto quote = events::defaultQuoteSchema(SCHEMA_LIMITS);
    auto profile = events::defaultProfileSchema(SCHEMA_LIMITS);
    REQUIRE(quote);
    REQUIRE(profile);

    auto first = quote->records()[0];
    first.id = 1;
    first.name = u"ConfiguredQuote";
    auto second = profile->records()[0];
    second.id = 2;
    second.name = u"ConfiguredProfile";
    std::ranges::reverse(second.fields);

    for (auto &lookup : second.lookups) {
        lookup.fieldId = second.fields.size() - 1 - lookup.fieldId;
    }

    const std::array records{core::LocalRecord{0, u"Generic", false, {{8}}, {{u"Value", 0}}}, std::move(first),
                             std::move(second)};
    auto result = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Fill complete local fields with distinct business values and nullable owning text.
 * @param record Complete ordinary record, independent of field positions.
 * @param seconds Distinguishing source bid/halt time.
 * @return Exact full local-order values used by the independent wire encoder.
 */
std::vector<core::FieldValue> values(const core::LocalRecord &record, std::int64_t seconds) {
    std::vector<core::FieldValue> result;

    for (const auto &field : record.fields) {
        const auto type = static_cast<core::SerialFieldType>(field.serialTypeId);
        core::FieldValue value{type, 0};

        if (type == core::SerialFieldType::TIME_SECONDS) {
            value.raw = seconds;
        } else if (type == core::SerialFieldType::UTF_CHAR) {
            value.raw = u'X';
        } else if (type == core::SerialFieldType::WIDE_DECIMAL) {
            // Eight-bit rank128 represents an integer without changing the raw decimal identity.
            value.raw = (seconds << 8) + 128;
        } else if (type == core::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"Owned mixed description";
        }

        result.push_back(std::move(value));
    }

    return result;
}

/** Register both custom routes, deliberately in a different order from the schema.
 * @param schema Validated complete caller declarations.
 * @return Owning immutable registry snapshot.
 */
api::EventRegistry registry(const core::LocalSchema &schema) {
    const std::array routes{api::EventRegistration{api::EventType::PROFILE, 2},
                            api::EventRegistration{api::EventType::QUOTE, 1}};
    api::EventRegistryWork work;
    auto result = api::EventRegistry::create(schema, routes, {}, work);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

TEST_CASE("mixed type selection is bounded normalized atomic and immutable") {
    auto invalid = runtimeOptions();
    invalid.maximumEventTypeArgumentsPerSubscription = 0;
    CHECK(errorCode([&] {
              api::ApiRuntime rejected(invalid);
          }) == api::ErrorCode::INVALID_ARGUMENT);

    auto limits = runtimeOptions();
    limits.maximumSubscriptionsPerEndpoint = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    auto feed = endpoint->getFeed();
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription({}));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription({static_cast<api::EventType>(999)}));
          }) == api::ErrorCode::INVALID_ARGUMENT);

    std::array<api::EventType, 65> tooMany;
    tooMany.fill(static_cast<api::EventType>(999));
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription(tooMany));
          }) == api::ErrorCode::RESOURCE_LIMIT);

    std::array<api::EventType, 64> duplicate;
    duplicate.fill(api::EventType::QUOTE);
    duplicate.back() = api::EventType::PROFILE;
    auto mixed = feed->createSubscription(duplicate);
    duplicate.fill(static_cast<api::EventType>(999));
    CHECK(mixed->getState() == api::SubscriptionState::CONFIGURED);
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription<events::Quote>());
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(mixed->awaitState(api::SubscriptionState::CONFIGURED, std::chrono::steady_clock::now()).status ==
          api::WaitStatus::REACHED);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(mixed->getState() == api::SubscriptionState::CLOSED);
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription({static_cast<api::EventType>(999)}));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription({api::EventType::QUOTE}));
          }) == api::ErrorCode::CLOSED);
}

TEST_CASE("configured raw type argument bound counts duplicates before normalization") {
    auto limits = runtimeOptions();
    limits.maximumEventTypeArgumentsPerSubscription = 2;
    limits.maximumSubscriptionsPerEndpoint = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint();
    auto feed = endpoint->getFeed();
    CHECK(errorCode([&] {
              static_cast<void>(
                  feed->createSubscription({api::EventType::QUOTE, api::EventType::PROFILE, api::EventType::QUOTE}));
          }) == api::ErrorCode::RESOURCE_LIMIT);
    auto accepted = feed->createSubscription({api::EventType::PROFILE, api::EventType::PROFILE});
    CHECK(accepted->getState() == api::SubscriptionState::CONFIGURED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("missing requested type rejects the whole mixed registration without consuming its slot") {
    auto source = events::defaultQuoteSchema(SCHEMA_LIMITS);
    REQUIRE(source);
    const api::EventRegistration route{api::EventType::QUOTE, 0};
    api::EventRegistryWork work;
    auto configured = api::EventRegistry::create(*source, std::span(&route, 1), {}, work);
    REQUIRE(configured);
    auto limits = runtimeOptions();
    limits.maximumSubscriptionsPerEndpoint = 1;
    api::ApiRuntime runtime(limits);
    api::EndpointOptions options;
    options.eventRegistry = *configured;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    CHECK(errorCode([&] {
              static_cast<void>(feed->createSubscription({api::EventType::QUOTE, api::EventType::PROFILE}));
          }) == api::ErrorCode::UNSUPPORTED_PROFILE);
    auto valid = feed->createSubscription({api::EventType::QUOTE, api::EventType::QUOTE, api::EventType::QUOTE});
    CHECK(valid->getState() == api::SubscriptionState::CONFIGURED);
    static_cast<void>(endpoint->closeAndAwaitTermination());
}

TEST_CASE("one mixed logical subscription delivers owning custom Quote and Profile variants") {
    for (const auto family : {core::SubscriptionFamily::STREAM, core::SubscriptionFamily::TICKER}) {
        auto source = customSchema();
        auto options = endpointOptions(family);
        options.eventRegistry = registry(source);
        auto limits = runtimeOptions();
        limits.maximumSubscriptionsPerEndpoint = 1;
        limits.maximumSymbolsPerSubscription = 1;
        api::ApiRuntime runtime(limits);
        auto endpoint = runtime.createEndpoint(options);
        auto mixed = endpoint->getFeed()->createSubscription(
            {api::EventType::PROFILE, api::EventType::QUOTE, api::EventType::QUOTE});
        CHECK(errorCode([&] {
                  static_cast<void>(endpoint->getFeed()->createSubscription<events::Profile>());
              }) == api::ErrorCode::RESOURCE_LIMIT);
        peer::PeerScript script;
        script.family = family;
        script.canonicalSymbols = true;
        script.describeAllRecords = true;
        script.subscriptionRecordIds = {1, 2};
        const std::array records{peer::Record{u"MIX", values(source.records()[2], 7), 0, {}, 2},
                                 peer::Record{u"MIX", values(source.records()[1], 3), 0, {}, 1}};
        const std::array<std::u16string, 2> expected{u"MIX", u"MIX"};
        peer::Peer sourcePeer(source, records, expected, 32, peer::PeerMode::HOLD, script);
        const auto observation = std::make_shared<Observation>();
        const std::weak_ptr<api::Subscription<api::RegisteredEvent>> weak = mixed;
        mixed->addEventListener([observation, weak](std::span<const api::RegisteredEvent> batch) {
            if (const auto current = weak.lock()) {
                try {
                    static_cast<void>(
                        current->awaitState(api::SubscriptionState::ACTIVE, std::chrono::steady_clock::now()));
                } catch (const api::Error &error) {
                    if (error.diagnostic().code == api::ErrorCode::WRONG_THREAD) {
                        ++observation->wrongThread;
                    }
                }
            }

            std::lock_guard lock(observation->mutex);
            observation->mixed.insert(observation->mixed.end(), batch.begin(), batch.end());
            observation->mixedCount.store(static_cast<unsigned>(observation->mixed.size()), std::memory_order_release);
        });
        mixed->addSymbols("MIX");
        endpoint->connect("127.0.0.1:" + std::to_string(sourcePeer.port()));
        REQUIRE(eventually([&] {
            return observation->mixedCount.load() == 2 || sourcePeer.failed() ||
                   mixed->getDiagnostic().code != api::ErrorCode::NONE;
        }));
        CHECK_FALSE(sourcePeer.failed());
        CHECK(sourcePeer.adds() == 1);
        CHECK(mixed->getDiagnostic().code == api::ErrorCode::NONE);
        REQUIRE(observation->mixedCount.load() == 2);
        CHECK(observation->wrongThread.load() != 0);
        static_cast<void>(endpoint->closeAndAwaitTermination());
        std::lock_guard lock(observation->mutex);
        unsigned quotes = 0;
        unsigned profiles = 0;

        for (const auto &event : observation->mixed) {
            if (const auto *quote = std::get_if<events::Quote>(&event)) {
                ++quotes;
                CHECK(quote->symbol == u"MIX");
                CHECK(quote->bidTimeMs == 3000);
                CHECK(quote->bidPrice.toDouble() == 3.0);
            } else {
                ++profiles;
                const auto &profile = std::get<events::Profile>(event);
                CHECK(profile.symbol == u"MIX");
                REQUIRE(profile.description);
                CHECK(*profile.description == u"Owned mixed description");
                CHECK(profile.haltStartTimeMs == 7000);
            }
        }

        CHECK(quotes == 1);
        CHECK(profiles == 1);
    }
}

TEST_CASE("closing mixed overlap withdraws only its exclusive route and preserves the concrete client") {
    auto source = customSchema();
    auto options = endpointOptions(core::SubscriptionFamily::STREAM);
    options.eventRegistry = registry(source);
    auto limits = runtimeOptions();
    limits.maximumSubscriptionsPerEndpoint = 2;
    limits.maximumSymbolsPerSubscription = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(options);
    auto mixed = endpoint->getFeed()->createSubscription({api::EventType::QUOTE, api::EventType::PROFILE});
    auto concrete = endpoint->getFeed()->createSubscription<events::Quote>();
    peer::PeerScript script;
    script.family = core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = true;
    script.describeAllRecords = true;
    script.subscriptionRecordIds = {1, 2};
    script.removeSequence =
        std::vector{peer::ExpectedSubscription{{u"MIX"}, {2}}, peer::ExpectedSubscription{{u"MIX"}, {1}}};
    script.afterRemove = std::vector{peer::Record{u"MIX", values(source.records()[1], 9), 0, {}, 1},
                                     peer::Record{u"MIX", values(source.records()[2], 11), 0, {}, 2}};
    const std::array records{peer::Record{u"MIX", values(source.records()[1], 3), 0, {}, 1},
                             peer::Record{u"MIX", values(source.records()[2], 7), 0, {}, 2}};
    const std::array<std::u16string, 2> expected{u"MIX", u"MIX"};
    peer::Peer sourcePeer(source, records, expected, 32, peer::PeerMode::HOLD, script);
    const auto observation = std::make_shared<Observation>();
    mixed->addEventListener([observation](std::span<const api::RegisteredEvent> batch) {
        std::lock_guard lock(observation->mutex);
        observation->mixed.insert(observation->mixed.end(), batch.begin(), batch.end());
        observation->mixedCount.store(static_cast<unsigned>(observation->mixed.size()));
    });
    concrete->addEventListener([observation](std::span<const events::Quote> batch) {
        observation->quotes.fetch_add(static_cast<unsigned>(batch.size()));
    });
    mixed->addSymbols("MIX");
    concrete->addSymbols("MIX");
    endpoint->connect("127.0.0.1:" + std::to_string(sourcePeer.port()));
    REQUIRE(eventually([&] {
        return observation->mixedCount.load() == 2 && observation->quotes.load() == 1;
    }));
    CHECK_FALSE(sourcePeer.failed());
    mixed->close();
    CHECK(mixed->awaitState(api::SubscriptionState::CLOSED, std::chrono::steady_clock::now() + 5s).status ==
          api::WaitStatus::REACHED);
    REQUIRE(eventually([&] {
        return sourcePeer.removes() == 1 && observation->quotes.load() == 2;
    }));
    CHECK_FALSE(sourcePeer.failed());
    CHECK(observation->mixedCount.load() == 2);
    CHECK(concrete->getState() == api::SubscriptionState::ACTIVE);
    CHECK(concrete->getDiagnostic().code == api::ErrorCode::NONE);
    concrete->clearSymbols();
    REQUIRE(eventually([&] {
        return sourcePeer.removes() == 2 || sourcePeer.failed();
    }));
    CHECK_FALSE(sourcePeer.failed());
    static_cast<void>(endpoint->closeAndAwaitTermination());
}

TEST_CASE("mixed creation after connect isolates a throwing selected listener from concrete overlap") {
    auto source = customSchema();
    auto options = endpointOptions(core::SubscriptionFamily::STREAM);
    options.eventRegistry = registry(source);
    auto limits = runtimeOptions();
    limits.maximumSubscriptionsPerEndpoint = 2;
    limits.maximumSymbolsPerSubscription = limits.maximumListenersPerSubscription = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(options);
    auto concrete = endpoint->getFeed()->createSubscription<events::Quote>();
    peer::PeerScript script;
    script.family = core::SubscriptionFamily::STREAM;
    script.canonicalSymbols = script.describeAllRecords = true;
    script.subscriptionRecordIds = {1, 2};
    script.removeSequence =
        std::vector{peer::ExpectedSubscription{{u"MIX"}, {2}}, peer::ExpectedSubscription{{u"MIX"}, {1}}};
    const std::array records{peer::Record{u"MIX", values(source.records()[1], 3), 0, {}, 1},
                             peer::Record{u"MIX", values(source.records()[2], 7), 0, {}, 2}};
    const std::array<std::u16string, 2> expected{u"MIX", u"MIX"};
    peer::Peer sourcePeer(source, records, expected, 32, peer::PeerMode::HOLD, script);
    endpoint->connect("127.0.0.1:" + std::to_string(sourcePeer.port()));
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 5s).status ==
            api::WaitStatus::REACHED);
    auto mixed = endpoint->getFeed()->createSubscription({api::EventType::QUOTE, api::EventType::PROFILE});
    const auto observation = std::make_shared<Observation>();
    const auto selectedLimit = std::make_shared<std::atomic<api::ErrorCode>>(api::ErrorCode::NONE);
    const std::weak_ptr<api::Subscription<api::RegisteredEvent>> weak = mixed;
    const auto listenerId = std::make_shared<api::Subscription<api::RegisteredEvent>::ListenerId>(0);
    *listenerId = mixed->addEventListener([weak, selectedLimit, listenerId](std::span<const api::RegisteredEvent>) {
        if (const auto current = weak.lock()) {
            current->removeEventListener(*listenerId);
            selectedLimit->store(errorCode([&] {
                current->addEventListener([](std::span<const api::RegisteredEvent>) {
                });
            }));
        }

        throw std::runtime_error("test-owned mixed listener failure");
    });
    concrete->addEventListener([observation](std::span<const events::Quote> batch) {
        observation->quotes.fetch_add(static_cast<unsigned>(batch.size()));
    });
    mixed->addSymbols("MIX");
    concrete->addSymbols("MIX");
    REQUIRE(eventually([&] {
        return observation->quotes.load() == 1 && mixed->getDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE;
    }));
    REQUIRE(eventually([&] {
        return sourcePeer.removes() == 1 || sourcePeer.failed();
    }));
    CHECK_FALSE(sourcePeer.failed());
    CHECK(selectedLimit->load() == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(concrete->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    concrete->clearSymbols();
    REQUIRE(eventually([&] {
        return sourcePeer.removes() == 2 || sourcePeer.failed();
    }));
    CHECK_FALSE(sourcePeer.failed());
    CHECK(sourcePeer.removes() == 2);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(mixed->getState() == api::SubscriptionState::FAILED);
}

TEST_CASE("mixed final listener capture retires outside locks and rejects recursive waits") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint();
    auto mixed = endpoint->getFeed()->createSubscription({api::EventType::QUOTE, api::EventType::PROFILE});
    auto capture = std::make_shared<CaptureAction>();
    api::ErrorCode endpointError = api::ErrorCode::NONE;
    api::ErrorCode runtimeError = api::ErrorCode::NONE;
    bool retired = false;
    capture->action = [&] {
        endpointError = errorCode([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        runtimeError = errorCode([&] {
            runtime.closeAndAwaitTermination();
        });
        retired = true;
    };
    const auto listener = mixed->addEventListener([capture](std::span<const api::RegisteredEvent>) {
    });
    capture.reset();
    mixed->removeEventListener(listener);
    CHECK(retired);
    CHECK(endpointError == api::ErrorCode::INVALID_STATE);
    CHECK(runtimeError == api::ErrorCode::INVALID_STATE);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(mixed->getState() == api::SubscriptionState::CLOSED);
}

TEST_CASE("mixed complete plan prepays cross-kind duplicate checks at the sixteen-route boundary") {
    std::vector<core::LocalRecord> records;
    std::vector<api::EventRegistration> registrations;

    for (std::int32_t index = 0; index < 16; ++index) {
        const auto quote = index % 2 == 0;
        auto name = u"Record" + std::u16string(1, static_cast<char16_t>(u'A' + index));

        if (quote) {
            records.push_back({index,
                               std::move(name),
                               false,
                               {{8}, {8}, {8}, {8}},
                               {{u"BidPrice", 0}, {u"BidSize", 1}, {u"AskPrice", 2}, {u"AskSize", 3}}});
        } else {
            records.push_back({index, std::move(name), false, {{10}}, {{u"Description", 0}}});
        }

        registrations.push_back({quote ? api::EventType::QUOTE : api::EventType::PROFILE, index});
    }

    auto source = core::LocalSchema::create(records, SCHEMA_LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(source);
    api::EventRegistryWork work;
    auto configured = api::EventRegistry::create(*source, registrations, {}, work);
    REQUIRE(configured);
    auto runtime = runtimeOptions();
    runtime.maximumSubscriptionsPerEndpoint = runtime.maximumSymbolsPerSubscription = 1;
    api::EndpointOptions options;
    options.eventRegistry = *configured;
    auto profile = api::detail::prepareReceivingProfile(runtime, options, "127.0.0.1", 12345);
    CHECK(profile.eventPlans[2].bindingCount == 16);
    CHECK(profile.eventPlans[2].work.visits >= 153);
    CHECK(profile.clientLimits.commandEntries == 16);
    CHECK(profile.endpoint.settings().budgets.desired.entries == 16);
    auto queueWork = profile.queueWork;
    auto queue = api::detail::createReceivingQueue(profile, queueWork);
    REQUIRE(queue);

    /** Native-free consumer used to verify actual mixed receiver preparation, not only its arithmetic. */
    class Consumer final : public root::client::SubscriptionConsumer {
        public:
        /** Acknowledge any whole batch without retaining its borrow.
         * @param batch Callback-only generic values.
         * @return Continue ordinary queue delivery.
         */
        root::mux::BatchAction onBatch(const root::client::SubscriptionBatch &batch) override {
            static_cast<void>(batch);

            return root::mux::BatchAction::CONTINUE;
        }
    };

    auto preparation = profile.eventPlans[2].work;
    auto consumer = std::make_shared<Consumer>();
    auto handle = root::client::openSubscriptionReceiver(
        *queue, profile.schema, profile.family, core::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1, profile.bindings,
        consumer, profile.eventPlans[2].limits, profile.clientLimits, preparation, queueWork);
    REQUIRE(handle);
}
