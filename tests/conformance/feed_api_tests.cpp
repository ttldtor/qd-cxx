// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: public API, finite controlled peers, no external endpoint or ambient resolver.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/api/endpoint_configuration.hpp"
#include "qd/api/feed.hpp"
#include "qd/api/feed_filter.hpp"
#include "qd/api/statistics.hpp"
#include "qd/transport/recovery.hpp"

#include "connect_peer.hpp"
#include "recovery_test_peer.hpp"

#include "qd/encoding/compact.hpp"
#include "qd/encoding/symbol_reader.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

/** Test-executable-only allocation faults; production targets have no fault hook. */
namespace allocation_probe {
struct WorkerFault;
thread_local bool failNext = false;         ///< One creator-thread owned-storage refusal, reset before throwing.
std::atomic<WorkerFault *> worker{nullptr}; ///< Explicit one-worker scenario, never armed during ordinary tests.

/** Bounded scheduling barrier at the sole worker's first allocation, before it owns any native state. */
struct WorkerFault {
    org::ttldtor::qdcxx::api::ApiRuntime &runtime;      ///< Main-owned runtime, joined before this fault state dies.
    std::thread::id owner = std::this_thread::get_id(); ///< Creator allocations must never consume the worker fault.
    std::atomic<bool> claimed{false}; ///< Exactly one foreign allocation receives the injected failure.
    std::atomic<bool> entered{false}; ///< Worker has reached the failure barrier.
    std::atomic<bool> release{false}; ///< Test-owned helper releases the worker to throw bad_alloc.

    /** Keep the runtime lifetime explicit through the fault scenario.
     * @param runtime Test-owned main-thread runtime; no other foreign thread exists when this hook is armed.
     */
    explicit WorkerFault(org::ttldtor::qdcxx::api::ApiRuntime &runtime) : runtime(runtime) {
    }

    /** Publish this scenario only after its owner and atomic scheduling fields exist. */
    void arm() {
        worker.store(this, std::memory_order_release);
    }

    /** Release and join the owned worker before this stack-resident scheduling state can disappear. */
    ~WorkerFault() {
        release.store(true, std::memory_order_release);
        worker.store(nullptr, std::memory_order_release);
        runtime.closeAndAwaitTermination();
    }
};

/** Refuse one requested allocation before allocating application storage. */
void beforeAllocation() {
    if (std::exchange(failNext, false)) {
        throw std::bad_alloc();
    }

    auto *fault = worker.load(std::memory_order_acquire);

    if (fault && std::this_thread::get_id() != fault->owner && !fault->claimed.exchange(true)) {
        fault->entered.store(true, std::memory_order_release);

        while (!fault->release.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        throw std::bad_alloc();
    }
}
} // namespace allocation_probe

/** Allocate test-runner storage with an explicitly armed deterministic fault boundary.
 * @param bytes Requested allocation size, including a valid allocation for zero.
 * @return Owning malloc-backed storage; throws bad_alloc for the explicit fault or real exhaustion.
 */
void *operator new(std::size_t bytes) {
    allocation_probe::beforeAllocation();

    if (void *result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc();
}

/** Release storage allocated by the test executable's scalar allocation function.
 * @param address Owning allocation, or null.
 */
void operator delete(void *address) noexcept {
    std::free(address);
}

/** Release sized scalar storage through the same malloc/free pair.
 * @param address Owning allocation, or null.
 * @param bytes Original allocation size; not needed by free.
 */
void operator delete(void *address, std::size_t bytes) noexcept {
    (void)bytes;
    std::free(address);
}

/** Allocate array storage with the same explicit test fault boundary.
 * @param bytes Requested array allocation size.
 * @return Owning malloc-backed storage; throws bad_alloc on injected or real exhaustion.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release array storage through the test executable's matching malloc/free pair.
 * @param address Owning array allocation, or null.
 */
void operator delete[](void *address) noexcept {
    std::free(address);
}

/** Release sized array storage through the same malloc/free pair.
 * @param address Owning array allocation, or null.
 * @param bytes Original size; not needed by free.
 */
void operator delete[](void *address, std::size_t bytes) noexcept {
    (void)bytes;
    std::free(address);
}

/** Allocate nothrow scalar storage through the matching throwing replacement.
 * @param bytes Requested allocation size.
 * @param tag Standard nothrow allocation tag.
 * @return Owning storage, or null on allocation failure.
 */
void *operator new(std::size_t bytes, const std::nothrow_t &tag) noexcept {
    (void)tag;

    try {
        return ::operator new(bytes);
    } catch (...) {
        return nullptr;
    }
}

/** Allocate nothrow array storage through the matching throwing replacement.
 * @param bytes Requested allocation size.
 * @param tag Standard nothrow allocation tag.
 * @return Owning storage, or null on allocation failure.
 */
void *operator new[](std::size_t bytes, const std::nothrow_t &tag) noexcept {
    (void)tag;

    try {
        return ::operator new[](bytes);
    } catch (...) {
        return nullptr;
    }
}

/** Release nothrow scalar storage after construction failure.
 * @param address Owning allocation, or null.
 * @param tag Standard nothrow allocation tag.
 */
void operator delete(void *address, const std::nothrow_t &tag) noexcept {
    (void)tag;
    std::free(address);
}

/** Release nothrow array storage after construction failure.
 * @param address Owning allocation, or null.
 * @param tag Standard nothrow allocation tag.
 */
void operator delete[](void *address, const std::nothrow_t &tag) noexcept {
    (void)tag;
    std::free(address);
}

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

/** Test-owned synchronized state observations; waiting never drives SDK progress.
 * @tparam State Public endpoint or subscription enum.
 */
template <class State> struct StateLog {
    std::mutex mutex;                                 ///< Protects the finite test observation vector.
    std::condition_variable condition;                ///< Wakes assertions after actual callback publication.
    std::vector<std::pair<State, State>> transitions; ///< Ordered actual old/new observations.
    std::vector<std::thread::id> threads;             ///< Actual callback execution domains.

    /** Record one callback observation.
     * @param previous Actual old state.
     * @param current Actual new state.
     */
    void record(State previous, State current) {
        {
            std::lock_guard lock(mutex);
            transitions.emplace_back(previous, current);
            threads.push_back(std::this_thread::get_id());
        }

        condition.notify_all();
    }

    /** Await an actual notification rather than polling state.
     * @param state Desired observed new state.
     * @return True within the finite test deadline.
     */
    bool await(State state) {
        std::unique_lock lock(mutex);

        return condition.wait_for(lock, 3000ms, [&] {
            return std::ranges::any_of(transitions, [state](const auto &change) {
                return change.second == state;
            });
        });
    }

    /** Copy the finite test observations after synchronization.
     * @return Owning ordered transitions.
     */
    auto snapshot() {
        std::lock_guard lock(mutex);

        return transitions;
    }
};

/** Verify a full observer sees contiguous distinct transitions and an absorbing final state.
 * @tparam State Public lifecycle enum.
 * @param changes Complete owning transition observations after callback quiescence.
 * @param initial Expected initial state, which must not have been notified on registration.
 * @param terminal Actual final observed state.
 */
template <class State>
void checkStateOrder(const std::vector<std::pair<State, State>> &changes, State initial, State terminal) {
    REQUIRE_FALSE(changes.empty());
    CHECK(changes.front().first == initial);
    CHECK(changes.back().second == terminal);

    for (std::size_t index = 0; index < changes.size(); ++index) {
        CHECK(changes[index].first != changes[index].second);

        if (index != 0) {
            CHECK(changes[index - 1].second == changes[index].first);
            CHECK(changes[index].first != State::CLOSED);
            CHECK(changes[index].first != State::FAILED);
        }
    }
}

} // namespace

TEST_CASE("subscription statistics reconcile overlap listener fanout failure and absent listeners") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}, {u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD,
                      script(core::SubscriptionFamily::STREAM, true));
    auto endpoint = runtime.createEndpoint(endpointOptions(api::DeliveryContract::STREAM));
    auto healthy = endpoint->getFeed()->createSubscription<events::Quote>();
    auto broken = endpoint->getFeed()->createSubscription<events::Quote>();
    auto silent = endpoint->getFeed()->createSubscription<events::Quote>();
    CHECK(healthy->getStatistics().callbackDurationTotal.availability == core::StatisticAvailability::NO_SAMPLE);
    std::atomic<unsigned> seen{0};
    healthy->addEventListener([&](auto batch) {
        seen += static_cast<unsigned>(batch.size());
    });
    healthy->addEventListener([&](auto batch) {
        seen += static_cast<unsigned>(batch.size());
    });
    broken->addEventListener([](auto) {
    });
    broken->addEventListener([](auto) {
        throw std::runtime_error("second selected event listener");
    });
    healthy->addSymbols("IBM");
    broken->addSymbols("IBM");
    silent->addSymbols("IBM");
    CHECK(healthy->getStatistics().desiredSelectors == 1);
    CHECK(healthy->getStatistics().desiredRevision == 1);
    CHECK_FALSE(healthy->getStatistics().activeDesiredRevision);
    std::atomic<bool> coherent{true};
    std::atomic<unsigned> observations{0};
    std::jthread reader([&](std::stop_token stop) {
        auto previous = healthy->getStatistics();

        while (!stop.stop_requested()) {
            for (const auto &subscription : {healthy, broken, silent}) {
                const auto cut = subscription->getStatistics();
                const auto completed = cut.deliveredEvents.value + cut.conflatedEvents.value +
                                       cut.discardedEvents.value + cut.callbackFailedEvents.value +
                                       cut.evictedOverflowEvents.value + cut.pendingEvents + cut.inFlightEvents;

                if (cut.admittedEvents.value != completed ||
                    cut.droppedEvents.value != cut.rejectedOverflowEvents.value + cut.evictedOverflowEvents.value)
                    coherent = false;
            }

            const auto next = healthy->getStatistics();

            if (next.ownerIdentity != previous.ownerIdentity || next.observedAt < previous.observedAt ||
                next.admittedEvents.value < previous.admittedEvents.value)
                coherent = false;

            previous = next;
            ++observations;
            std::this_thread::yield();
        }
    });
    endpoint->connect(address(source));
    REQUIRE(eventually([&] {
        const auto first = healthy->getStatistics();
        const auto second = broken->getStatistics();
        const auto third = silent->getStatistics();

        return first.sourceEpoch && *first.sourceEpoch > 0 && first.sourceEpoch == second.sourceEpoch &&
               first.sourceEpoch == third.sourceEpoch && first.activeSelectors == 1 && second.activeSelectors == 1 &&
               third.activeSelectors == 1 && first.activeDesiredRevision == first.desiredRevision &&
               second.activeDesiredRevision == second.desiredRevision &&
               third.activeDesiredRevision == third.desiredRevision;
    }));
    source.releaseData();
    REQUIRE(eventually([&] {
        return seen == 4 && healthy->getStatistics().deliveredEvents.value == 2 &&
               broken->getStatistics().callbackFailedEvents.value == 2 &&
               silent->getStatistics().discardedEvents.value == 2;
    }));
    const auto active = healthy->getStatistics();
    CHECK(active.activeSelectors == 1);
    CHECK(active.activeDesiredRevision == active.desiredRevision);
    CHECK(active.sourceEpoch == 1);
    CHECK(active.queueCapacityEvents >= active.pendingEvents);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    reader.request_stop();
    reader.join();
    CHECK(coherent.load());
    CHECK(observations.load() > 0);
    CHECK(endpoint->getStatistics().connection.readDataRecords.value == 2);

    for (const auto &subscription : {healthy, broken, silent}) {
        const auto cut = subscription->getStatistics();
        CHECK(cut.final);
        CHECK(cut.admittedEvents.value == 2);
        CHECK(cut.pendingEvents == 0);
        CHECK(cut.inFlightEvents == 0);
        CHECK(cut.activeSelectors == 0);
        CHECK_FALSE(cut.activeDesiredRevision);
        CHECK(cut.droppedEvents.value == 0);
    }

    const auto delivered = healthy->getStatistics();
    const auto failed = broken->getStatistics();
    const auto discarded = silent->getStatistics();
    CHECK(delivered.callbackCalls.value == 2);
    CHECK(delivered.callbackFailures.value == 0);
    CHECK(delivered.callbackDurationSamples.value == 2);
    CHECK(delivered.callbackDurationTotal.availability == core::StatisticAvailability::AVAILABLE);
    CHECK(delivered.callbackDurationTotal.value >= delivered.callbackDurationMaximum.value);
    CHECK(failed.callbackCalls.value == 2);
    CHECK(failed.callbackFailures.value == 1);
    CHECK(failed.callbackDurationSamples.value == 2);
    CHECK(failed.deliveredEvents.value == 0);
    CHECK(discarded.callbackCalls.value == 0);
    CHECK(discarded.callbackDurationMaximum.availability == core::StatisticAvailability::NO_SAMPLE);
    CHECK(delivered.ownerIdentity != failed.ownerIdentity);
    runtime.closeAndAwaitTermination();
    CHECK(healthy->getStatistics().observedAt == delivered.observedAt);
}

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

