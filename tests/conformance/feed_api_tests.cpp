// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public API, finite controlled peers, no external endpoint or ambient resolver.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/feed.hpp"
#include "qd/transport/recovery.hpp"

#include "connect_peer.hpp"

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>

namespace {
namespace root = org::ttldtor::qdcxx;
namespace api = root::api;
namespace core = root::core;
namespace events = root::events;
namespace peer = root::connect_tests;
using namespace std::chrono_literals;

/** Explicit resolver spelling is unused by numeric loopback tests and avoids ambient system discovery.
 * @return Finite ordinary runtime defaults with one explicit numeric DNS server.
 */
api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

/** Use one attempt so a deterministic peer fault cannot hide behind unrelated retry timing.
 * @param contract Selected existing collector semantics.
 * @return Finite controlled deadlines and one native attempt.
 */
api::EndpointOptions endpointOptions(api::DeliveryContract contract = api::DeliveryContract::TICKER) {
    api::EndpointOptions result;
    result.contract = contract;
    result.maximumAttempts = 1;
    result.readTimeout = 3000ms;
    result.drainTimeout = 100ms;

    return result;
}

/** Observe a bounded asynchronous condition without driving any SDK work.
 * @tparam Predicate Test-owned observation callable.
 * @param predicate Atomic or locked detached observation.
 * @param timeout Explicit whole wait ceiling.
 * @return Whether the condition became true within that ceiling.
 */
template <class Predicate> bool eventually(Predicate predicate, std::chrono::milliseconds timeout = 3000ms) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

/** Capture a synchronous public rejection without inspecting arbitrary exception text.
 * @param action Requested public operation.
 * @return Detached high-level category, or NONE if the operation succeeded.
 */
api::ErrorCode errorCode(const std::function<void()> &action) {
    try {
        action();
    } catch (const api::Error &error) {
        return error.diagnostic().code;
    }

    return api::ErrorCode::NONE;
}

/** Construct the complete unchanged standard declaration used by the independent peer encoder.
 * @param profile Select Profile rather than Quote.
 * @return Complete binary schema with all eight/sixteen fields and every existing alias.
 */
core::LocalSchema standardSchema(bool profile = false) {
    constexpr core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto builtin = profile ? events::defaultProfileSchema(LIMITS) : events::defaultQuoteSchema(LIMITS);
    REQUIRE(builtin);
    auto schema = core::LocalSchema::create(builtin->records(), LIMITS, core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

/** Fill the complete standard record with exact values, preserving every field type.
 * @param schema Complete peer declaration.
 * @return Whole source field vector, with recognizable times, exchange code and owned text.
 */
std::vector<core::FieldValue> standardValues(const core::LocalSchema &schema) {
    std::vector<core::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        const auto type = static_cast<core::SerialFieldType>(field.serialTypeId);
        core::FieldValue value{type, 0};

        if (type == core::SerialFieldType::UTF_CHAR) {
            value.raw = u'Q';
        } else if (type == core::SerialFieldType::TIME_SECONDS) {
            value.raw = 3;
        } else if (type == core::SerialFieldType::UTF_CHAR_ARRAY) {
            value.units = u"retained sample";
        }

        result.push_back(std::move(value));
    }

    return result;
}

/** Select canonical test wire symbols and optionally hold initial DATA behind a test-owned barrier.
 * @param contract Existing source family.
 * @param waitForRelease Whether the peer waits for releaseData after parsing actual ADD.
 * @return Explicit independent peer script.
 */
peer::PeerScript script(core::SubscriptionFamily contract = core::SubscriptionFamily::TICKER,
                        bool waitForRelease = false) {
    peer::PeerScript result;
    result.family = contract;
    result.canonicalSymbols = true;
    result.waitForDataRelease = waitForRelease;

    return result;
}

/** Build the exact numeric loopback spelling from the owned listener.
 * @param source Test-owned bounded native peer.
 * @return Plain public-API address without DNS.
 */
std::string address(const peer::Peer &source) {
    return "127.0.0.1:" + std::to_string(source.port());
}

/** Test-owned final-capture hook; construction never copies or runs the configured action. */
struct CaptureAction {
    std::function<void()> action; ///< Explicit finite release barrier or reentry probe.

    /** Run the configured test action only when the final shared capture is actually destroyed. */
    ~CaptureAction() {
        if (action) {
            action();
        }
    }
};

/** Verify actual public terminal state while preserving exact existing retirement diagnostics.
 * A recorded read failure remains FAILED; the test never relabels it as a clean cancellation.
 * @param endpoint Endpoint after creator-thread await or joined runtime shutdown.
 */
void checkRetiredEndpoint(const std::shared_ptr<api::Endpoint> &endpoint) {
    const auto diagnostic = endpoint->getDiagnostic();
    CAPTURE(diagnostic.stage);
    CAPTURE(diagnostic.nativeCode);
    CAPTURE(diagnostic.detailCode);

    if (diagnostic.stage == "recovery-peer-end" && diagnostic.nativeCode == 0) {
        CHECK(endpoint->getState() == api::EndpointState::FAILED);
        CHECK(diagnostic.code == api::ErrorCode::TRANSPORT_FAILURE);
        CHECK(diagnostic.detailCode == static_cast<std::uint32_t>(root::transport::RecoveryError::TRANSPORT_FAILURE));

        return;
    }

#ifdef _WIN32

    if (diagnostic.nativeCode == 1236) {
        CHECK(endpoint->getState() == api::EndpointState::FAILED);
        CHECK(diagnostic.code == api::ErrorCode::TRANSPORT_FAILURE);
        CHECK(diagnostic.stage == "socket-read");
        CHECK(diagnostic.detailCode == static_cast<std::uint32_t>(root::transport::RecoveryError::TRANSPORT_FAILURE));

        return;
    }

#endif
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(diagnostic.code == api::ErrorCode::NONE);
}
} // namespace

TEST_CASE("public feed bounds atomic desired sets listeners and inactive retirement") {
    auto options = runtimeOptions();
    options.maximumEndpoints = 1;
    options.maximumSubscriptionsPerEndpoint = 1;
    options.maximumSymbolsPerSubscription = 2;
    options.maximumListenersPerSubscription = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> calls{0};
    const auto id = subscription->addEventListener([&](std::span<const events::Quote>) {
        ++calls;
    });
    CHECK(calls.load() == 0);
    CHECK(errorCode([&] {
              subscription->addEventListener([](std::span<const events::Quote>) {
              });
          }) == api::ErrorCode::RESOURCE_LIMIT);
    subscription->removeEventListener(id);
    subscription->addEventListener([](std::span<const events::Quote>) {
    });
    subscription->setSymbols({"IBM", "AAPL"});
    CHECK(errorCode([&] {
              subscription->addSymbols("MSFT");
          }) == api::ErrorCode::RESOURCE_LIMIT);
    subscription->removeSymbols("IBM");
    subscription->addSymbols("MSFT");
    CHECK(errorCode([&] {
              subscription->setSymbols({"VALID", "*"});
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              static_cast<void>(endpoint->getFeed()->createSubscription<events::Profile>());
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(errorCode([&] {
              static_cast<void>(runtime.createEndpoint());
          }) == api::ErrorCode::RESOURCE_LIMIT);
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.drained);
    CHECK(subscription->getState() == api::SubscriptionState::CLOSED);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(errorCode([&] {
              subscription->clearSymbols();
          }) == api::ErrorCode::CLOSED);
    auto replacement = runtime.createEndpoint(endpointOptions());
    CHECK(replacement->getState() == api::EndpointState::NOT_CONNECTED);
    runtime.closeAndAwaitTermination();
}

TEST_CASE("public feed rejects foreign thread waits and preserves detached closed ports") {
    std::shared_ptr<api::Endpoint> endpoint;
    std::shared_ptr<api::Subscription<events::Quote>> subscription;
    {
        api::ApiRuntime runtime(runtimeOptions());
        endpoint = runtime.createEndpoint(endpointOptions());
        subscription = endpoint->getFeed()->createSubscription<events::Quote>();
        std::atomic<api::ErrorCode> endpointError{api::ErrorCode::NONE};
        std::atomic<api::ErrorCode> runtimeError{api::ErrorCode::NONE};
        std::thread producer([&] {
            endpointError = errorCode([&] {
                static_cast<void>(endpoint->closeAndAwaitTermination());
            });
            runtimeError = errorCode([&] {
                runtime.closeAndAwaitTermination();
            });
        });
        producer.join();
        CHECK(endpointError.load() == api::ErrorCode::WRONG_THREAD);
        CHECK(runtimeError.load() == api::ErrorCode::WRONG_THREAD);
        CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
        runtime.closeAndAwaitTermination();
    }

    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(subscription->getState() == api::SubscriptionState::CLOSED);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(errorCode([&] {
              subscription->addSymbols("IBM");
          }) == api::ErrorCode::CLOSED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public feed delivers complete owning Quote and Profile values without application progress") {
    for (const bool profile : {false, true}) {
        api::ApiRuntime runtime(runtimeOptions());
        auto schema = standardSchema(profile);
        const std::vector<std::u16string> symbols{u"IBM"};
        const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
        auto peerScript = script();
        peerScript.subscriptionRecordId = profile ? 1 : 0;
        peer::Peer source(schema, records, symbols, 64, peer::PeerMode::HOLD, peerScript);
        auto endpoint = runtime.createEndpoint(endpointOptions());
        std::atomic<unsigned> received{0};
        std::optional<events::Quote> quote;
        std::optional<events::Profile> company;
        std::mutex observations;

        if (profile) {
            auto subscription = endpoint->getFeed()->createSubscription<events::Profile>();
            subscription->addEventListener([&](std::span<const events::Profile> values) {
                std::lock_guard lock(observations);
                company = values.front();
                ++received;
            });
            subscription->addSymbols("IBM");
            endpoint->connect(address(source));
            CHECK(eventually([&] {
                return received.load() != 0;
            }));
            CHECK(endpoint->getDiagnostic().stage == "none");
            CHECK(subscription->getDiagnostic().stage == "none");
            CHECK(source.adds() == 1);
            CHECK_FALSE(source.failed());
            static_cast<void>(endpoint->closeAndAwaitTermination());
        } else {
            auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
            subscription->addEventListener([&](std::span<const events::Quote> values) {
                std::lock_guard lock(observations);
                quote = values.front();
                ++received;
            });
            subscription->addSymbols("IBM");
            endpoint->connect(address(source));
            CHECK(eventually([&] {
                return received.load() != 0;
            }));
            CHECK(endpoint->getDiagnostic().stage == "none");
            CHECK(subscription->getDiagnostic().stage == "none");
            CHECK(source.adds() == 1);
            CHECK_FALSE(source.failed());
            static_cast<void>(endpoint->closeAndAwaitTermination());
        }

        runtime.closeAndAwaitTermination();

        if (profile) {
            REQUIRE(company);
            CHECK(company->symbol == u"IBM");
            CHECK(company->description == u"retained sample");
        } else {
            REQUIRE(quote);
            CHECK(quote->symbol == u"IBM");
            CHECK(quote->bidTimeMs == 3000);
            CHECK(quote->askExchangeCode == u'Q');
        }

        CHECK(source.adds() == 1);
        CHECK_FALSE(source.failed());
    }
}

TEST_CASE("public feed supports registration after connect and independent overlapping clients") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    endpoint->connect(address(source));
    endpoint->connect(address(source));
    CHECK(errorCode([&] {
              endpoint->connect("127.0.0.1:1");
          }) == api::ErrorCode::INVALID_STATE);
    REQUIRE(eventually([&] {
        return endpoint->getState() == api::EndpointState::CONNECTED ||
               endpoint->getDiagnostic().code != api::ErrorCode::NONE;
    }));
    CAPTURE(endpoint->getDiagnostic().stage);
    CAPTURE(endpoint->getDiagnostic().detailCode);
    REQUIRE(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    auto first = endpoint->getFeed()->createSubscription<events::Quote>();
    auto second = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> firstCount{0};
    std::atomic<unsigned> secondCount{0};
    first->addEventListener([&](std::span<const events::Quote>) {
        ++firstCount;
    });
    second->addEventListener([&](std::span<const events::Quote>) {
        ++secondCount;
    });
    first->addSymbols("IBM");
    second->addSymbols("IBM");
    CHECK(eventually([&] {
        return firstCount.load() && secondCount.load();
    }));
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(first->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(second->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(source.adds() == 1);
    CHECK_FALSE(source.failed());
}

TEST_CASE("public feed isolates listener failure and keeps selected listener slots charged") {
    auto limits = runtimeOptions();
    limits.maximumListenersPerSubscription = 1;
    api::ApiRuntime runtime(limits);
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto broken = endpoint->getFeed()->createSubscription<events::Quote>();
    auto healthy = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> received{0};
    std::atomic<api::ErrorCode> selectedLimit{api::ErrorCode::NONE};
    std::uint64_t listener = 0;
    listener = broken->addEventListener([&, broken](std::span<const events::Quote>) {
        broken->removeEventListener(listener);
        selectedLimit = errorCode([&] {
            broken->addEventListener([](std::span<const events::Quote>) {
            });
        });
        throw std::runtime_error("test-owned listener failure");
    });
    healthy->addEventListener([&](std::span<const events::Quote>) {
        ++received;
    });
    broken->addSymbols("IBM");
    healthy->addSymbols("IBM");
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return received.load() && broken->getDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE;
    }));
    CHECK(selectedLimit.load() == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(healthy->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(broken->getState() == api::SubscriptionState::FAILED);
}

TEST_CASE("public feed callback rejects await and may release all endpoint producer ownership") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::weak_ptr<api::Endpoint> weak = endpoint;
    std::atomic<bool> returned{false};
    std::atomic<api::ErrorCode> awaitError{api::ErrorCode::NONE};
    subscription->addEventListener([endpoint, &awaitError, &returned](std::span<const events::Quote>) mutable {
        awaitError = errorCode([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        endpoint->close();
        endpoint.reset();
        returned = true;
    });
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    endpoint.reset();
    CHECK(eventually([&] {
        return returned.load();
    }));
    CHECK(awaitError.load() == api::ErrorCode::WRONG_THREAD);
    runtime.closeAndAwaitTermination();
    CHECK(weak.expired());
    CHECK(subscription->getState() == api::SubscriptionState::CLOSED);
}

TEST_CASE("public feed STREAM preserves equal consecutive complete records") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}, {u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script(core::SubscriptionFamily::STREAM));
    auto endpoint = runtime.createEndpoint(endpointOptions(api::DeliveryContract::STREAM));
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> recordsSeen{0};
    subscription->addEventListener([&](std::span<const events::Quote> values) {
        recordsSeen += static_cast<unsigned>(values.size());
    });
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return recordsSeen.load() == 2;
    }));
    CHECK(endpoint->getDiagnostic().stage == "none");
    CHECK(subscription->getDiagnostic().stage == "none");
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK_FALSE(source.failed());
}

TEST_CASE("public feed callback clear preserves the whole batch and applies the actual removal barrier") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}, {u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script(core::SubscriptionFamily::STREAM));
    auto endpoint = runtime.createEndpoint(endpointOptions(api::DeliveryContract::STREAM));
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> recordsSeen{0};
    subscription->addEventListener([&, subscription](std::span<const events::Quote> values) {
        recordsSeen += static_cast<unsigned>(values.size());
        subscription->clearSymbols();
    });

    // Frontend mutations coalesce into a bounded complete snapshot before any native command exists.
    for (unsigned index = 0; index < 1000; ++index) {
        subscription->setSymbols({"IGNORED"});
        subscription->setSymbols({"IBM", "IBM"});
    }

    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return source.removes() == 1 && source.afterRemoveSent();
    }));
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(recordsSeen.load() == 2);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK_FALSE(source.failed());
}