TEST_CASE("endpoint statistics retain only detached identity and freeze after inactive retirement") {
    std::optional<api::EndpointStatistics> detached;
    {
        api::ApiRuntime runtime(runtimeOptions());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto baseline = endpoint->getStatistics();
        REQUIRE(baseline.ownerIdentity);
        CHECK_FALSE(baseline.final);
        allocation_probe::failNext = true;
        const auto read = endpoint->getStatistics();
        const bool allocated = !allocation_probe::failNext;
        allocation_probe::failNext = false;
        CHECK_FALSE(allocated);
        CHECK(read.observedAt == baseline.observedAt);
        CHECK(endpoint->closeAndAwaitTermination().drained);
        detached = endpoint->getStatistics();
        CHECK(detached->final);
        CHECK(detached->connection.connectionAttempts.value == 0);
        CHECK(detached->connection.activeConnections == 0);
        baseline.connection.connectionAttempts.value = 999;
        CHECK(endpoint->getStatistics().connection.connectionAttempts.value == 0);
    }

    api::ApiRuntime replacement(runtimeOptions());
    auto endpoint = replacement.createEndpoint(endpointOptions());
    CHECK(endpoint->getStatistics().ownerIdentity != detached->ownerIdentity);
    CHECK(detached->final);
    CHECK(detached->connection.connectionAttempts.value == 0);
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
    CAPTURE(endpoint->getDiagnostic().nativeCode);
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
    StateLog<api::SubscriptionState> brokenStates;
    broken->addStateChangeListener([&](auto previous, auto current) {
        brokenStates.record(previous, current);
    });
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
    checkStateOrder(brokenStates.snapshot(), api::SubscriptionState::CONFIGURED, api::SubscriptionState::FAILED);
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

    const auto blockedStatistics = endpoint->getStatistics();
    CHECK_FALSE(blockedStatistics.final);
    const auto blockedDelivery = subscription->getStatistics();
    CHECK(blockedDelivery.admittedEvents.value == 1);
    CHECK(blockedDelivery.inFlightEvents == 1);
    CHECK(blockedDelivery.pendingEvents == 0);
    CHECK(blockedDelivery.deliveredEvents.value == 0);
    CHECK(blockedDelivery.callbackCalls.value == 1);
    CHECK(blockedDelivery.callbackDurationSamples.value == 0);

    for (unsigned read = 0; read != 100; ++read) {
        const auto observation = endpoint->getStatistics();
        CHECK_FALSE(observation.final);
        CHECK(observation.ownerIdentity == blockedStatistics.ownerIdentity);
        const auto delivery = subscription->getStatistics();
        CHECK(delivery.inFlightEvents == 1);
        CHECK_FALSE(delivery.final);
    }

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
    CHECK(endpoint->getStatistics().final);
    CHECK(subscription->getStatistics().final);
    CHECK(subscription->getStatistics().inFlightEvents == 0);
    CHECK(subscription->getStatistics().deliveredEvents.value == 1);
    CHECK(subscription->getStatistics().callbackDurationSamples.value == 1);
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

TEST_CASE("public state observers have no initial call and preserve inactive legacy retirement") {
    api::ApiRuntime runtime(runtimeOptions());
    const auto owner = std::this_thread::get_id();
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    StateLog<api::EndpointState> endpoints;
    StateLog<api::SubscriptionState> subscriptions;
    bool eventCaptureRetired = false;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        eventCaptureRetired = true;
    };
    subscription->addEventListener([capture](std::span<const events::Quote>) {
    });
    capture.reset();
    api::ErrorCode endpointAwait = api::ErrorCode::NONE;
    api::ErrorCode runtimeAwait = api::ErrorCode::NONE;
    api::ErrorCode terminalAdmission = api::ErrorCode::NONE;
    bool legacyRetiredBeforeClosed = false;
    const auto subscriptionListener = subscription->addStateChangeListener([&](auto previous, auto current) {
        subscriptions.record(previous, current);

        if (current == api::SubscriptionState::CLOSED) {
            legacyRetiredBeforeClosed = eventCaptureRetired;
        }
    });
    const auto endpointListener = endpoint->addStateChangeListener([&](auto previous, auto current) {
        endpoints.record(previous, current);
        endpoint->close();
        endpointAwait = errorCode([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        runtimeAwait = errorCode([&] {
            runtime.closeAndAwaitTermination();
        });
        terminalAdmission = errorCode([&] {
            endpoint->addStateChangeListener([](auto, auto) {
            });
        });
        subscription->removeStateChangeListener(subscriptionListener);
    });
    CHECK(endpoints.snapshot().empty());
    CHECK(subscriptions.snapshot().empty());
    CHECK(endpoint->closeAndAwaitTermination().drained);
    checkStateOrder(endpoints.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
    // The endpoint CLOSING observer removed the subscription observer before its first selection.
    CHECK(subscriptions.snapshot().empty());
    CHECK(eventCaptureRetired);
    CHECK_FALSE(legacyRetiredBeforeClosed);
    CHECK(endpointAwait == api::ErrorCode::INVALID_STATE);
    CHECK(runtimeAwait == api::ErrorCode::INVALID_STATE);
    CHECK(terminalAdmission == api::ErrorCode::CLOSED);

    for (const auto thread : endpoints.threads) {
        CHECK(thread == owner);
    }

    endpoint->removeStateChangeListener(endpointListener);
    subscription->removeStateChangeListener(subscriptionListener);
    const auto count = endpoints.snapshot().size();
    runtime.closeAndAwaitTermination();
    CHECK(endpoints.snapshot().size() == count);
}

TEST_CASE("public state observers preserve event capture retirement before CLOSED") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Profile>();
    bool captureRetired = false;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        captureRetired = true;
    };
    subscription->addEventListener([capture](std::span<const events::Profile>) {
    });
    capture.reset();
    StateLog<api::SubscriptionState> changes;
    bool subscriptionClosedAfterCapture = false;
    bool endpointClosedAfterCapture = false;
    subscription->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::SubscriptionState::CLOSED) {
            subscriptionClosedAfterCapture = captureRetired;
            subscription->close();
        }
    });
    endpoint->addStateChangeListener([&](auto, auto current) {
        if (current == api::EndpointState::CLOSED) {
            endpointClosedAfterCapture = captureRetired && subscription->getState() == api::SubscriptionState::CLOSED;
        }
    });
    CHECK(endpoint->closeAndAwaitTermination().drained);
    checkStateOrder(changes.snapshot(), api::SubscriptionState::CONFIGURED, api::SubscriptionState::CLOSED);
    CHECK(subscriptionClosedAfterCapture);
    CHECK(endpointClosedAfterCapture);
}

TEST_CASE("public state observer reentry changes only future selections on the SDK worker") {
    api::ApiRuntime runtime(runtimeOptions());
    const auto owner = std::this_thread::get_id();
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto other = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addSymbols("IBM");
    StateLog<api::EndpointState> all;
    StateLog<api::EndpointState> removed;
    StateLog<api::EndpointState> added;
    StateLog<api::SubscriptionState> logical;
    std::atomic<api::ErrorCode> workerAwait{api::ErrorCode::NONE};
    api::Endpoint::StateChangeListenerId removedId = 0;
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        all.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            endpoint->removeStateChangeListener(removedId);
            endpoint->addStateChangeListener([&](auto oldState, auto newState) {
                added.record(oldState, newState);
            });
            workerAwait = errorCode([&] {
                static_cast<void>(endpoint->closeAndAwaitTermination());
            });
            other->close();
        }
    });
    removedId = endpoint->addStateChangeListener([&](auto previous, auto current) {
        removed.record(previous, current);
    });
    subscription->addStateChangeListener([&](auto previous, auto current) {
        logical.record(previous, current);
    });
    std::atomic<bool> delivered{false};
    subscription->addEventListener([&](std::span<const events::Quote>) {
        delivered = true;
    });
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return delivered.load();
    }));
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    checkRetiredEndpoint(endpoint);
    checkStateOrder(all.snapshot(), api::EndpointState::NOT_CONNECTED, endpoint->getState());
    checkStateOrder(logical.snapshot(), api::SubscriptionState::CONFIGURED, api::SubscriptionState::CLOSED);
    REQUIRE(removed.snapshot().size() == 1);
    CHECK(removed.snapshot().front().second == api::EndpointState::CONNECTING);
    REQUIRE_FALSE(added.snapshot().empty());
    CHECK(added.snapshot().front().first == api::EndpointState::CONNECTING);
    CHECK(workerAwait.load() == api::ErrorCode::WRONG_THREAD);

    for (const auto thread : all.threads) {
        CHECK(thread != owner);
        CHECK(thread == all.threads.front());
    }

    CHECK(other->closeAndAwaitTermination().drained);
}

TEST_CASE("public state observer exceptions remain advisory through terminal notifications") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addSymbols("IBM");
    endpoint->addStateChangeListener([](auto, auto) {
        throw std::runtime_error("advisory endpoint observer");
    });
    subscription->addStateChangeListener([](auto, auto) {
        throw std::runtime_error("advisory subscription observer");
    });
    StateLog<api::EndpointState> endpoints;
    StateLog<api::SubscriptionState> subscriptions;
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        endpoints.record(previous, current);
    });
    subscription->addStateChangeListener([&](auto previous, auto current) {
        subscriptions.record(previous, current);
    });
    std::atomic<bool> delivered{false};
    subscription->addEventListener([&](std::span<const events::Quote>) {
        delivered = true;
    });
    endpoint->connect(address(source));
    CHECK(eventually([&] {
        return delivered.load();
    }));
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    const auto terminal = endpoint->closeAndAwaitTermination();
    checkRetiredEndpoint(endpoint);
    checkStateOrder(endpoints.snapshot(), api::EndpointState::NOT_CONNECTED, endpoint->getState());
    checkStateOrder(subscriptions.snapshot(), api::SubscriptionState::CONFIGURED, api::SubscriptionState::CLOSED);
    CHECK(terminal.diagnostic.code == endpoint->getDiagnostic().code);
    CHECK(terminal.diagnostic.stage == endpoint->getDiagnostic().stage);
    CHECK(terminal.diagnostic.nativeCode == endpoint->getDiagnostic().nativeCode);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(endpoint->getStateChangeDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(subscription->getStateChangeDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(endpoints.snapshot().size() <= 4);
}

TEST_CASE("public state observers report natural no-subscription failure before quiescence") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols;
    const std::vector<peer::Record> records;
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::NO_READINESS, script());
    auto options = endpointOptions();
    options.connectTimeout = 100ms;
    options.readTimeout = 100ms;
    auto endpoint = runtime.createEndpoint(options);
    StateLog<api::EndpointState> changes;
    std::atomic<api::ErrorCode> lateAdmission{api::ErrorCode::NONE};
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::FAILED) {
            lateAdmission = errorCode([&] {
                static_cast<void>(endpoint->getFeed()->createSubscription<events::Quote>());
            });
            throw std::runtime_error("terminal advisory observer");
        }
    });
    endpoint->connect(address(source));
    CHECK(changes.await(api::EndpointState::FAILED));
    const auto terminal = endpoint->closeAndAwaitTermination();
    checkStateOrder(changes.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::FAILED);
    CHECK(lateAdmission.load() == api::ErrorCode::CLOSED);
    CHECK(terminal.diagnostic.code != api::ErrorCode::NONE);
    CHECK(terminal.diagnostic.code != api::ErrorCode::CALLBACK_FAILURE);
    CHECK(endpoint->getStateChangeDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(endpoint->getState() == api::EndpointState::FAILED);
}

TEST_CASE("public state observer capacity includes removed selected captures") {
    auto limits = runtimeOptions();
    limits.maximumStateListenersPerEndpoint = 2;
    api::ApiRuntime runtime(limits);
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols;
    const std::vector<peer::Record> records;
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> selected{false};
    std::atomic<bool> removedCalled{false};
    std::atomic<bool> removedDestroyed{false};
    StateLog<api::EndpointState> changes;
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            selected = true;
            std::unique_lock lock(mutex);
            condition.wait(lock, [&] {
                return release;
            });
        }
    });
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        // Reentering a getter would deadlock if the last callable were destroyed under its port lock.
        static_cast<void>(endpoint->getState());
        removedDestroyed = true;
    };
    const auto id = endpoint->addStateChangeListener([&, capture](auto, auto) {
        removedCalled = true;
    });
    capture.reset();
    endpoint->connect(address(source));
    const auto entered = eventually([&] {
        return selected.load();
    });
    endpoint->removeStateChangeListener(id);
    CHECK(errorCode([&] {
              endpoint->addStateChangeListener([](auto, auto) {
              });
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK_FALSE(removedDestroyed.load());
    {
        std::lock_guard lock(mutex);
        release = true;
    }

    condition.notify_all();
    CHECK(entered);
    CHECK(eventually([&] {
        return removedDestroyed.load();
    }));
    CHECK(removedCalled.load());
    CHECK_NOTHROW(endpoint->addStateChangeListener([](auto, auto) {
    }));
    static_cast<void>(endpoint->closeAndAwaitTermination());
}

TEST_CASE("public state observer retirement includes producer-held capture and keeps CLOSED absorbing") {
    auto limits = runtimeOptions();
    limits.maximumEndpoints = 1;
    limits.maximumStateListenersPerEndpoint = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> destructorEntered{false};
    std::atomic<bool> destructorReturned{false};
    std::atomic<bool> awaitReturned{false};
    std::atomic<bool> prematureReturn{false};
    std::atomic<api::ErrorCode> slotReuse{api::ErrorCode::NONE};
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        destructorEntered = true;
        static_cast<void>(endpoint->getState());
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] {
            return release;
        });
        destructorReturned = true;
    };
    const auto id = endpoint->addStateChangeListener([capture](auto, auto) {
    });
    capture.reset();
    std::thread producer([&] {
        endpoint->removeStateChangeListener(id);
    });
    const auto entered = eventually([&] {
        return destructorEntered.load();
    });
    std::thread releaser([&] {
        const auto closed = eventually([&] {
            return endpoint->getState() == api::EndpointState::CLOSED;
        });
        prematureReturn = !closed || awaitReturned.load();
        slotReuse = errorCode([&] {
            static_cast<void>(runtime.createEndpoint(endpointOptions()));
        });
        {
            std::lock_guard lock(mutex);
            release = true;
        }

        condition.notify_all();
    });
    CHECK(endpoint->closeAndAwaitTermination().drained);
    awaitReturned = true;
    releaser.join();
    producer.join();
    CHECK(entered);
    CHECK(destructorReturned.load());
    CHECK_FALSE(prematureReturn.load());
    CHECK(slotReuse.load() == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK_NOTHROW(static_cast<void>(runtime.createEndpoint(endpointOptions())));
    endpoint->removeStateChangeListener(id);
}

TEST_CASE("public state observers close before activation without a peer and deliver the pending transition first") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    StateLog<api::EndpointState> changes;
    std::atomic<api::ErrorCode> callbackAwait{api::ErrorCode::NONE};
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            endpoint->close();
            callbackAwait = errorCode([&] {
                static_cast<void>(endpoint->closeAndAwaitTermination());
            });
        }
    });
    // The first observer closes before activation; no listener, peer or socket progress is required.
    endpoint->connect("127.0.0.1:1");
    CHECK(changes.await(api::EndpointState::CLOSED));
    CHECK(endpoint->closeAndAwaitTermination().drained);
    checkStateOrder(changes.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
    CHECK(changes.snapshot().size() == 3);
    CHECK(callbackAwait.load() == api::ErrorCode::WRONG_THREAD);
}

TEST_CASE("public state observer bounds and producer destruction use the existing reentry guard") {
    auto limits = runtimeOptions();
    limits.maximumStateListenersPerEndpoint = 1;
    limits.maximumStateListenersPerSubscription = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Profile>();
    CHECK(errorCode([&] {
              endpoint->addStateChangeListener({});
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              subscription->addStateChangeListener({});
          }) == api::ErrorCode::INVALID_ARGUMENT);
    api::ErrorCode endpointAwait = api::ErrorCode::NONE;
    api::ErrorCode runtimeAwait = api::ErrorCode::NONE;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        endpointAwait = errorCode([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        runtimeAwait = errorCode([&] {
            runtime.closeAndAwaitTermination();
        });
    };
    const auto id = subscription->addStateChangeListener([capture](auto, auto) {
    });
    capture.reset();
    CHECK(errorCode([&] {
              subscription->addStateChangeListener([](auto, auto) {
              });
          }) == api::ErrorCode::RESOURCE_LIMIT);
    subscription->removeStateChangeListener(id);
    CHECK(endpointAwait == api::ErrorCode::INVALID_STATE);
    CHECK(runtimeAwait == api::ErrorCode::INVALID_STATE);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK_NOTHROW(subscription->addStateChangeListener([](auto, auto) {
    }));
    CHECK(endpoint->closeAndAwaitTermination().drained);
    subscription->removeStateChangeListener(id);
}

TEST_CASE("public state observer terminal publication precedes state callback quiescence and slot reuse") {
    auto limits = runtimeOptions();
    limits.maximumEndpoints = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> terminalEntered{false};
    std::atomic<bool> awaitReturned{false};
    std::atomic<bool> prematureReturn{false};
    std::atomic<api::ErrorCode> slotReuse{api::ErrorCode::NONE};
    StateLog<api::EndpointState> changes;
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            endpoint->close();
        }

        if (current == api::EndpointState::CLOSED) {
            terminalEntered = true;
            std::unique_lock lock(mutex);
            condition.wait(lock, [&] {
                return release;
            });
        }
    });
    endpoint->connect("127.0.0.1:1");
    std::thread releaser([&] {
        const auto entered = eventually([&] {
            return terminalEntered.load();
        });
        prematureReturn = !entered || awaitReturned.load() || endpoint->getState() != api::EndpointState::CLOSED;
        slotReuse = errorCode([&] {
            static_cast<void>(runtime.createEndpoint(endpointOptions()));
        });
        {
            std::lock_guard lock(mutex);
            release = true;
        }

        condition.notify_all();
    });
    CHECK(endpoint->closeAndAwaitTermination().drained);
    awaitReturned = true;
    releaser.join();
    CHECK_FALSE(prematureReturn.load());
    CHECK(slotReuse.load() == api::ErrorCode::RESOURCE_LIMIT);
    checkStateOrder(changes.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
}

TEST_CASE("public callback box allocation failure releases its reservation before capture self-await") {
    SUBCASE("state observer") {
        api::ApiRuntime runtime(runtimeOptions());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        bool destroyed = false;
        api::ErrorCode awaitError = api::ErrorCode::INVALID_STATE;
        auto capture = std::make_shared<CaptureAction>();
        capture->action = [&] {
            awaitError = errorCode([&] {
                static_cast<void>(endpoint->closeAndAwaitTermination());
                runtime.closeAndAwaitTermination();
            });
            destroyed = true;
        };
        api::Endpoint::StateChangeListener listener = [capture](auto, auto) {
        };
        capture.reset();
        const std::function<void()> action = [&] {
            endpoint->addStateChangeListener(std::move(listener));
        };
        allocation_probe::failNext = true;
        const auto rejected = errorCode(action);
        CHECK_FALSE(allocation_probe::failNext);
        CHECK(rejected == api::ErrorCode::ALLOCATION_FAILURE);
        CHECK(destroyed);
        CHECK(awaitError == api::ErrorCode::NONE);
        CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    }

    SUBCASE("event listener") {
        api::ApiRuntime runtime(runtimeOptions());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
        bool destroyed = false;
        api::ErrorCode awaitError = api::ErrorCode::INVALID_STATE;
        auto capture = std::make_shared<CaptureAction>();
        capture->action = [&] {
            awaitError = errorCode([&] {
                static_cast<void>(endpoint->closeAndAwaitTermination());
                runtime.closeAndAwaitTermination();
            });
            destroyed = true;
        };
        api::Subscription<events::Quote>::Listener listener = [capture](auto) {
        };
        capture.reset();
        const std::function<void()> action = [&] {
            subscription->addEventListener(std::move(listener));
        };
        allocation_probe::failNext = true;
        const auto rejected = errorCode(action);
        CHECK_FALSE(allocation_probe::failNext);
        CHECK(rejected == api::ErrorCode::ALLOCATION_FAILURE);
        CHECK(destroyed);
        CHECK(awaitError == api::ErrorCode::NONE);
        CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    }
}

TEST_CASE("public worker allocation failure sweeps distinct endpoints before waiting for producer captures") {
    api::ApiRuntime runtime(runtimeOptions());
    auto first = runtime.createEndpoint(endpointOptions());
    auto second = runtime.createEndpoint(endpointOptions());
    StateLog<api::EndpointState> secondStates;
    second->addStateChangeListener([&](auto previous, auto current) {
        secondStates.record(previous, current);
    });
    std::atomic<bool> destructorEntered{false};
    bool destructorReturned = false;
    api::ErrorCode nestedAwait = api::ErrorCode::INVALID_STATE;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        destructorEntered = true;
        nestedAwait = errorCode([&] {
            static_cast<void>(second->closeAndAwaitTermination());
        });
        destructorReturned = true;
    };
    const auto id = first->addStateChangeListener([capture](auto, auto) {
    });
    capture.reset();
    allocation_probe::WorkerFault fault(runtime);
    fault.arm();
    second->connect("127.0.0.1:1");
    CHECK(eventually([&] {
        return fault.entered.load(std::memory_order_acquire);
    }));
    std::atomic<bool> releasedAfterDestructor{false};
    std::thread releaser([&] {
        releasedAfterDestructor = eventually([&] {
            return destructorEntered.load();
        });
        fault.release.store(true, std::memory_order_release);
    });
    first->removeStateChangeListener(id);
    releaser.join();
    CHECK(releasedAfterDestructor.load());
    CHECK(destructorReturned);
    CHECK(nestedAwait == api::ErrorCode::NONE);
    runtime.closeAndAwaitTermination();
    checkStateOrder(secondStates.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::FAILED);
    REQUIRE(secondStates.snapshot().size() == 2);
    CHECK(secondStates.snapshot().front().second == api::EndpointState::CONNECTING);
    CHECK(second->getDiagnostic().code == api::ErrorCode::ALLOCATION_FAILURE);
    CHECK(second->getDiagnostic().stage == "runtime-worker");
    CHECK(first->getDiagnostic().code == api::ErrorCode::ALLOCATION_FAILURE);
}

TEST_CASE("public worker allocation failure respects MAIN claimed retirement and deferred CONNECTING") {
    api::ApiRuntime runtime(runtimeOptions());
    auto inactive = runtime.createEndpoint(endpointOptions());
    auto failing = runtime.createEndpoint(endpointOptions());
    StateLog<api::EndpointState> inactiveStates;
    StateLog<api::EndpointState> failingStates;
    const auto owner = std::this_thread::get_id();
    failing->addStateChangeListener([&](auto previous, auto current) {
        failingStates.record(previous, current);
    });
    allocation_probe::WorkerFault fault(runtime);
    bool nestedRetired = false;
    bool workerIntercepted = false;
    inactive->addStateChangeListener([&](auto previous, auto current) {
        inactiveStates.record(previous, current);

        if (current == api::EndpointState::CLOSING) {
            fault.arm();
            failing->connect("127.0.0.1:1");
            workerIntercepted = eventually([&] {
                return fault.entered.load(std::memory_order_acquire);
            });
            fault.release.store(true, std::memory_order_release);
            static_cast<void>(failing->closeAndAwaitTermination());
            nestedRetired = true;
        }
    });
    CHECK(inactive->closeAndAwaitTermination().drained);
    CHECK(nestedRetired);
    CHECK(workerIntercepted);
    runtime.closeAndAwaitTermination();
    checkStateOrder(inactiveStates.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
    checkStateOrder(failingStates.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::FAILED);
    CHECK(failingStates.snapshot().size() == 2);
    CHECK(inactive->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(failing->getDiagnostic().code == api::ErrorCode::ALLOCATION_FAILURE);
    CHECK(failing->getDiagnostic().stage == "runtime-worker");

    for (const auto thread : inactiveStates.threads) {
        CHECK(thread == owner);
    }

    for (const auto thread : failingStates.threads) {
        CHECK(thread != owner);
    }
}

TEST_CASE("public state observers preserve CONNECTING order on an already running sibling worker") {
    api::ApiRuntime runtime(runtimeOptions());
    auto starter = runtime.createEndpoint(endpointOptions());
    StateLog<api::EndpointState> starterStates;
    starter->addStateChangeListener([&](auto previous, auto current) {
        starterStates.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            starter->close();
        }
    });
    starter->connect("127.0.0.1:1");
    CHECK(starterStates.await(api::EndpointState::CLOSED));
    CHECK(starter->closeAndAwaitTermination().drained);
    REQUIRE_FALSE(starterStates.threads.empty());
    const auto worker = starterStates.threads.front();
    constexpr std::size_t CONCURRENT_CONNECT_ROUNDS = 32;

    for (std::size_t round = 0; round < CONCURRENT_CONNECT_ROUNDS; ++round) {
        auto endpoint = runtime.createEndpoint(endpointOptions());
        StateLog<api::EndpointState> changes;
        endpoint->addStateChangeListener([&](auto previous, auto current) {
            changes.record(previous, current);

            if (current == api::EndpointState::CONNECTING) {
                endpoint->close();
            }
        });
        std::thread producer([&] {
            endpoint->connect("127.0.0.1:1");
        });
        producer.join();
        CHECK(changes.await(api::EndpointState::CLOSED));
        CHECK(endpoint->closeAndAwaitTermination().drained);
        checkStateOrder(changes.snapshot(), api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
        CHECK(changes.snapshot().size() == 3);

        for (const auto thread : changes.threads) {
            CHECK(thread == worker);
        }
    }
}

TEST_CASE("public await current state defines ready terminal cancellation and deadline precedence") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::stop_source stopped;
    stopped.request_stop();
    const auto past = std::chrono::steady_clock::now() - 1ms;

    const auto ready = endpoint->awaitState(api::EndpointState::NOT_CONNECTED, past, stopped.get_token());
    CHECK(ready.status == api::WaitStatus::REACHED);
    CHECK(ready.state == api::EndpointState::NOT_CONNECTED);
    CHECK(ready.diagnostic.code == api::ErrorCode::NONE);
    CHECK(subscription->awaitState(api::SubscriptionState::CONFIGURED, past, stopped.get_token()).status ==
          api::WaitStatus::REACHED);
    CHECK(endpoint->awaitState(api::EndpointState::CONNECTED, past, stopped.get_token()).status ==
          api::WaitStatus::CANCELLED);
    CHECK(subscription->awaitState(api::SubscriptionState::ACTIVE, past, stopped.get_token()).status ==
          api::WaitStatus::CANCELLED);
    CHECK(endpoint->awaitState(api::EndpointState::CONNECTED, past).status == api::WaitStatus::DEADLINE_EXPIRED);
    CHECK(subscription->awaitState(api::SubscriptionState::ACTIVE, past).status == api::WaitStatus::DEADLINE_EXPIRED);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(subscription->getState() == api::SubscriptionState::CONFIGURED);
    CHECK(errorCode([&] {
              static_cast<void>(endpoint->awaitState(static_cast<api::EndpointState>(-1), past));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              static_cast<void>(subscription->awaitState(static_cast<api::SubscriptionState>(255), past));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(endpoint->closeAndAwaitTermination().drained);

    CHECK(endpoint->awaitState(api::EndpointState::CLOSED, past, stopped.get_token()).status ==
          api::WaitStatus::REACHED);
    const auto terminal = endpoint->awaitState(api::EndpointState::CONNECTED, past, stopped.get_token());
    CHECK(terminal.status == api::WaitStatus::TERMINAL);
    CHECK(terminal.state == api::EndpointState::CLOSED);
    CHECK(terminal.diagnostic.code == api::ErrorCode::NONE);
    CHECK(subscription->awaitState(api::SubscriptionState::ACTIVE, past, stopped.get_token()).status ==
          api::WaitStatus::TERMINAL);
}

TEST_CASE("public await keeps creator affinity on detached endpoint and subscription handles") {
    std::shared_ptr<api::Endpoint> endpoint;
    std::shared_ptr<api::Subscription<events::Profile>> subscription;
    {
        api::ApiRuntime runtime(runtimeOptions());
        endpoint = runtime.createEndpoint(endpointOptions());
        subscription = endpoint->getFeed()->createSubscription<events::Profile>();
        runtime.closeAndAwaitTermination();
    }

    CHECK(endpoint->awaitState(api::EndpointState::CLOSED).status == api::WaitStatus::REACHED);
    endpoint.reset();
    CHECK(subscription->awaitState(api::SubscriptionState::CLOSED).status == api::WaitStatus::REACHED);
    std::atomic<api::ErrorCode> error{api::ErrorCode::NONE};
    std::thread foreign([&] {
        error = errorCode([&] {
            static_cast<void>(subscription->awaitState(api::SubscriptionState::CLOSED));
        });
    });
    foreign.join();
    CHECK(error.load() == api::ErrorCode::WRONG_THREAD);
}

TEST_CASE("public await rejects worker callbacks and every creator retirement context before ready success") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto sibling = runtime.createEndpoint(endpointOptions());
    auto subscription = sibling->getFeed()->createSubscription<events::Quote>();
    std::atomic<api::ErrorCode> workerError{api::ErrorCode::NONE};
    api::ErrorCode distinctError = api::ErrorCode::NONE;
    api::ErrorCode subscriptionError = api::ErrorCode::NONE;
    sibling->addStateChangeListener([&](auto, auto) {
        distinctError = errorCode([&] {
            static_cast<void>(endpoint->awaitState(api::EndpointState::NOT_CONNECTED));
        });
        subscriptionError = errorCode([&] {
            static_cast<void>(subscription->awaitState(subscription->getState()));
        });
    });
    CHECK(sibling->closeAndAwaitTermination().drained);
    CHECK(distinctError == api::ErrorCode::INVALID_STATE);
    CHECK(subscriptionError == api::ErrorCode::INVALID_STATE);
    endpoint->addStateChangeListener([&](auto, auto current) {
        workerError = errorCode([&] {
            static_cast<void>(endpoint->awaitState(current));
        });

        if (current == api::EndpointState::CONNECTING) {
            endpoint->close();
        }
    });
    endpoint->connect("127.0.0.1:1");
    const auto result = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 3s);
    CHECK(result.status == api::WaitStatus::REACHED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(workerError.load() == api::ErrorCode::WRONG_THREAD);
    CHECK(endpoint->awaitState(api::EndpointState::CONNECTING).status == api::WaitStatus::TERMINAL);
}

TEST_CASE("public await cancellation registration and concurrent state publication do not lose wakeups") {
    api::ApiRuntime runtime(runtimeOptions());
    // Scheduling stress covers cancellation before/during registration; it is not a deterministic tiny-window hook.
    constexpr unsigned ROUNDS = 32;

    for (unsigned round = 0; round < ROUNDS; ++round) {
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
        std::stop_source cancellation;
        std::thread requester([source = cancellation]() mutable {
            source.request_stop();
        });
        const auto result = subscription->awaitState(api::SubscriptionState::ACTIVE,
                                                     std::chrono::steady_clock::now() + 3s, cancellation.get_token());
        requester.join();
        CHECK(result.status == api::WaitStatus::CANCELLED);
        CHECK(result.state == api::SubscriptionState::CONFIGURED);
        CHECK(result.diagnostic.code == api::ErrorCode::NONE);
        endpoint->addStateChangeListener([endpoint](auto, auto current) {
            if (current == api::EndpointState::CONNECTING) {
                endpoint->close();
            }
        });
        std::thread producer([endpoint] {
            endpoint->connect("127.0.0.1:1");
        });
        const auto closed = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 3s);
        producer.join();
        CHECK(closed.status == api::WaitStatus::REACHED);
        CHECK(endpoint->closeAndAwaitTermination().drained);
    }
}

TEST_CASE("public await CLOSED observes state before opt-in terminal observer quiescence") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    std::mutex mutex;
    std::condition_variable condition;
    bool release = false;
    std::atomic<bool> terminalEntered{false};
    std::atomic<bool> terminalExited{false};
    endpoint->addStateChangeListener([&](auto, auto current) {
        if (current == api::EndpointState::CONNECTING) {
            endpoint->close();
        }

        if (current == api::EndpointState::CLOSED) {
            terminalEntered = true;
            std::unique_lock lock(mutex);
            condition.wait(lock, [&] {
                return release;
            });
            terminalExited = true;
        }
    });
    endpoint->connect("127.0.0.1:1");
    const auto result = endpoint->awaitState(api::EndpointState::CLOSED, std::chrono::steady_clock::now() + 3s);
    CHECK(result.status == api::WaitStatus::REACHED);
    CHECK_FALSE(terminalExited.load());
    {
        std::lock_guard lock(mutex);
        release = true;
    }

    condition.notify_all();
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(terminalEntered.load());
    CHECK(terminalExited.load());
}

TEST_CASE("public await captures actual subscription failure and deregisters cancellation before returning") {
    std::stop_source cancellation;
    {
        api::ApiRuntime runtime(runtimeOptions());
        auto schema = standardSchema(false);
        const std::vector<std::u16string> symbols{u"IBM"};
        const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
        peer::Peer source(schema, records, symbols, 64, peer::PeerMode::HOLD, script());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
        std::atomic<api::ErrorCode> callbackError{api::ErrorCode::NONE};
        subscription->addEventListener([&](std::span<const events::Quote>) {
            callbackError = errorCode([&] {
                static_cast<void>(subscription->awaitState(api::SubscriptionState::ACTIVE));
            });
            throw std::runtime_error("intentional-event-failure");
        });
        subscription->addSymbols("IBM");
        endpoint->connect(address(source));
        const auto failed = subscription->awaitState(api::SubscriptionState::FAILED,
                                                     std::chrono::steady_clock::now() + 3s, cancellation.get_token());
        CHECK(failed.status == api::WaitStatus::REACHED);
        CHECK(failed.state == api::SubscriptionState::FAILED);
        CHECK(failed.diagnostic.code == api::ErrorCode::CALLBACK_FAILURE);
        CHECK(callbackError.load() == api::ErrorCode::WRONG_THREAD);
        CHECK(subscription->awaitState(api::SubscriptionState::ACTIVE).status == api::WaitStatus::TERMINAL);
        static_cast<void>(endpoint->closeAndAwaitTermination());
        CHECK_FALSE(source.failed());
    }

    // Neither the detached port nor its stack mutex is retained by a deregistered stop callback.
    CHECK(cancellation.request_stop());
}

TEST_CASE("public await expires an actual wait and rejects creator capture self and distinct waits") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto sibling = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Profile>();
    const auto cutoff = std::chrono::steady_clock::now() + 5ms;
    const auto expired = endpoint->awaitState(api::EndpointState::CONNECTED, cutoff);
    CHECK(expired.status == api::WaitStatus::DEADLINE_EXPIRED);
    CHECK(std::chrono::steady_clock::now() >= cutoff);
    CHECK(expired.state == api::EndpointState::NOT_CONNECTED);
    api::ErrorCode selfError = api::ErrorCode::NONE;
    api::ErrorCode distinctError = api::ErrorCode::NONE;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        selfError = errorCode([&] {
            static_cast<void>(subscription->awaitState(api::SubscriptionState::CONFIGURED));
        });
        distinctError = errorCode([&] {
            static_cast<void>(sibling->awaitState(api::EndpointState::NOT_CONNECTED));
        });
    };
    const auto id = subscription->addStateChangeListener([capture](auto, auto) {
    });
    capture.reset();
    subscription->removeStateChangeListener(id);
    CHECK(selfError == api::ErrorCode::INVALID_STATE);
    CHECK(distinctError == api::ErrorCode::INVALID_STATE);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(sibling->closeAndAwaitTermination().drained);
}