TEST_CASE("public feed shares one runtime across independent native endpoints") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer firstSource(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    peer::Peer secondSource(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto firstEndpoint = runtime.createEndpoint(endpointOptions());
    auto secondEndpoint = runtime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> firstCount{0};
    std::atomic<unsigned> secondCount{0};
    first->addEventListener([&](std::span<const events::Quote> values) {
        firstCount += static_cast<unsigned>(values.size());
    });
    second->addEventListener([&](std::span<const events::Quote> values) {
        secondCount += static_cast<unsigned>(values.size());
    });
    first->addSymbols("IBM");
    second->addSymbols("IBM");
    firstEndpoint->connect(address(firstSource));
    secondEndpoint->connect(address(secondSource));
    CHECK(eventually([&] {
        return firstCount.load() && secondCount.load();
    }));
    CHECK(firstEndpoint->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(secondEndpoint->getDiagnostic().code == api::ErrorCode::NONE);
    static_cast<void>(firstEndpoint->closeAndAwaitTermination());
    CHECK(secondEndpoint->getState() == api::EndpointState::CONNECTED);
    CHECK(secondEndpoint->getDiagnostic().code == api::ErrorCode::NONE);
    runtime.closeAndAwaitTermination();
    checkRetiredEndpoint(secondEndpoint);
    CHECK_FALSE(firstSource.failed());
    CHECK_FALSE(secondSource.failed());
}

TEST_CASE("public feed close retains ownership while an application callback blocks") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto policy = endpointOptions();
    policy.drainTimeout = 10ms;
    auto endpoint = runtime.createEndpoint(policy);
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> entered{false};
    std::atomic<bool> callbackReturned{false};
    std::atomic<bool> awaitReturned{false};
    std::atomic<bool> prematureReturn{false};
    subscription->addEventListener([&](std::span<const events::Quote>) {
        entered = true;
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] {
            return release;
        });
        callbackReturned = true;
    });
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    const auto callbackEntered = eventually([&] {
        return entered.load();
    });
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);

    // This test-owned helper always releases the callback; no failure path can leave a blocked worker.
    std::thread releaser([&] {
        std::this_thread::sleep_for(50ms);
        prematureReturn = awaitReturned.load();
        {
            std::lock_guard lock(mutex);
            release = true;
        }

        condition.notify_one();
    });
    const auto terminal = endpoint->closeAndAwaitTermination();
    awaitReturned = true;
    releaser.join();
    CHECK(callbackEntered);
    CHECK(callbackReturned.load());
    CHECK_FALSE(prematureReturn.load());
    CHECK(terminal.deadlineExpired);
    CHECK_FALSE(terminal.drained);
    checkRetiredEndpoint(endpoint);
}