TEST_CASE("public await terminal failure precedes cancellation and deadline with exact diagnostic snapshot") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema();
    const std::vector<std::u16string> symbols;
    const std::vector<peer::Record> records;
    peer::Peer source(schema, records, symbols, 8192, peer::PeerMode::NO_READINESS, script());
    auto options = endpointOptions();
    options.connectTimeout = 100ms;
    options.readTimeout = 100ms;
    auto endpoint = runtime.createEndpoint(options);
    endpoint->connect(address(source));
    const auto failed = endpoint->awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + 3s);
    CHECK(failed.status == api::WaitStatus::REACHED);
    CHECK(failed.state == api::EndpointState::FAILED);
    CHECK(failed.diagnostic.code == api::ErrorCode::TRANSPORT_FAILURE);
    const auto terminal = endpoint->closeAndAwaitTermination();
    CHECK(terminal.diagnostic.code == failed.diagnostic.code);
    CHECK(terminal.diagnostic.stage == failed.diagnostic.stage);
    CHECK(terminal.diagnostic.nativeCode == failed.diagnostic.nativeCode);
    CHECK(terminal.diagnostic.detailCode == failed.diagnostic.detailCode);
    std::stop_source cancellation;
    cancellation.request_stop();
    const auto result = endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() - 1ms,
                                             cancellation.get_token());
    CHECK(result.status == api::WaitStatus::TERMINAL);
    CHECK(result.state == api::EndpointState::FAILED);
    CHECK(result.diagnostic.code == failed.diagnostic.code);
    CHECK(result.diagnostic.stage == failed.diagnostic.stage);
    CHECK(endpoint->awaitState(api::EndpointState::FAILED, {}, cancellation.get_token()).status ==
          api::WaitStatus::REACHED);
}

TEST_CASE("public await accepts cancellation requested by its own worker state callback") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<std::u16string> symbols{u"IBM"};
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    peer::Peer source(schema, records, symbols, 64, peer::PeerMode::HOLD, script());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    std::stop_source cancellation;
    std::atomic<bool> callbackReturned{false};
    subscription->addStateChangeListener([source = cancellation, &callbackReturned](auto, auto current) mutable {
        if (current == api::SubscriptionState::ACTIVE) {
            source.request_stop();
            callbackReturned = true;
        }
    });
    subscription->addSymbols("IBM");
    endpoint->connect(address(source));
    const auto result = subscription->awaitState(api::SubscriptionState::CLOSED, std::chrono::steady_clock::now() + 3s,
                                                 cancellation.get_token());
    CHECK(result.status == api::WaitStatus::CANCELLED);
    CHECK(result.state == api::SubscriptionState::ACTIVE);
    CHECK(result.diagnostic.code == api::ErrorCode::NONE);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    CHECK(callbackReturned.load());
    CHECK_FALSE(source.failed());
}

namespace {
/** Finite test-owned callback/capture barrier; storage is declared before the runtime it observes. */
struct LifecycleBarrier {
    std::mutex mutex;                  ///< Protects scheduling flags, never an SDK control lock.
    std::condition_variable condition; ///< Explicit owned scheduling, not network progress.
    bool entered = false;              ///< Callback/capture reached the held phase.
    bool release = false;              ///< Main test opened this barrier.
    std::atomic<bool> timedOut{false}; ///< A failed setup cannot hold the worker indefinitely.

    /** Hold one actual callback/destructor for at most five seconds. */
    void enter() {
        std::unique_lock lock(mutex);
        entered = true;
        condition.notify_all();
        timedOut = !condition.wait_for(lock, 5s, [&] {
            return release;
        });
    }

    /** Observe entry without driving any SDK progress.
     * @return True within the finite setup allowance.
     */
    bool awaitEntry() {
        std::unique_lock lock(mutex);

        return condition.wait_for(lock, 3s, [&] {
            return entered;
        });
    }

    /** Wake only this owned barrier; safe on assertion unwind. */
    void open() {
        {
            std::lock_guard lock(mutex);
            release = true;
        }

        condition.notify_all();
    }
};

/** Release before runtime destruction while the separate barrier storage still lives. */
struct ReleaseLifecycleBarrier {
    LifecycleBarrier &barrier; ///< Test storage declared before the main-owned runtime.

    /** Guarantee the runtime never joins behind an unreleased test barrier. */
    ~ReleaseLifecycleBarrier() {
        barrier.open();
    }
};

/** Decode actual outgoing TICKER ADD tuples from a bounded captured wire prefix.
 * @param bytes Bytes captured from the real client, not writer composition output.
 * @return Number of complete exact IBM/local-record0 ADD frames.
 */
std::size_t checkLifecycleAdds(std::span<const std::byte> bytes) {
    root::protocol::FrameReader reader({8192, 8201});
    std::size_t adds = 0;

    while (!bytes.empty()) {
        const auto framed = reader.feed(bytes);
        REQUIRE(framed.status != root::protocol::FrameStatus::FAILED);
        REQUIRE(framed.accepted != 0);
        bytes = bytes.subspan(framed.accepted);

        if (!framed.frame || framed.frame->type != 11) {
            continue;
        }

        std::span<const std::byte> payload = framed.frame->payload;
        root::encoding::SymbolReader symbols({8192, 128, 128}, 0);
        const auto symbol = symbols.read(payload);
        REQUIRE(symbol.status == root::encoding::SymbolStatus::OK);
        CHECK(symbol.value->units == u"IBM");
        CHECK(symbol.value->eventFlags == 0);
        payload = payload.subspan(symbol.consumed);
        const auto record = root::encoding::readCompactInt(payload);
        REQUIRE(record.status == root::encoding::Status::OK);
        CHECK(record.value == 0);
        CHECK(payload.subspan(record.consumed).empty());
        ++adds;
    }

    return adds;
}

/** Count complete framed messages in actual bounded peer captures, without interpreting payloads.
 * @param bytes Exact wire bytes captured by the independent peer.
 * @return Complete frame occurrences; a trailing incomplete frame fails the test.
 */
std::size_t capturedMessages(std::span<const std::byte> bytes) {
    root::protocol::FrameReader reader({8192, 8201});
    std::size_t messages = 0;

    while (!bytes.empty()) {
        const auto result = reader.feed(bytes);
        REQUIRE(result.status != root::protocol::FrameStatus::FAILED);
        REQUIRE(result.accepted != 0);
        bytes = bytes.subspan(result.accepted);
        messages += result.frame.has_value();
    }

    CHECK(reader.finish() == root::protocol::FrameStatus::FINISHED);

    return messages;
}
} // namespace

TEST_CASE("public lifecycle notification capacity includes selected callbacks and reserves close") {
    LifecycleBarrier barrier;
    StateLog<api::EndpointState> changes;
    auto options = runtimeOptions();
    options.maximumPendingStateChangesPerEndpoint = 1;
    api::ApiRuntime runtime(options);
    ReleaseLifecycleBarrier release{barrier};
    auto endpoint = runtime.createEndpoint(endpointOptions());
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::CONNECTING) {
            barrier.enter();
        }
    });
    endpoint->connect("127.0.0.1:1");
    REQUIRE(barrier.awaitEntry());
    CHECK(errorCode([&] {
              endpoint->disconnect();
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(endpoint->getState() == api::EndpointState::CONNECTING);
    endpoint->close();
    barrier.open();
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK_FALSE(barrier.timedOut.load());
    const auto observed = changes.snapshot();
    checkStateOrder(observed, api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
    CHECK(observed.size() == 3);
}

TEST_CASE("public lifecycle notification slot remains charged through selected capture destruction") {
    LifecycleBarrier barrier;
    api::Endpoint::StateChangeListenerId id = 0;
    auto options = runtimeOptions();
    options.maximumPendingStateChangesPerEndpoint = 1;
    api::ApiRuntime runtime(options);
    ReleaseLifecycleBarrier release{barrier};
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        barrier.enter();
    };
    id = endpoint->addStateChangeListener([&, capture, endpoint](auto, auto current) {
        if (current == api::EndpointState::CONNECTING) {
            endpoint->removeStateChangeListener(id);
        }
    });
    capture.reset();
    endpoint->connect("127.0.0.1:1");
    REQUIRE(barrier.awaitEntry());
    CHECK(errorCode([&] {
              endpoint->disconnectAndClear();
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(endpoint->getState() == api::EndpointState::CONNECTING);
    endpoint->close();
    barrier.open();
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK_FALSE(barrier.timedOut.load());
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
}

TEST_CASE("public lifecycle preserves rapid immediate producer states and refuses overflow before mutation") {
    LifecycleBarrier barrier;
    StateLog<api::EndpointState> changes;
    std::atomic<bool> held{false};
    auto options = runtimeOptions();
    options.maximumPendingStateChangesPerEndpoint = 4;
    api::ApiRuntime runtime(options);
    ReleaseLifecycleBarrier release{barrier};
    auto endpoint = runtime.createEndpoint(endpointOptions());
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);

        if (current == api::EndpointState::CONNECTING && !held.exchange(true)) {
            barrier.enter();
        }
    });
    endpoint->connect("127.0.0.1:1");
    REQUIRE(barrier.awaitEntry());
    endpoint->disconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    endpoint->reconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    endpoint->connect("127.0.0.1:1");
    CHECK(endpoint->getState() == api::EndpointState::CONNECTING);
    endpoint->disconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(errorCode([&] {
              endpoint->connect("127.0.0.1:1");
          }) == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    endpoint->close();
    barrier.open();
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK_FALSE(barrier.timedOut.load());
    const auto observed = changes.snapshot();
    checkStateOrder(observed, api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
    REQUIRE(observed.size() == 6);
    CHECK(observed[1].second == api::EndpointState::NOT_CONNECTED);
    CHECK(observed[2].second == api::EndpointState::CONNECTING);
    CHECK(observed[3].second == api::EndpointState::NOT_CONNECTED);
}

TEST_CASE("public lifecycle full notification ring still publishes actual worker failure before retirement") {
    StateLog<api::EndpointState> changes;
    auto options = runtimeOptions();
    options.maximumPendingStateChangesPerEndpoint = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    endpoint->addStateChangeListener([&, endpoint](auto previous, auto current) {
        changes.record(previous, current);
        endpoint->close();
    });
    allocation_probe::WorkerFault fault(runtime);
    fault.arm();
    endpoint->connect("127.0.0.1:1");
    const bool intercepted = eventually([&] {
        return fault.entered.load();
    });
    CHECK(intercepted);
    CHECK(errorCode([&] {
              endpoint->disconnect();
          }) == api::ErrorCode::RESOURCE_LIMIT);
    fault.release = true;
    const auto terminal = endpoint->closeAndAwaitTermination();
    CHECK(terminal.diagnostic.code == api::ErrorCode::ALLOCATION_FAILURE);
    CHECK(terminal.diagnostic.stage == "runtime-worker");
    CHECK(endpoint->getState() == api::EndpointState::FAILED);
    const auto observed = changes.snapshot();
    checkStateOrder(observed, api::EndpointState::NOT_CONNECTED, api::EndpointState::FAILED);
    CHECK(observed.size() == 2);
}

TEST_CASE("public lifecycle reconnect replays unchanged desired Quote on a real second source epoch") {
    std::atomic<unsigned> first{0};
    std::atomic<unsigned> second{0};
    std::atomic<unsigned> unexpected{0};
    std::optional<events::Quote> retained;
    std::mutex values;
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    auto firstValues = standardValues(schema);
    auto secondValues = firstValues;

    for (std::size_t index = 0; index < secondValues.size(); ++index) {
        if (static_cast<core::SerialFieldType>(schema.records()[0].fields[index].serialTypeId) ==
            core::SerialFieldType::TIME_SECONDS) {
            secondValues[index].raw = 7;
        }
    }

    const std::vector<peer::Record> firstRecords{{u"IBM", firstValues}};
    const std::vector<peer::Record> secondRecords{{u"IBM", secondValues}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(2);
    std::size_t expectedReadBytes = 0;

    for (std::size_t index = 0; index < 2; ++index) {
        auto &script = config.sessions[index];
        script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
        script.replyBytes =
            peer::fixturePeerData(index == 0 ? firstRecords : secondRecords, core::SubscriptionFamily::TICKER, true);
        script.readThroughMessage = 11;
        script.waitForClientClose = true;
        expectedReadBytes += script.initialBytes.size() + script.replyBytes.size();
    }

    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto settings = endpointOptions();
    settings.maximumAttempts = 2;
    settings.retryDelay = 1ms;
    auto endpoint = runtime.createEndpoint(settings);
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    const auto initialStatistics = endpoint->getStatistics();
    REQUIRE(initialStatistics.ownerIdentity);
    CHECK(initialStatistics.connection.connectionAttempts.value == 0);
    std::atomic<bool> coherent{true};
    std::atomic<unsigned> observations{0};
    std::jthread reader([&](std::stop_token stop) {
        auto previous = initialStatistics;

        while (!stop.stop_requested()) {
            const auto next = endpoint->getStatistics();
            const auto &counts = next.connection;
            const auto delivery = subscription->getStatistics();

            if (delivery.admittedEvents.value !=
                delivery.deliveredEvents.value + delivery.conflatedEvents.value + delivery.discardedEvents.value +
                    delivery.callbackFailedEvents.value + delivery.evictedOverflowEvents.value +
                    delivery.pendingEvents + delivery.inFlightEvents)
                coherent = false;

            if (next.ownerIdentity != initialStatistics.ownerIdentity ||
                next.generation != initialStatistics.generation || next.observedAt < previous.observedAt ||
                counts.readBytes.value < previous.connection.readBytes.value ||
                counts.writtenBytes.value < previous.connection.writtenBytes.value ||
                counts.establishedConnections.value != counts.closedConnections.value + counts.activeConnections) {
                coherent = false;
            }

            previous = next;
            ++observations;
            std::this_thread::yield();
        }
    });
    subscription->addEventListener([&](std::span<const events::Quote> quotes) {
        for (const auto &quote : quotes) {
            if (quote.bidTimeMs == 3000) {
                {
                    std::lock_guard lock(values);
                    retained = quote;
                }

                ++first;
            } else if (quote.bidTimeMs == 7000) {
                ++second;
            } else {
                ++unexpected;
            }
        }
    });
    subscription->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return first.load() == 1;
    }));
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    REQUIRE(eventually([&] {
        return endpoint->getStatistics().connection.readDataRecords.value == 1;
    }));
    const auto firstStatistics = endpoint->getStatistics();
    const auto firstDeliveryStatistics = subscription->getStatistics();
    CHECK(firstDeliveryStatistics.admittedEvents.value == 1);
    CHECK(firstDeliveryStatistics.deliveredEvents.value == 1);
    CHECK(firstDeliveryStatistics.sourceEpoch == 1);
    CHECK(firstDeliveryStatistics.activeDesiredRevision == firstDeliveryStatistics.desiredRevision);
    CHECK(firstStatistics.connection.sourceEpoch == 1);
    endpoint->reconnect();
    CHECK(endpoint->getState() == api::EndpointState::CONNECTING);
    const bool deliveredSecond = eventually([&] {
        return second.load() == 1;
    });
    const auto failureSnapshot = source.snapshot();
    CAPTURE(static_cast<int>(endpoint->getState()));
    CAPTURE(static_cast<int>(endpoint->getDiagnostic().code));
    CAPTURE(endpoint->getDiagnostic().stage);
    CAPTURE(endpoint->getDiagnostic().nativeCode);
    CAPTURE(static_cast<int>(failureSnapshot.error));
    CAPTURE(failureSnapshot.nativeCode);
    CAPTURE(first.load());
    CAPTURE(second.load());

    for (const auto &session : failureSnapshot.sessions) {
        INFO("accepted=", session.accepted, " completed=", session.completed, " received=", session.received.size(),
             " reply=", session.replyBytesWritten);
    }

    REQUIRE(deliveredSecond);
    REQUIRE(eventually([&] {
        const auto counts = endpoint->getStatistics().connection;

        return counts.readDataRecords.value == 2 && counts.sourceEpoch == 2;
    }));
    CHECK(subscription->getStatistics().sourceEpoch == 2);
    CHECK(subscription->getStatistics().activeDesiredRevision == subscription->getStatistics().desiredRevision);
    CHECK(unexpected.load() == 0);
    CHECK(first.load() == 1);
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 2s).status ==
            api::WaitStatus::REACHED);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    const auto snapshot = source.snapshot();
    REQUIRE(snapshot.sessions.size() == 2);
    CHECK(snapshot.sessions[0].matched);
    CHECK(snapshot.sessions[1].matched);
    CHECK(checkLifecycleAdds(snapshot.sessions[0].received) == 1);
    CHECK(checkLifecycleAdds(snapshot.sessions[1].received) == 1);
    CHECK(snapshot.error == root::test::TcpPeerError::NONE);
    {
        std::lock_guard lock(values);
        REQUIRE(retained);
        CHECK(retained->bidTimeMs == 3000);
        CHECK(retained->symbol == u"IBM");
    }

    static_cast<void>(endpoint->closeAndAwaitTermination());
    checkRetiredEndpoint(endpoint);
    REQUIRE(eventually([&] {
        const auto captured = source.snapshot();

        return captured.sessions[0].completed && captured.sessions[1].completed;
    }));
    const auto finalStatistics = endpoint->getStatistics();
    const auto captured = source.snapshot();
    const auto &counts = finalStatistics.connection;
    CHECK(finalStatistics.final);
    CHECK(counts.connectionAttempts.value == 2);
    CHECK(counts.establishedConnections.value == 2);
    CHECK(counts.closedConnections.value == 2);
    CHECK(counts.activeConnections == 0);
    CHECK_FALSE(counts.sourceEpoch);
    CHECK(counts.readBytes.value == expectedReadBytes);
    CHECK(counts.writtenBytes.value == captured.sessions[0].received.size() + captured.sessions[1].received.size());
    CHECK(counts.readMessages.value == 6);
    CHECK(counts.writtenMessages.value ==
          capturedMessages(captured.sessions[0].received) + capturedMessages(captured.sessions[1].received));
    CHECK(counts.readDataRecords.value == 2);
    CHECK(counts.writtenSubscriptionRecords.value == 2);
    CHECK(counts.writtenDataRecords.availability == core::StatisticAvailability::UNSUPPORTED);
    CHECK(counts.readSubscriptionRecords.availability == core::StatisticAvailability::UNSUPPORTED);
    CHECK(counts.roundTripTime.availability == core::StatisticAvailability::UNSUPPORTED);
    const auto finalDeliveryStatistics = subscription->getStatistics();
    CHECK(finalDeliveryStatistics.final);
    CHECK(finalDeliveryStatistics.ownerIdentity == firstDeliveryStatistics.ownerIdentity);
    CHECK(finalDeliveryStatistics.generation == firstDeliveryStatistics.generation);
    CHECK(finalDeliveryStatistics.admittedEvents.value == 2);
    CHECK(finalDeliveryStatistics.deliveredEvents.value == 2);
    CHECK(finalDeliveryStatistics.callbackCalls.value == 2);
    CHECK(finalDeliveryStatistics.droppedEvents.value == 0);
    CHECK(finalDeliveryStatistics.pendingEvents == 0);
    CHECK(finalDeliveryStatistics.inFlightEvents == 0);
    CHECK_FALSE(finalDeliveryStatistics.activeDesiredRevision);
    runtime.closeAndAwaitTermination();
    CHECK(endpoint->getStatistics().observedAt == finalStatistics.observedAt);
    CHECK(firstStatistics.connection.readDataRecords.value == 1);
    CHECK(coherent.load());
    CHECK(observations.load() != 0);
}

TEST_CASE("public lifecycle offline cached Quote replay survives pause and clear fences every later selection") {
    std::atomic<unsigned> initial{0};
    std::atomic<unsigned> cached{0};
    std::atomic<unsigned> fresh{0};
    std::atomic<unsigned> staleAfterClear{0};
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    auto oldValues = standardValues(schema);
    auto newValues = oldValues;

    for (std::size_t index = 0; index < newValues.size(); ++index) {
        if (static_cast<core::SerialFieldType>(schema.records()[0].fields[index].serialTypeId) ==
            core::SerialFieldType::TIME_SECONDS) {
            newValues[index].raw = 7;
        }
    }

    const std::vector<peer::Record> oldRecords{{u"IBM", oldValues}};
    const std::vector<peer::Record> newRecords{{u"IBM", newValues}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(2);

    for (std::size_t index = 0; index < config.sessions.size(); ++index) {
        auto &script = config.sessions[index];
        script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
        script.replyBytes =
            peer::fixturePeerData(index == 0 ? oldRecords : newRecords, core::SubscriptionFamily::TICKER, true);
        script.readThroughMessage = 11;
        script.waitForClientClose = true;
    }

    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto settings = endpointOptions();
    settings.maximumAttempts = 2;
    settings.retryDelay = 1ms;
    auto endpoint = runtime.createEndpoint(settings);
    auto first = endpoint->getFeed()->createSubscription<events::Quote>();
    first->addEventListener([&](std::span<const events::Quote> quotes) {
        for (const auto &quote : quotes) {
            if (quote.bidTimeMs == 3000)
                ++initial;
            else if (quote.bidTimeMs == 7000)
                ++fresh;
        }
    });
    first->addSymbols("IBM");
    const auto address = "127.0.0.1:" + std::to_string(source.port());
    endpoint->connect(address);
    REQUIRE(eventually([&] {
        return initial.load() == 1;
    }));
    endpoint->disconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    auto offline = endpoint->getFeed()->createSubscription({api::EventType::QUOTE});
    offline->addEventListener([&](std::span<const api::RegisteredEvent> batch) {
        for (const auto &event : batch) {
            const auto *quote = std::get_if<events::Quote>(&event);
            REQUIRE(quote);

            if (quote->bidTimeMs == 3000)
                ++cached;
            else if (quote->bidTimeMs == 7000)
                ++fresh;
        }
    });
    offline->addSymbols("IBM");
    // This actual cached callback is enabled only after native SUSPENDED quiescence; NOT_CONNECTED alone is not ACK.
    REQUIRE(eventually([&] {
        return cached.load() == 1;
    }));
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    endpoint->reconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    endpoint->disconnectAndClear();
    auto afterClear = endpoint->getFeed()->createSubscription({api::EventType::QUOTE});
    afterClear->addEventListener([&](std::span<const api::RegisteredEvent> batch) {
        for (const auto &event : batch) {
            const auto *quote = std::get_if<events::Quote>(&event);
            REQUIRE(quote);

            if (quote->bidTimeMs == 3000)
                ++staleAfterClear;
            else if (quote->bidTimeMs == 7000)
                ++fresh;
        }
    });
    afterClear->addSymbols("IBM");
    endpoint->connect(address);
    const bool freshDelivered = eventually([&] {
        return fresh.load() == 3;
    });
    CAPTURE(fresh.load());
    CAPTURE(staleAfterClear.load());
    CAPTURE(static_cast<int>(endpoint->getState()));
    CAPTURE(endpoint->getDiagnostic().stage);
    CAPTURE(endpoint->getDiagnostic().nativeCode);
    CAPTURE(first->getDiagnostic().stage);
    CAPTURE(offline->getDiagnostic().stage);
    CAPTURE(afterClear->getDiagnostic().stage);
    CAPTURE(afterClear->getDiagnostic().detailCode);
    CAPTURE(static_cast<int>(afterClear->getDiagnostic().code));
    const auto failedPeer = source.snapshot();
    CAPTURE(static_cast<int>(failedPeer.error));
    CAPTURE(failedPeer.nativeCode);
    CAPTURE(failedPeer.sessions[0].completed);
    CAPTURE(failedPeer.sessions[1].accepted);
    CAPTURE(failedPeer.sessions[1].received.size());
    REQUIRE(freshDelivered);
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 2s).status ==
            api::WaitStatus::REACHED);
    CHECK(initial.load() == 1);
    CHECK(cached.load() == 1);
    CHECK(staleAfterClear.load() == 0);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    const auto snapshot = source.snapshot();
    REQUIRE(snapshot.sessions.size() == 2);
    CHECK(snapshot.sessions[0].matched);
    CHECK(snapshot.sessions[1].matched);
    CHECK(checkLifecycleAdds(snapshot.sessions[0].received) == 1);
    CHECK(checkLifecycleAdds(snapshot.sessions[1].received) == 1);
    CHECK(snapshot.error == root::test::TcpPeerError::NONE);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    checkRetiredEndpoint(endpoint);
}

TEST_CASE("public lifecycle clear inside selected Quote fanout permits its remainder without stale state publication") {
    std::atomic<unsigned> firstCalls{0};
    std::atomic<unsigned> remainder{0};
    StateLog<api::EndpointState> changes;
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &script = config.sessions[0];
    script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
    script.replyBytes = peer::fixturePeerData(records, core::SubscriptionFamily::TICKER, true);
    script.readThroughMessage = 11;
    script.waitForClientClose = true;
    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto quotes = endpoint->getFeed()->createSubscription<events::Quote>();
    quotes->addEventListener([&, endpoint](std::span<const events::Quote> batch) {
        CHECK(batch.size() == 1);
        endpoint->disconnectAndClear();
        CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
        ++firstCalls;
    });
    quotes->addEventListener([&, endpoint](std::span<const events::Quote> batch) {
        CHECK(firstCalls.load() == 1);
        REQUIRE(batch.size() == 1);
        CHECK(batch[0].bidTimeMs == 3000);
        CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
        ++remainder;
    });
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);
    });
    quotes->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return remainder.load() == 1;
    }));
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    const auto closed = endpoint->closeAndAwaitTermination();
    static_cast<void>(closed);
    const auto observed = changes.snapshot();
    checkStateOrder(observed, api::EndpointState::NOT_CONNECTED, endpoint->getState());
    bool disconnected = false;

    for (const auto &transition : observed) {
        if (transition.second == api::EndpointState::NOT_CONNECTED)
            disconnected = true;
        else if (disconnected)
            CHECK(transition.second != api::EndpointState::CONNECTED);
    }

    CHECK(disconnected);
    CHECK(firstCalls.load() == 1);
    CHECK(remainder.load() == 1);
    CHECK(source.snapshot().error == root::test::TcpPeerError::NONE);
    checkRetiredEndpoint(endpoint);
}

TEST_CASE("public lifecycle inactive and terminal controls are idempotent without a native peer") {
    StateLog<api::EndpointState> changes;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    endpoint->addStateChangeListener([&](auto previous, auto current) {
        changes.record(previous, current);
    });
    endpoint->disconnect();
    endpoint->disconnectAndClear();
    endpoint->disconnectAndClear();
    endpoint->reconnect();
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    CHECK(changes.snapshot().empty());
    const auto closed = endpoint->closeAndAwaitTermination();
    CHECK(closed.diagnostic.code == api::ErrorCode::NONE);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    const auto before = changes.snapshot();
    endpoint->disconnect();
    endpoint->disconnectAndClear();
    endpoint->reconnect();
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(changes.snapshot() == before);
    checkStateOrder(before, api::EndpointState::NOT_CONNECTED, api::EndpointState::CLOSED);
}