TEST_CASE("public feed inactive retirement rejects nested capture awaits while another endpoint starts the worker") {
    api::ApiRuntime runtime(runtimeOptions());
    auto firstEndpoint = runtime.createEndpoint(endpointOptions());
    auto nestedEndpoint = runtime.createEndpoint(endpointOptions());
    auto activeEndpoint = runtime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto nested = nestedEndpoint->getFeed()->createSubscription<events::Quote>();
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    std::atomic<api::ErrorCode> nestedError{api::ErrorCode::NONE};
    std::atomic<api::ErrorCode> runtimeError{api::ErrorCode::NONE};
    std::atomic<bool> connectedDuringCleanup{false};
    auto nestedCapture = std::make_shared<CaptureAction>();
    nestedCapture->action = [&] {
        nestedError = errorCode([&] {
            static_cast<void>(firstEndpoint->closeAndAwaitTermination());
        });
    };
    nested->addEventListener([nestedCapture](std::span<const events::Quote>) {
    });
    nestedCapture.reset();
    auto firstCapture = std::make_shared<CaptureAction>();
    firstCapture->action = [&] {
        static_cast<void>(nestedEndpoint->closeAndAwaitTermination());
        std::thread producer([&] {
            activeEndpoint->connect(address(source));
        });
        producer.join();
        connectedDuringCleanup = eventually([&] {
            return activeEndpoint->getState() == api::EndpointState::CONNECTED;
        });
        runtimeError = errorCode([&] {
            runtime.closeAndAwaitTermination();
        });
    };
    first->addEventListener([firstCapture](std::span<const events::Quote>) {
    });
    firstCapture.reset();
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(nestedEndpoint->getState() == api::EndpointState::CLOSED);
    CHECK(nestedError.load() == api::ErrorCode::INVALID_STATE);
    CHECK(runtimeError.load() == api::ErrorCode::INVALID_STATE);
    CHECK(connectedDuringCleanup.load());
    runtime.closeAndAwaitTermination();
}