TEST_CASE("public feed uses canonical Candle object identity in its atomic desired subscription map") {
    auto options = runtimeOptions();
    options.maximumSymbolsPerSubscription = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    auto first = events::CandleSymbol::valueOf(u"AAPL&Q{=15Minutes,price=BID}");
    auto equivalent = events::CandleSymbol::valueOf(u"AAPL&Q{=15m,price=bid}");
    subscription->setSymbols({first, equivalent});
    subscription->addSymbols("AAPL&Q{=15m,price=bid}");
    CHECK(errorCode([&] {
              subscription->addSymbols("OTHER");
          }) == api::ErrorCode::RESOURCE_LIMIT);
    auto invalid = events::CandleSymbol::valueOf(std::u16string{u'B', static_cast<char16_t>(0xd800)});
    CHECK(errorCode([&] {
              subscription->setSymbols({first, invalid});
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              subscription->addSymbols("OTHER");
          }) == api::ErrorCode::RESOURCE_LIMIT);
    auto nullSymbol = events::CandleSymbol::valueOf(std::nullopt);
    CHECK(errorCode([&] {
              subscription->setSymbols({nullSymbol});
          }) == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(errorCode([&] {
              subscription->removeSymbols(nullSymbol);
          }) == api::ErrorCode::INVALID_ARGUMENT);
    subscription->removeSymbols(equivalent);
    subscription->addSymbols("OTHER");
    subscription->setSymbols({});
    subscription->addSymbols({first, equivalent});
    subscription->removeSymbols(equivalent);
    subscription->addSymbols("OTHER");
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("decorated selector copy assignment preserves typed invariant on allocation refusal") {
    using Selector = api::SubscriptionSelector;
    static_assert(std::is_nothrow_move_constructible_v<Selector>);
    static_assert(std::is_nothrow_move_assignable_v<Selector>);
    auto target = Selector::indexed(u"A", {7, std::nullopt});
    const auto source = Selector::timeSeries(std::u16string(128, u'B'), std::numeric_limits<std::int64_t>::max());
    const auto before = target;
    bool refused = false;
    allocation_probe::failNext = true;
    try {
        target = source;
    } catch (const std::bad_alloc &) {
        refused = true;
    }
    allocation_probe::failNext = false;
    REQUIRE(refused);
    CHECK(target.sameDemand(before));
    CHECK(target.kind() == api::SubscriptionSymbolKind::INDEXED);
    CHECK(target.sourceId() == 7);
    CHECK_FALSE(target.fromTime());
    target = source;
    CHECK(target.sameDemand(source));
    const auto copied = target;
    target = target;
    CHECK(target.sameDemand(copied));
    auto moved = std::move(target);
    CHECK(moved.sameDemand(copied));
    target = std::move(moved);
    CHECK(target.sameDemand(copied));
    target = std::move(target);
    // Moved-from and self-moved values stay valid with unspecified text, not guaranteed previous contents.
    CHECK(target.kind() == api::SubscriptionSymbolKind::TIME_SERIES);
    CHECK(target.sourceId() == 0);
    CHECK(target.fromTime().has_value());
    target = Selector::plain(u"RECOVERED");
    CHECK(target.sameDemand(Selector::plain(u"RECOVERED")));
}

TEST_CASE("detached explicit All selector keeps CLOSED rejection after endpoint owner retirement") {
    std::shared_ptr<api::Subscription<events::Quote>> detached;
    const auto all = api::SubscriptionSelector::all();
    {
        api::RuntimeOptions limits;
        limits.dnsServers.push_back({{127, 0, 0, 1}});
        api::ApiRuntime runtime(limits);
        api::EndpointOptions options;
        options.contract = api::DeliveryContract::STREAM;
        options.enableWildcard = true;
        auto endpoint = runtime.createEndpoint(options);
        detached = endpoint->getFeed()->createSubscription<events::Quote>();
        detached->setSelectors(std::span(&all, 1));
        runtime.closeAndAwaitTermination();
    }
    CHECK(errorCode([&] {
              detached->addSelectors(std::span(&all, 1));
          }) == api::ErrorCode::CLOSED);
    CHECK(errorCode([&] {
              detached->removeSelector(all);
          }) == api::ErrorCode::CLOSED);
}

TEST_CASE("public subscription observation owns one committed logical snapshot and normalized types") {
    api::SubscriptionSnapshot retained;
    std::shared_ptr<api::Subscription<api::RegisteredEvent>> detached;
    {
        api::ApiRuntime runtime(runtimeOptions());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        detached = endpoint->getFeed()->createSubscription(
            {api::EventType::PROFILE, api::EventType::QUOTE, api::EventType::QUOTE});
        REQUIRE(detached->getEventTypes().size() == 2);
        CHECK(detached->containsEventType(api::EventType::PROFILE));
        CHECK_FALSE(detached->containsEventType(static_cast<api::EventType>(900)));
        detached->setSymbols({"AAPL", "IBM"});
        retained = detached->getSnapshot();
        CHECK(retained.revision == 1);
        detached->setSymbols({"IBM", "AAPL"});
        CHECK(detached->getSnapshot().revision == 1);
        detached->addSymbols("IBM");
        detached->removeSymbols("ABSENT");
        CHECK(detached->getSnapshot().revision == 1);
        detached->setSymbols({"MSFT"});
        CHECK(detached->getSnapshot().revision == 2);
        CHECK(endpoint->closeAndAwaitTermination().drained);
    }
    REQUIRE(retained.symbols.size() == 2);
    CHECK(retained.symbols[0].eventSymbol() == u"AAPL");
    CHECK(detached->getSnapshot().revision == 2);
    REQUIRE(detached->getSnapshot().symbols.size() == 1);
    CHECK(detached->getSnapshot().symbols[0].eventSymbol() == u"MSFT");
}

TEST_CASE("public subscription observation is synchronous offline and removes before adding one revision") {
    std::vector<std::pair<api::SubscriptionChangeKind, std::uint64_t>> changes;
    const auto owner = std::this_thread::get_id();
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addSymbols("AAPL");
    const auto id = subscription->addChangeListener([&](const api::SubscriptionChange &change) {
        CHECK(std::this_thread::get_id() == owner);
        CHECK(change.snapshot.revision == subscription->getSnapshot().revision);
        changes.emplace_back(change.kind, change.snapshot.revision);
    });
    REQUIRE(changes.size() == 1);
    CHECK(changes[0].first == api::SubscriptionChangeKind::ADDED);
    subscription->setSymbols({"IBM"});
    REQUIRE(changes.size() == 3);
    CHECK(changes[1] == std::pair{api::SubscriptionChangeKind::REMOVED, std::uint64_t{2}});
    CHECK(changes[2] == std::pair{api::SubscriptionChangeKind::ADDED, std::uint64_t{2}});
    subscription->removeChangeListener(id);
    CHECK(changes.back().first == api::SubscriptionChangeKind::CLOSED);
    const auto count = changes.size();
    subscription->removeChangeListener(id);
    subscription->clearSymbols();
    CHECK(changes.size() == count);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation defers self removal and suppresses the later addition phase") {
    unsigned depth = 0;
    unsigned maximumDepth = 0;
    std::vector<api::SubscriptionChangeKind> first;
    std::vector<api::SubscriptionChangeKind> second;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    api::Subscription<events::Quote>::ChangeListenerId firstId = 0;
    firstId = subscription->addChangeListener([&](const auto &change) {
        maximumDepth = std::max(maximumDepth, ++depth);
        first.push_back(change.kind);

        if (change.kind == api::SubscriptionChangeKind::REMOVED) {
            subscription->removeChangeListener(firstId);
            CHECK(change.snapshot.symbols[0].eventSymbol() == u"IBM");
        }

        --depth;
    });
    subscription->addChangeListener([&](const auto &change) {
        maximumDepth = std::max(maximumDepth, ++depth);
        second.push_back(change.kind);
        --depth;
    });
    subscription->addSymbols("AAPL");
    first.clear();
    second.clear();
    subscription->setSymbols({"IBM"});
    CHECK(maximumDepth == 1);
    REQUIRE(first.size() == 2);
    CHECK(first[0] == api::SubscriptionChangeKind::REMOVED);
    CHECK(first[1] == api::SubscriptionChangeKind::CLOSED);
    REQUIRE(second.size() == 2);
    CHECK(second[0] == api::SubscriptionChangeKind::REMOVED);
    CHECK(second[1] == api::SubscriptionChangeKind::ADDED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation bounds an entire reentrant turn before committing") {
    unsigned calls = 0;
    api::ErrorCode refused = api::ErrorCode::NONE;
    auto options = runtimeOptions();
    options.maximumSubscriptionChangesPerTurn = 3;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    constexpr std::string_view SYMBOLS[] = {"A", "B", "C", "D"};
    subscription->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        ++calls;
        refused = errorCode([&] {
            subscription->addSymbols(SYMBOLS[calls]);
        });
    });
    subscription->addSymbols("A");
    CHECK(calls == 3);
    CHECK(refused == api::ErrorCode::WORK_LIMIT);
    CHECK(subscription->getSnapshot().revision == 3);
    CHECK(subscription->getSnapshot().symbols.size() == 3);
    subscription->clearSymbols();
    CHECK(subscription->getSnapshot().revision == 4);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation keeps executing capacity and reserves close without waiting") {
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false;
    bool release = false;
    std::atomic<unsigned> closed{0};
    auto options = runtimeOptions();
    options.maximumChangeListenersPerSubscription = 1;
    options.maximumPendingSubscriptionChanges = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    const auto id = subscription->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::CLOSED) {
            ++closed;

            return;
        }

        std::unique_lock lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait(lock, [&] {
            return release;
        });
    });
    std::thread producer([&] {
        subscription->addSymbols("AAPL");
    });
    {
        std::unique_lock lock(mutex);
        const bool reached = condition.wait_for(lock, 3s, [&] {
            return entered;
        });
        CHECK(reached);
    }
    const auto refused = errorCode([&] {
        subscription->addSymbols("IBM");
    });
    const auto observed = subscription->getSnapshot();
    subscription->removeChangeListener(id);
    const auto capacity = errorCode([&] {
        subscription->addChangeListener([](const auto &) {
        });
    });
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    producer.join();
    CHECK(refused == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(capacity == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(observed.revision == 1);
    CHECK(observed.symbols.size() == 1);
    CHECK(closed.load() == 1);
    subscription->addChangeListener([](const auto &) {
    });
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation records advisory throws and rejects self waits before ready") {
    unsigned delivered = 0;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addChangeListener([](const auto &) {
        throw std::runtime_error("observer failure");
    });
    subscription->addChangeListener([&](const auto &) {
        ++delivered;
        CHECK(errorCode([&] {
                  static_cast<void>(subscription->awaitState(subscription->getState()));
              }) == api::ErrorCode::INVALID_STATE);
        CHECK(errorCode([&] {
                  static_cast<void>(endpoint->closeAndAwaitTermination());
              }) == api::ErrorCode::INVALID_STATE);
        CHECK(errorCode([&] {
                  runtime.closeAndAwaitTermination();
              }) == api::ErrorCode::INVALID_STATE);
    });
    subscription->addSymbols("AAPL");
    CHECK(delivered == 1);
    CHECK(subscription->getChangeDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(subscription->getState() == api::SubscriptionState::CONFIGURED);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(delivered == 2);
}

TEST_CASE("public subscription observation failure preserves revision and listener admission") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addSymbols("AAPL");
    unsigned calls = 0;
    allocation_probe::failNext = true;
    const auto admission = errorCode([&] {
        subscription->addChangeListener([&](const auto &) {
            ++calls;
        });
    });
    CHECK(admission == api::ErrorCode::ALLOCATION_FAILURE);
    CHECK(calls == 0);
    CHECK(subscription->getSnapshot().revision == 1);
    subscription->addChangeListener([&](const auto &) {
        ++calls;
    });
    CHECK(calls == 1);
    allocation_probe::failNext = true;
    const auto mutation = errorCode([&] {
        subscription->setSymbols({"IBM"});
    });
    CHECK(mutation == api::ErrorCode::ALLOCATION_FAILURE);
    CHECK(subscription->getSnapshot().revision == 1);
    CHECK(calls == 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation retires captures outside locks and charges final destruction") {
    bool destroyed = false;
    api::ErrorCode charged = api::ErrorCode::NONE;
    auto limits = runtimeOptions();
    limits.maximumChangeListenersPerSubscription = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        CHECK(subscription->getSnapshot().revision == 0);
        charged = errorCode([&] {
            subscription->addChangeListener([](const auto &) {
            });
        });
        subscription->clearSymbols();
        destroyed = true;
    };
    const auto id = subscription->addChangeListener([capture](const auto &) {
    });
    capture.reset();
    subscription->removeChangeListener(id);
    CHECK(destroyed);
    CHECK(charged == api::ErrorCode::RESOURCE_LIMIT);
    subscription->addChangeListener([](const auto &) {
    });
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("endpoint statistics final waits for subscription observer and capture retirement") {
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    bool closed = false;
    bool captureRetired = false;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        CHECK(closed);
        CHECK_FALSE(endpoint->getStatistics().final);
        CHECK_FALSE(subscription->getStatistics().final);
        captureRetired = true;
    };
    subscription->addChangeListener([&, capture](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::CLOSED) {
            CHECK_FALSE(endpoint->getStatistics().final);
            CHECK_FALSE(subscription->getStatistics().final);
            closed = true;
        }
    });
    capture.reset();
    CHECK_FALSE(endpoint->getStatistics().final);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(closed);
    CHECK(captureRetired);
    CHECK(endpoint->getStatistics().final);
    CHECK(subscription->getStatistics().final);
    CHECK(subscription->getStatistics().callbackCalls.value == 0);
}

TEST_CASE("public subscription observation is deferred until the complete real data selection returns") {
    std::atomic<bool> inData{false};
    std::atomic<unsigned> remainder{0};
    std::atomic<unsigned> observedRemoval{0};
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &script = config.sessions[0];
    script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
    script.replyBytes = peer::fixturePeerData(records, core::SubscriptionFamily::TICKER, true);
    script.readThroughMessage = 11;
    script.waitForClientClose = true;
    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addChangeListener([&](const auto &change) {
        CHECK_FALSE(inData.load());

        if (change.kind == api::SubscriptionChangeKind::REMOVED) {
            CHECK(remainder.load() == 1);
            CHECK(change.snapshot.symbols.empty());
            ++observedRemoval;
        }
    });
    subscription->addEventListener([&](auto batch) {
        inData.store(true);
        REQUIRE(batch.size() == 1);
        subscription->clearSymbols();
        CHECK(observedRemoval.load() == 0);
    });
    subscription->addEventListener([&](auto batch) {
        REQUIRE(batch.size() == 1);
        CHECK(observedRemoval.load() == 0);
        ++remainder;
        inData.store(false);
    });
    subscription->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return observedRemoval.load() == 1;
    }));
    endpoint->disconnect();
    const auto before = subscription->getSnapshot().revision;
    subscription->addSymbols("AAPL");
    CHECK(subscription->getSnapshot().revision == before + 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(source.snapshot().error == root::test::TcpPeerError::NONE);
}

TEST_CASE("public subscription observation preserves typed identities while demand is shared") {
    std::atomic<unsigned> firstCalls{0};
    std::atomic<unsigned> secondCalls{0};
    std::atomic<unsigned> removals{0};
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &script = config.sessions[0];
    script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
    script.replyBytes = peer::fixturePeerData(records, core::SubscriptionFamily::TICKER, true);
    script.readThroughMessage = 11;
    script.waitForClientClose = true;
    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto first = endpoint->getFeed()->createSubscription<events::Quote>();
    auto second = endpoint->getFeed()->createSubscription<events::Quote>();
    const auto indexed = api::SubscriptionSelector::indexed(u"IBM", {0, u"ignored"});
    const std::array selectors{api::SubscriptionSelector::plain(u"IBM"), indexed};
    first->setSelectors(selectors);
    second->addSymbols("IBM");
    const auto observerId = first->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::REMOVED) {
            CHECK(change.symbols.size() == 1);
            CHECK(change.symbols[0].kind() == api::SubscriptionSymbolKind::PLAIN);
            CHECK(change.snapshot.symbols.size() == 1);
            ++removals;
        }
    });
    first->addEventListener([&](auto batch) {
        CHECK(batch.size() == 1);
        first->removeSymbols("IBM");
        ++firstCalls;
    });
    second->addEventListener([&](auto batch) {
        CHECK(batch.size() == 1);
        ++secondCalls;
    });
    endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return secondCalls.load() == 1 && removals.load() == 1;
    }));
    CHECK(firstCalls.load() == 1);
    CHECK(first->getSnapshot().symbols.size() == 1);
    CHECK(second->getSnapshot().symbols.size() == 1);
    first->removeChangeListener(observerId);
    first->clearSymbols();
    CHECK(second->getSnapshot().symbols.size() == 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(source.snapshot().error == root::test::TcpPeerError::NONE);
}

TEST_CASE("public subscription observation matches pinned production Java membership phases") {
    std::ifstream oracle(ORG_TTLDTOR_QDCXX_SUBSCRIPTION_OBSERVER_FIXTURE);
    REQUIRE(oracle.is_open());
    std::vector<std::string> expected;
    std::string line;

    while (std::getline(oracle, line)) {
        if (!line.starts_with("ordinary\tL.")) {
            continue;
        }

        auto value = line.substr(std::string_view("ordinary\t").size());
        const auto monitor = value.find(";monitor=");

        if (monitor != std::string::npos) {
            value.erase(monitor);
        }

        if (value.starts_with("L.closed\t")) {
            value = "L.closed";
        }

        expected.push_back(std::move(value));
    }

    REQUIRE(expected.size() == 6);
    const auto values = [](std::span<const api::SubscriptionSelector> symbols) {
        std::vector<std::string> names;

        for (const auto &symbol : symbols) {
            const auto text = symbol.eventSymbol();
            std::string name = "String:";

            for (const auto unit : text) {
                CHECK(unit <= 0x7f);
                name.push_back(static_cast<char>(unit));
            }

            names.push_back(std::move(name));
        }

        std::ranges::sort(names);
        std::string result = "[";

        for (const auto &name : names) {
            if (result.size() != 1) {
                result += ", ";
            }

            result += name;
        }

        return result + "]";
    };
    std::vector<std::string> observed;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    const auto id = subscription->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::CLOSED) {
            observed.emplace_back("L.closed");

            return;
        }

        observed.push_back(
            std::string(change.kind == api::SubscriptionChangeKind::ADDED ? "L.added\t" : "L.removed\t") +
            values(change.symbols) + ";current=" + values(change.snapshot.symbols));
    });
    subscription->addSymbols({"A", "B"});
    subscription->addSymbols("A");
    subscription->setSymbols({"B", "C"});
    subscription->removeSymbols("ABSENT");
    subscription->removeSymbols("B");
    subscription->clearSymbols();
    subscription->clearSymbols();
    subscription->removeChangeListener(id);
    subscription->removeChangeListener(id);
    subscription->addSymbols("D");
    CHECK(observed == expected);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation refuses a two phase replace before publishing either phase") {
    unsigned calls = 0;
    auto limits = runtimeOptions();
    limits.maximumPendingSubscriptionChanges = 1;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addSymbols("AAPL");
    subscription->addChangeListener([&](const auto &) {
        ++calls;
    });
    CHECK(calls == 1);
    const auto code = errorCode([&] {
        subscription->setSymbols({"IBM"});
    });
    CHECK(code == api::ErrorCode::RESOURCE_LIMIT);
    const auto snapshot = subscription->getSnapshot();
    CHECK(snapshot.revision == 1);
    REQUIRE(snapshot.symbols.size() == 1);
    CHECK(snapshot.symbols[0].eventSymbol() == u"AAPL");
    CHECK(calls == 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation shares finite nonrecursive work across sibling subscriptions") {
    unsigned depth = 0;
    unsigned maximumDepth = 0;
    api::ErrorCode bounded = api::ErrorCode::NONE;
    auto limits = runtimeOptions();
    limits.maximumSubscriptionChangesPerTurn = 2;
    api::ApiRuntime runtime(limits);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto first = endpoint->getFeed()->createSubscription<events::Quote>();
    auto second = endpoint->getFeed()->createSubscription<events::Profile>();
    first->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        maximumDepth = std::max(maximumDepth, ++depth);
        second->addSymbols("B");
        --depth;
    });
    second->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        maximumDepth = std::max(maximumDepth, ++depth);
        bounded = errorCode([&] {
            first->addSymbols("C");
        });
        --depth;
    });
    first->addSymbols("A");
    CHECK(maximumDepth == 1);
    CHECK(bounded == api::ErrorCode::WORK_LIMIT);
    CHECK(first->getSnapshot().revision == 1);
    CHECK(second->getSnapshot().revision == 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation preserves cross runtime mutations with one finite causal root") {
    unsigned depth = 0;
    unsigned maximumDepth = 0;
    unsigned secondCalls = 0;
    api::ErrorCode bounded = api::ErrorCode::NONE;
    std::vector<std::u16string> order;
    auto bootstrap = root::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto firstLimits = runtimeOptions();
    firstLimits.maximumSubscriptionChangesPerTurn = 3;
    api::ApiRuntime firstRuntime(*bootstrap, firstLimits);
    api::ApiRuntime secondRuntime(*bootstrap, runtimeOptions());
    auto firstEndpoint = firstRuntime.createEndpoint(endpointOptions());
    auto secondEndpoint = secondRuntime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Quote>();
    first->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        maximumDepth = std::max(maximumDepth, ++depth);
        const std::u16string symbol(change.symbols[0].eventSymbol());
        order.push_back(symbol);

        if (symbol == u"A") {
            second->addSymbols("B");
            CHECK(second->getSnapshot().revision == 1);
            CHECK(secondCalls == 0);
        } else if (symbol == u"C") {
            bounded = errorCode([&] {
                second->addSymbols("REFUSED");
            });
        }

        --depth;
    });
    second->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        maximumDepth = std::max(maximumDepth, ++depth);
        ++secondCalls;
        order.emplace_back(change.symbols[0].eventSymbol());
        first->addSymbols("C");
        --depth;
    });
    first->addSymbols("A");
    CHECK(maximumDepth == 1);
    CHECK(order == std::vector<std::u16string>{u"A", u"B", u"C"});
    CHECK(bounded == api::ErrorCode::WORK_LIMIT);
    CHECK(first->getSnapshot().revision == 2);
    CHECK(second->getSnapshot().revision == 1);
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(secondEndpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation close remains a fence until offline owner retirement") {
    unsigned calls = 0;
    unsigned closed = 0;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addChangeListener([&](const auto &change) {
        ++calls;

        if (change.kind == api::SubscriptionChangeKind::CLOSED) {
            ++closed;
        }
    });
    subscription->addSymbols("AAPL");
    CHECK(calls == 1);
    subscription->close();
    CHECK(closed == 0);
    CHECK(errorCode([&] {
              subscription->addSymbols("IBM");
          }) == api::ErrorCode::CLOSED);
    CHECK(subscription->getSnapshot().revision == 1);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(closed == 1);
    subscription->close();
    CHECK(closed == 1);
}

TEST_CASE("public subscription observation holds cross runtime selection until the source callback returns") {
    std::mutex mutex;
    std::condition_variable condition;
    bool busy = false;
    bool release = false;
    bool producerReturned = false;
    std::atomic<unsigned> crossCalls{0};
    api::ErrorCode causalLimit = api::ErrorCode::NONE;
    auto bootstrap = root::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto limits = runtimeOptions();
    limits.maximumSubscriptionChangesPerTurn = 2;
    api::ApiRuntime firstRuntime(*bootstrap, limits);
    api::ApiRuntime secondRuntime(*bootstrap, runtimeOptions());
    auto firstEndpoint = firstRuntime.createEndpoint(endpointOptions());
    auto secondEndpoint = secondRuntime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Quote>();
    second->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        if (change.symbols[0].eventSymbol() == u"BUSY") {
            std::unique_lock lock(mutex);
            busy = true;
            condition.notify_all();
            condition.wait(lock, [&] {
                return release;
            });
        } else {
            ++crossCalls;
            causalLimit = errorCode([&] {
                first->addSymbols("ROOT-CAP-REFUSAL");
            });
        }
    });
    std::thread producer([&] {
        second->addSymbols("BUSY");
        std::lock_guard lock(mutex);
        producerReturned = true;
        condition.notify_all();
    });
    {
        std::unique_lock lock(mutex);
        CHECK(condition.wait_for(lock, 3s, [&] {
            return busy;
        }));
    }
    first->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        second->addSymbols("CROSS");
        {
            std::lock_guard lock(mutex);
            release = true;
        }
        condition.notify_all();
        std::unique_lock lock(mutex);
        CHECK(condition.wait_for(lock, 3s, [&] {
            return producerReturned;
        }));
        CHECK(crossCalls.load() == 0);
    });
    first->addSymbols("A");
    producer.join();
    CHECK(crossCalls.load() == 1);
    CHECK(causalLimit == api::ErrorCode::WORK_LIMIT);
    CHECK(first->getSnapshot().revision == 1);
    CHECK(second->getSnapshot().revision == 2);
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(secondEndpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation continues from real data into an offline foreign runtime") {
    std::atomic<bool> inData{false};
    std::atomic<unsigned> foreignCalls{0};
    auto bootstrap = root::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::ApiRuntime firstRuntime(*bootstrap, runtimeOptions());
    api::ApiRuntime secondRuntime(*bootstrap, runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &script = config.sessions[0];
    script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
    script.replyBytes = peer::fixturePeerData(records, core::SubscriptionFamily::TICKER, true);
    script.readThroughMessage = 11;
    script.waitForClientClose = true;
    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto firstEndpoint = firstRuntime.createEndpoint(endpointOptions());
    auto secondEndpoint = secondRuntime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Profile>();
    second->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::ADDED) {
            CHECK_FALSE(inData.load());
            CHECK(second->getState() == api::SubscriptionState::CONFIGURED);
            ++foreignCalls;
        }
    });
    first->addEventListener([&](auto) {
        inData.store(true);
        second->addSymbols("FOREIGN");
        CHECK(foreignCalls.load() == 0);
    });
    first->addEventListener([&](auto) {
        inData.store(false);
    });
    first->addSymbols("IBM");
    firstEndpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return foreignCalls.load() == 1;
    }));
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(secondEndpoint->closeAndAwaitTermination().drained);
    CHECK(source.snapshot().error == root::test::TcpPeerError::NONE);
}

TEST_CASE("public subscription observation keeps detached continuation captures through reentrant destruction") {
    unsigned depth = 0;
    unsigned maximumDepth = 0;
    bool inDestructor = false;
    bool destroyed = false;
    unsigned foreignAdded = 0;
    unsigned foreignClosed = 0;
    auto bootstrap = root::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::ApiRuntime firstRuntime(*bootstrap, runtimeOptions());
    api::ApiRuntime secondRuntime(*bootstrap, runtimeOptions());
    auto firstEndpoint = firstRuntime.createEndpoint(endpointOptions());
    auto secondEndpoint = secondRuntime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Quote>();
    std::weak_ptr<api::Subscription<events::Quote>> weak = second;
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        inDestructor = true;
        CHECK(first->getSnapshot().revision == 1);
        first->addSymbols("CAPTURE");
        destroyed = true;
        inDestructor = false;
    };
    second->addChangeListener([&, capture](const auto &change) {
        maximumDepth = std::max(maximumDepth, ++depth);

        if (change.kind == api::SubscriptionChangeKind::CLOSED) {
            ++foreignClosed;
        } else {
            ++foreignAdded;
        }

        --depth;
    });
    capture.reset();
    first->addChangeListener([&](const auto &change) {
        if (change.kind != api::SubscriptionChangeKind::ADDED) {
            return;
        }

        maximumDepth = std::max(maximumDepth, ++depth);
        CHECK_FALSE(inDestructor);

        if (change.symbols[0].eventSymbol() == u"A") {
            second->addSymbols("B");
            second.reset();
            CHECK(weak.expired());
            CHECK(foreignClosed == 0);
        }

        --depth;
    });
    first->addSymbols("A");
    CHECK(maximumDepth == 1);
    CHECK(foreignAdded == 0);
    CHECK(foreignClosed == 1);
    CHECK(destroyed);
    CHECK(first->getSnapshot().revision == 2);
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(secondEndpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public subscription observation initializes a reentrant listener before its ordinary deltas") {
    std::vector<std::uint64_t> revisions;
    unsigned registrations = 0;
    api::ApiRuntime runtime(runtimeOptions());
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto subscription = endpoint->getFeed()->createSubscription<events::Quote>();
    subscription->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::REMOVED && registrations++ == 0) {
            subscription->addChangeListener([&](const auto &next) {
                if (next.kind != api::SubscriptionChangeKind::CLOSED) {
                    revisions.push_back(next.snapshot.revision);
                }
            });
        }
    });
    subscription->addSymbols("A");
    subscription->setSymbols({"B"});
    CHECK(revisions == std::vector<std::uint64_t>{2});
    subscription->addSymbols("C");
    CHECK(revisions == std::vector<std::uint64_t>{2, 3});
    CHECK(endpoint->closeAndAwaitTermination().drained);
}

TEST_CASE("public endpoint classifier preserves custom registry and detached immutable options") {
    api::ApiRuntime runtime(runtimeOptions());
    const std::array categories{api::FeedFilterCategory{u"A", u"A*"}, api::FeedFilterCategory{u"other", u"*"}};
    auto ordinary = runtime.createEndpoint(endpointOptions());
    const auto plain = api::SubscriptionSelector::plain(u"AAPL");
    auto defaultFilter = ordinary->createFilter(categories);
    CHECK(defaultFilter.accept(api::EventType::QUOTE, plain));
    REQUIRE(defaultFilter.getCategory(api::EventType::QUOTE, plain));
    CHECK(*defaultFilter.getCategory(api::EventType::QUOTE, plain) == u"A");
    CHECK(errorCode([&] {
              static_cast<void>(ordinary->createFilter({}));
          }) == api::ErrorCode::INVALID_ARGUMENT);
    auto profileSchema = standardSchema(true);
    const std::array registrations{api::EventRegistration{api::EventType::PROFILE, 0}};
    api::EventRegistryWork work;
    auto registry = api::EventRegistry::create(profileSchema, registrations, {}, work);
    REQUIRE(registry);
    auto options = endpointOptions();
    options.eventRegistry = *registry;
    auto custom = runtime.createEndpoint(options);
    auto profileFilter = custom->createFilter(categories);
    CHECK_FALSE(profileFilter.accept(api::EventType::QUOTE, plain));
    CHECK(profileFilter.accept(api::EventType::PROFILE, plain));
    CHECK(ordinary->closeAndAwaitTermination().drained);
    CHECK(custom->closeAndAwaitTermination().drained);
    const auto retained = custom->createFilter(categories);
    CHECK_FALSE(retained.accept(api::EventType::QUOTE, plain));
    CHECK(retained.accept(api::EventType::PROFILE, plain));
}

TEST_CASE("public subscription observation keeps detached feed creation a checked CLOSED refusal") {
    std::shared_ptr<api::Feed> retained;
    {
        api::ApiRuntime runtime(runtimeOptions());
        auto endpoint = runtime.createEndpoint(endpointOptions());
        retained = endpoint->getFeed();
        CHECK(endpoint->closeAndAwaitTermination().drained);
    }
    CHECK(errorCode([&] {
              static_cast<void>(retained->createSubscription<events::Quote>());
          }) == api::ErrorCode::CLOSED);
}

TEST_CASE("public subscription observation keeps the data capture retirement fence through self removal") {
    std::atomic<unsigned> foreignCalls{0};
    std::atomic<bool> destroying{false};
    std::atomic<bool> destroyed{false};
    auto bootstrap = root::transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    api::ApiRuntime firstRuntime(*bootstrap, runtimeOptions());
    api::ApiRuntime secondRuntime(*bootstrap, runtimeOptions());
    auto schema = standardSchema(false);
    const std::vector<peer::Record> records{{u"IBM", standardValues(schema)}};
    root::test::TcpPeerConfig config;
    config.sessions.resize(1);
    auto &script = config.sessions[0];
    script.initialBytes = peer::fixturePeerPrefix(schema, core::SubscriptionFamily::TICKER);
    script.replyBytes = peer::fixturePeerData(records, core::SubscriptionFamily::TICKER, true);
    script.readThroughMessage = 11;
    script.waitForClientClose = true;
    root::test::OwnedTcpPeer source(std::move(config));
    REQUIRE(source.port() != 0);
    auto firstEndpoint = firstRuntime.createEndpoint(endpointOptions());
    auto secondEndpoint = secondRuntime.createEndpoint(endpointOptions());
    auto first = firstEndpoint->getFeed()->createSubscription<events::Quote>();
    auto second = secondEndpoint->getFeed()->createSubscription<events::Profile>();
    second->addChangeListener([&](const auto &change) {
        if (change.kind == api::SubscriptionChangeKind::ADDED) {
            CHECK(destroyed.load());
            CHECK_FALSE(destroying.load());
            ++foreignCalls;
        }
    });
    auto capture = std::make_shared<CaptureAction>();
    capture->action = [&] {
        destroying.store(true);
        CHECK(foreignCalls.load() == 0);
        second->addSymbols("FROM-CAPTURE");
        CHECK(foreignCalls.load() == 0);
        destroyed.store(true);
        destroying.store(false);
    };
    api::Subscription<events::Quote>::ListenerId id = 0;
    id = first->addEventListener([&, capture](auto) {
        first->removeEventListener(id);
        second->addSymbols("FROM-DATA");
        CHECK(foreignCalls.load() == 0);
    });
    capture.reset();
    first->addSymbols("IBM");
    firstEndpoint->connect("127.0.0.1:" + std::to_string(source.port()));
    REQUIRE(eventually([&] {
        return foreignCalls.load() == 2;
    }));
    CHECK(destroyed.load());
    CHECK(firstEndpoint->closeAndAwaitTermination().drained);
    CHECK(secondEndpoint->closeAndAwaitTermination().drained);
    CHECK(source.snapshot().error == root::test::TcpPeerError::NONE);
}