TEST_CASE("public feed await includes a producer-held removed listener capture") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD,
                      script(core::SubscriptionFamily::TICKER, true));
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> destructorEntered{false};
    std::atomic<bool> destructorReturned{false};
    std::atomic<bool> awaitReturned{false};
    std::atomic<bool> prematureReturn{false};
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        destructorEntered = true;
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] {
            return release;
        });
        destructorReturned = true;
    };
    const auto listener = subscription->addEventListener([capture](std::span<const events::Quote>) {
    });
    capture.reset();
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return source.adds() == 1;
    }));
    std::thread producer([&] {
        subscription->removeEventListener(listener);
    });
    const auto entered = eventually([&] {
        return destructorEntered.load();
    });
    std::thread releaser([&] {
        std::this_thread::sleep_for(50ms);
        prematureReturn = awaitReturned.load();
        {
            std::lock_guard lock(mutex);
            release = true;
        }

        condition.notify_one();
    });
    static_cast<void>(endpoint->closeAndAwaitTermination());
    awaitReturned = true;
    releaser.join();
    producer.join();
    CHECK(entered);
    CHECK(destructorReturned.load());
    CHECK_FALSE(prematureReturn.load());
    CHECK(subscription->getState() == api::SubscriptionState::CLOSED);
}

TEST_CASE("public feed producer-side final capture destruction cannot await its own reservation") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    api::ErrorCode endpointError = api::ErrorCode::NONE;
    api::ErrorCode runtimeError = api::ErrorCode::NONE;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        endpointError = errorCode([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        runtimeError = errorCode([&] {
            runtime.closeAndAwaitTermination();
        });
    };
    const auto listener = subscription->addEventListener([capture](std::span<const events::Quote>) {
    });
    capture.reset();
    subscription->removeEventListener(listener);
    CHECK(endpointError == api::ErrorCode::INVALID_STATE);
    CHECK(runtimeError == api::ErrorCode::INVALID_STATE);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public feed default drain allowance retires a controlled admitted Quote prefix") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto policy = endpointOptions();
    policy.drainTimeout = api::EndpointOptions{}.drainTimeout;
    auto endpoint = runtime.createEndpoint(policy);
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::atomic<unsigned> received{0};
    subscription->addEventListener([&](std::span<const events::Quote> values) {
        received += static_cast<unsigned>(values.size());
    });
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return received.load() != 0;
    }));
    const auto started = std::chrono::steady_clock::now();
    const auto result = endpoint->closeAndAwaitTermination();
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
    CAPTURE(elapsed.count());
    CAPTURE(result.deadlineExpired);
    CAPTURE(result.hadGap);
    CAPTURE(result.diagnostic.stage);
    CAPTURE(result.diagnostic.nativeCode);
    CHECK(result.drained);
    CHECK_FALSE(result.deadlineExpired);
    CHECK_FALSE(result.hadGap);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK_FALSE(source.failed());
}