TEST_CASE("public subscription observations retain admitted text types and normalize mixed registry arguments") {
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.contract = api::DeliveryContract::STREAM;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto message = feed->createSubscription<events::TextMessage>();
    auto configuration = feed->createSubscription<events::TextConfiguration>();
    auto mixed = feed->createSubscription({api::EventType::TEXT_CONFIGURATION, api::EventType::QUOTE,
                                           api::EventType::TEXT_MESSAGE, api::EventType::TEXT_CONFIGURATION});
    const std::vector messageTypes{api::EventType::TEXT_MESSAGE};
    const std::vector configurationTypes{api::EventType::TEXT_CONFIGURATION};
    const std::vector mixedTypes{api::EventType::QUOTE, api::EventType::TEXT_MESSAGE,
                                 api::EventType::TEXT_CONFIGURATION};
    CHECK(message->getEventTypes() == messageTypes);
    CHECK(configuration->getEventTypes() == configurationTypes);
    CHECK(mixed->getEventTypes() == mixedTypes);
    CHECK(message->containsEventType(api::EventType::TEXT_MESSAGE));
    CHECK_FALSE(message->containsEventType(api::EventType::TEXT_CONFIGURATION));
    CHECK(mixed->containsEventType(api::EventType::TEXT_CONFIGURATION));
    std::atomic<unsigned> additions{0};
    std::atomic<unsigned> removals{0};
    std::atomic<unsigned> closed{0};
    auto listener = [&](std::vector<api::EventType> types) {
        return [&, types = std::move(types)](const api::SubscriptionChange &change) {
            CHECK(change.snapshot.eventTypes == types);

            if (change.kind == api::SubscriptionChangeKind::ADDED) {
                ++additions;
            } else if (change.kind == api::SubscriptionChangeKind::REMOVED) {
                ++removals;
            } else {
                ++closed;
            }
        };
    };
    message->addChangeListener(listener(messageTypes));
    configuration->addChangeListener(listener(configurationTypes));
    mixed->addChangeListener(listener(mixedTypes));
    message->addSymbols("MESSAGE");
    configuration->addSymbols("CONFIGURATION");
    mixed->addSymbols("FIRST");
    const auto retained = mixed->getSnapshot();
    mixed->setSymbols({"SECOND"});
    CHECK(retained.revision == 1);
    REQUIRE(retained.symbols.size() == 1);
    CHECK(retained.symbols.front().eventSymbol() == u"FIRST");
    CHECK(retained.eventTypes == mixedTypes);
    CHECK(mixed->getSnapshot().revision == 2);
    CHECK(mixed->getSnapshot().eventTypes == mixedTypes);
    CHECK(additions.load() == 4);
    CHECK(removals.load() == 1);
    mixed->close();
    CHECK(closed.load() == 0);
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(closed.load() == 3);
    CHECK(retained.eventTypes == mixedTypes);
}

TEST_CASE("serialized events preserve nine-type observation and change-listener dispatch") {
    api::ApiRuntime runtime(runtimeOptions());
    auto options = endpointOptions();
    options.contract = api::DeliveryContract::STREAM;
    auto endpoint = runtime.createEndpoint(options);
    auto feed = endpoint->getFeed();
    auto message = feed->createSubscription<events::Message>();
    auto configuration = feed->createSubscription<events::Configuration>();
    const std::vector messageTypes{api::EventType::MESSAGE};
    const std::vector configurationTypes{api::EventType::CONFIGURATION};
    CHECK(message->getEventTypes() == messageTypes);
    CHECK(configuration->getEventTypes() == configurationTypes);
    const std::vector allTypes{api::EventType::QUOTE,
                               api::EventType::PROFILE,
                               api::EventType::TRADE,
                               api::EventType::TRADE_ETH,
                               api::EventType::SUMMARY,
                               api::EventType::TEXT_MESSAGE,
                               api::EventType::TEXT_CONFIGURATION,
                               api::EventType::MESSAGE,
                               api::EventType::CONFIGURATION};
    auto mixed = feed->createSubscription({api::EventType::CONFIGURATION, api::EventType::TEXT_CONFIGURATION,
                                           api::EventType::MESSAGE, api::EventType::TEXT_MESSAGE,
                                           api::EventType::SUMMARY, api::EventType::TRADE_ETH, api::EventType::TRADE,
                                           api::EventType::PROFILE, api::EventType::QUOTE, api::EventType::MESSAGE});
    CHECK(mixed->getEventTypes() == allTypes);
    std::atomic<unsigned> added{0};
    auto capture = std::make_shared<int>(1);
    std::weak_ptr<int> lifetime = capture;
    mixed->addChangeListener([&, capture](const api::SubscriptionChange &change) {
        CHECK(change.snapshot.eventTypes == allTypes);

        if (change.kind == api::SubscriptionChangeKind::ADDED) {
            ++added;
        }
    });
    capture.reset();
    CHECK_FALSE(lifetime.expired());
    mixed->addSymbols("ALL");
    CHECK(added.load() == 1);
    const auto retained = mixed->getSnapshot();
    CHECK(endpoint->closeAndAwaitTermination().drained);
    CHECK(lifetime.expired());
    CHECK(retained.eventTypes == allTypes);
    REQUIRE(retained.symbols.size() == 1);
    CHECK(retained.symbols.front().eventSymbol() == u"ALL");
}

TEST_CASE("public configuration allocation refusal publishes no endpoint and snapshot failure preserves intent") {
    auto limits = runtimeOptions();
    limits.maximumEndpoints = 1;
    api::ApiRuntime runtime(limits);
    allocation_probe::failNext = true;
    CHECK(errorCode([&] {
              static_cast<void>(runtime.createEndpoint());
          }) == api::ErrorCode::ALLOCATION_FAILURE);
    auto endpoint = runtime.createEndpoint();
    allocation_probe::failNext = true;
    CHECK(errorCode([&] {
              static_cast<void>(endpoint->getEffectiveConfiguration());
          }) == api::ErrorCode::ALLOCATION_FAILURE);
    const auto snapshot = endpoint->getEffectiveConfiguration();
    CHECK(snapshot.connectionRevision == 0);
    CHECK_FALSE(snapshot.activationRequested);
    CHECK_FALSE(snapshot.acceptedAddress);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    static_cast<void>(endpoint->closeAndAwaitTermination());
}

TEST_CASE("detached statistics baselines preserve independent rates and weighted callback means") {
    using namespace org::ttldtor::qdcxx;
    using namespace std::chrono_literals;

    api::SubscriptionStatistics first;
    first.ownerIdentity = std::make_shared<int>(0);
    first.observedAt = std::chrono::steady_clock::time_point{} + 1s;
    auto second = first;
    second.observedAt += 2s;
    second.admittedEvents.value = 10;
    second.callbackDurationTotal = {100, core::StatisticAvailability::AVAILABLE};
    second.callbackDurationSamples.value = 2;
    api::StatisticsBaseline baseline(first);
    auto independent = baseline;
    const auto rate =
        api::statisticsLifetimeRate(baseline.value(), second, &api::SubscriptionStatistics::admittedEvents);
    REQUIRE(rate);
    CHECK(rate->perSecond == 5.0);
    const auto mean = api::statisticsCallbackMean(baseline.value(), second);
    REQUIRE(mean);
    CHECK(*mean == 50.0);
    baseline.reset(second);
    CHECK(independent.value().admittedEvents.value == 0);
    CHECK_FALSE(api::statisticsLifetimeRate(baseline.value(), second, &api::SubscriptionStatistics::admittedEvents));
    CHECK_FALSE(api::statisticsCallbackMean(baseline.value(), second));
    auto incompatible = second;
    incompatible.ownerIdentity = std::make_shared<int>(0);
    CHECK_FALSE(api::statisticsLifetimeRate(first, incompatible, &api::SubscriptionStatistics::admittedEvents));
    incompatible = second;
    incompatible.generation++;
    CHECK_FALSE(api::statisticsLifetimeRate(first, incompatible, &api::SubscriptionStatistics::admittedEvents));
    incompatible = second;
    incompatible.admittedEvents.availability = core::StatisticAvailability::SATURATED;
    CHECK_FALSE(api::statisticsLifetimeRate(first, incompatible, &api::SubscriptionStatistics::admittedEvents));
    CHECK_FALSE(api::statisticsLifetimeRate(first, second, &api::SubscriptionStatistics::callbackDurationMaximum));
    incompatible = second;
    incompatible.admittedEvents.value = std::numeric_limits<std::uint64_t>::max();
    const auto large = api::statisticsLifetimeRate(first, incompatible, &api::SubscriptionStatistics::admittedEvents);
    REQUIRE(large);
    CHECK(std::isfinite(large->perSecond));
    incompatible = second;
    incompatible.admittedEvents.value = 0;
    auto decreasing = first;
    decreasing.admittedEvents.value = 1;
    CHECK_FALSE(api::statisticsLifetimeRate(decreasing, incompatible, &api::SubscriptionStatistics::admittedEvents));
    incompatible = second;
    incompatible.callbackDurationSamples.value = 0;
    CHECK_FALSE(api::statisticsCallbackMean(first, incompatible));
    auto later = second;
    later.observedAt += 1s;
    later.callbackDurationTotal.value = 110;
    later.callbackDurationSamples.value = 3;
    const auto laterMean = api::statisticsCallbackMean(second, later);
    REQUIRE(laterMean);
    CHECK(*laterMean == 10.0);
}

TEST_CASE("endpoint lifetime rate preserves per-field write uncertainty") {
    using namespace org::ttldtor::qdcxx;
    using namespace std::chrono_literals;

    api::EndpointStatistics first;
    first.ownerIdentity = std::make_shared<int>(0);
    first.observedAt = std::chrono::steady_clock::time_point{} + 1s;
    auto second = first;
    second.observedAt += 1s;
    second.connection.writtenBytes.value = 7;
    second.connection.readBytes.value = 9;
    second.connection.writeAcceptanceUnknown = true;
    CHECK_FALSE(api::statisticsLifetimeRate(first, second, &transport::ConnectionStatistics::writtenBytes));
    const auto read = api::statisticsLifetimeRate(first, second, &transport::ConnectionStatistics::readBytes);
    REQUIRE(read);
    CHECK(read->perSecond == 9.0);
    CHECK_FALSE(api::statisticsLifetimeRate(first, second, &transport::ConnectionStatistics::roundTripTime));
}

TEST_CASE("epoch statistics subtract prior lifetime totals only for qualified source identity") {
    using namespace org::ttldtor::qdcxx;
    using namespace std::chrono_literals;

    api::EndpointStatistics first;
    first.ownerIdentity = std::make_shared<int>(0);
    first.observedAt = std::chrono::steady_clock::time_point{} + 1s;
    first.connection.sourceEpoch = 7;
    first.connection.establishedConnections.value = 7;
    first.connection.readBytes.value = 100;
    auto second = first;
    second.observedAt += 1s;
    second.connection.readBytes.value = 125;
    const auto rate = api::statisticsEpochRate(first, second, &transport::ConnectionStatistics::readBytes);
    REQUIRE(rate);
    CHECK(rate->sourceEpoch == 7);
    CHECK(rate->perSecond == 25.0);
    auto incompatible = second;
    incompatible.connection.sourceEpoch = 8;
    CHECK_FALSE(api::statisticsEpochRate(first, incompatible, &transport::ConnectionStatistics::readBytes));
    incompatible = second;
    incompatible.connection.establishedConnections.availability = core::StatisticAvailability::SATURATED;
    CHECK_FALSE(api::statisticsEpochRate(first, incompatible, &transport::ConnectionStatistics::readBytes));
    first.connection.sourceEpoch.reset();
    incompatible = second;
    incompatible.connection.sourceEpoch.reset();
    CHECK_FALSE(api::statisticsEpochRate(first, incompatible, &transport::ConnectionStatistics::readBytes));
    api::SubscriptionStatistics subscriptionFirst;
    subscriptionFirst.ownerIdentity = first.ownerIdentity;
    subscriptionFirst.observedAt = first.observedAt;
    subscriptionFirst.sourceEpoch = 7;
    subscriptionFirst.admittedEvents.value = 100;
    auto subscriptionLast = subscriptionFirst;
    subscriptionLast.observedAt += 1s;
    subscriptionLast.admittedEvents.value = 125;
    const auto subscriptionRate =
        api::statisticsEpochRate(subscriptionFirst, subscriptionLast, &api::SubscriptionStatistics::admittedEvents);
    REQUIRE(subscriptionRate);
    CHECK(subscriptionRate->perSecond == 25.0);
    subscriptionLast.sourceEpoch = 8;
    CHECK_FALSE(
        api::statisticsEpochRate(subscriptionFirst, subscriptionLast, &api::SubscriptionStatistics::admittedEvents));
    subscriptionFirst.sourceEpoch.reset();
    subscriptionLast.sourceEpoch.reset();
    CHECK_FALSE(
        api::statisticsEpochRate(subscriptionFirst, subscriptionLast, &api::SubscriptionStatistics::admittedEvents));
    subscriptionFirst.sourceEpoch = std::numeric_limits<std::uint64_t>::max();
    subscriptionLast.sourceEpoch = subscriptionFirst.sourceEpoch;
    CHECK_FALSE(
        api::statisticsEpochRate(subscriptionFirst, subscriptionLast, &api::SubscriptionStatistics::admittedEvents));
}

TEST_CASE("statistics rates retain positive elapsed ticks across clock origins and reject duration counters") {
    using namespace org::ttldtor::qdcxx;
    using Clock = std::chrono::steady_clock;
    using Rep = Clock::duration::rep;
    api::SubscriptionStatistics first;
    first.ownerIdentity = std::make_shared<int>(0);
    auto second = first;
    second.admittedEvents.value = 1;
    const auto checkOneTick = [&](Rep origin) {
        first.observedAt = Clock::time_point{Clock::duration{origin}};
        second.observedAt = Clock::time_point{Clock::duration{static_cast<Rep>(origin + 1)}};
        const auto rate = api::statisticsLifetimeRate(first, second, &api::SubscriptionStatistics::admittedEvents);
        REQUIRE(rate);
        CHECK(rate->perSecond ==
              doctest::Approx(static_cast<double>(Clock::period::den) / Clock::period::num).epsilon(1e-12));
    };
    checkOneTick(0);
    checkOneTick(-1);
    checkOneTick(std::numeric_limits<Rep>::min());
    checkOneTick(std::numeric_limits<Rep>::max() - 1);
    first.observedAt = Clock::time_point{Clock::duration{std::numeric_limits<Rep>::min()}};
    second.observedAt = Clock::time_point{Clock::duration{std::numeric_limits<Rep>::max()}};
    CHECK(api::statisticsLifetimeRate(first, second, &api::SubscriptionStatistics::admittedEvents));
    first.callbackDurationTotal = {0, core::StatisticAvailability::AVAILABLE};
    second.callbackDurationTotal = {100, core::StatisticAvailability::AVAILABLE};
    CHECK_FALSE(api::statisticsLifetimeRate(first, second, &api::SubscriptionStatistics::callbackDurationTotal));
    first.sourceEpoch = 1;
    second.sourceEpoch = 1;
    CHECK_FALSE(api::statisticsEpochRate(first, second, &api::SubscriptionStatistics::callbackDurationTotal));
}
