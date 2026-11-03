// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: controlled bootstrap borrowing, finite loopback peers and subprocess death modes.
// feed_bootstrap_death.cmake supplies --bootstrap-owner-death/--bootstrap-thread-death; no external service.
#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/api/feed.hpp"
#include "qd/transport/recovery.hpp"

#include "bootstrap_lease.hpp"
#include "connect_peer.hpp"

#include <doctest/doctest.h>

#ifdef _WIN32
#    include <winsock2.h>
#else
#    include <fcntl.h>
#    include <netinet/in.h>
#    include <sys/socket.h>
#    include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace bootstrap_allocations {
/** Run the separately compiled constructor-only allocation fault sweep.
 * @return Zero after all one-shot refusal and host-lease checks pass.
 */
int run();
} // namespace bootstrap_allocations

namespace {
namespace root = org::ttldtor::qdcxx;
namespace api = root::api;
namespace transport = root::transport;
namespace peer = root::connect_tests;
using namespace std::chrono_literals;

/** Select explicit resolver input so constructor tests never discover ambient DNS.
 * @return Existing finite runtime defaults with one numeric loopback DNS server.
 */
api::RuntimeOptions options() {
    api::RuntimeOptions result;
    result.dnsServers.push_back({{127, 0, 0, 1}});

    return result;
}

/** Capture a public failure without arbitrary exception-text matching.
 * @tparam Action Synchronous public operation.
 * @param action Operation expected to reject before native activation.
 * @return Detached diagnostic, or NONE after success.
 */
template <class Action> api::Diagnostic diagnostic(Action action) {
    try {
        action();
    } catch (const api::Error &error) {
        return error.diagnostic();
    }

    return {};
}

/** Bound a test observation without progressing the runtime.
 * @tparam Predicate Atomic observation callable.
 * @param predicate Test-owned observation.
 * @return True before the explicit finite ceiling.
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

/** Construct the unchanged complete Quote schema for the independent wire peer.
 * @return Complete validated standard schema.
 */
root::core::LocalSchema schema() {
    constexpr root::core::SchemaLimits LIMITS{16, 128, 256, 128, 16384};
    auto builtin = root::events::defaultQuoteSchema(LIMITS);
    REQUIRE(builtin);
    auto result =
        root::core::LocalSchema::create(builtin->records(), LIMITS, root::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Test-owned silent numeric UDP resolver; receiving an actual query proves DNS is pending at close. */
class SilentDns {
#ifdef _WIN32
    SOCKET socket_ = INVALID_SOCKET;
#else
    int socket_ = -1;
#endif
    std::uint16_t port_ = 0; ///< Zero after setup refusal.

    public:
    /** Bind one nonblocking loopback UDP socket; no thread, SDK or external resolver is used. */
    SilentDns() {
        socket_ = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32

        if (socket_ == INVALID_SOCKET) {
            return;
        }

        u_long nonblocking = 1;

        if (::ioctlsocket(socket_, FIONBIO, &nonblocking) != 0) {
            return;
        }

#else

        if (socket_ < 0) {
            return;
        }

        if (::fcntl(socket_, F_SETFL, O_NONBLOCK) != 0) {
            return;
        }

#endif
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (::bind(socket_, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) != 0) {
            return;
        }

#ifdef _WIN32
        int length = sizeof(address);
#else
        socklen_t length = sizeof(address);
#endif

        if (::getsockname(socket_, reinterpret_cast<sockaddr *>(&address), &length) == 0) {
            port_ = ntohs(address.sin_port);
        }
    }

    /** Retire only the test-owned UDP descriptor. */
    ~SilentDns() {
#ifdef _WIN32

        if (socket_ != INVALID_SOCKET) {
            ::closesocket(socket_);
        }

#else

        if (socket_ >= 0) {
            ::close(socket_);
        }

#endif
    }

    /** Inspect successful ephemeral binding.
     * @return Numeric loopback UDP port, or zero after setup refusal.
     */
    std::uint16_t port() const noexcept {
        return port_;
    }

    /** Consume at most one whole test query without answering it.
     * @return True after the worker has sent an actual DNS query to this owned socket.
     */
    bool received() const noexcept {
        char bytes[512];

        return ::recv(socket_, bytes, sizeof(bytes), 0) > 0;
    }
};

/** Restore the exact private test borrow count even if a test-owned thread cannot be created. */
struct ScopedBorrowCount {
    std::atomic_size_t &counter; ///< Test-owned counter; no native borrower is active in this fixture.
    std::size_t original;        ///< Actual count restored before any owner/probe can retire.

    /** Override one deterministic capacity boundary without a racing stress loop.
     * @param counter Test-owned bootstrap counter.
     * @param replacement Explicit artificial boundary retained through the joined test worker.
     */
    ScopedBorrowCount(std::atomic_size_t &counter, std::size_t replacement)
        : counter(counter), original(counter.load()) {
        counter.store(replacement);
    }

    /** Restore the real count before leaving the fixture on success or exception. */
    ~ScopedBorrowCount() {
        counter.store(original);
    }
};

/** Observe final selected capture destruction independently of endpoint terminal notification. */
struct Capture {
    std::atomic<bool> &retired; ///< Test observation outlives runtime and all selected callbacks.

    /** Keep the final-release observation alive through this captured owner.
     * @param retired Test-owned atomic, retained until runtime and listener retirement finish.
     */
    explicit Capture(std::atomic<bool> &retired) : retired(retired) {
    }

    ~Capture() {
        retired.store(true, std::memory_order_release);
    }
};

/** Keep one bounded, joined host thread alive without SDK calls or a polling loop. */
class EmbeddingHostThread {
    std::mutex mutex_;
    std::condition_variable_any changed_;
    bool ready_ = false;
    std::atomic<bool> alive_{false};
    std::jthread thread_;

    public:
    /** Start only after every host bootstrap was created on main; throw if readiness exceeds five seconds. */
    EmbeddingHostThread()
        : thread_([this](std::stop_token stop) {
              std::unique_lock lock(mutex_);
              alive_.store(true, std::memory_order_release);
              ready_ = true;
              changed_.notify_all();
              changed_.wait(lock, stop, [] {
                  return false;
              });
              alive_.store(false, std::memory_order_release);
          }) {
        std::unique_lock lock(mutex_);

        if (!changed_.wait_for(lock, 5s, [this] {
                return ready_;
            })) {
            throw std::runtime_error("host-thread-ready");
        }
    }

    /** Inspect the known owned thread, without claiming to enumerate foreign threads.
     * @return True while the host thread is waiting for its own stop token.
     */
    bool alive() const noexcept {
        return alive_.load(std::memory_order_acquire);
    }
};

/** Test-owned callback observation; declared before the runtime that retains its references. */
class EmbeddingQuoteSignal {
    std::mutex mutex_;
    std::condition_variable changed_;
    unsigned count_ = 0;
    bool valid_ = true;

    public:
    /** Record actual typed native delivery without retaining the borrowed span.
     * @param batch The current listener invocation's Quote values.
     */
    void record(std::span<const root::events::Quote> batch) {
        {
            std::lock_guard lock(mutex_);
            valid_ = valid_ && batch.size() == 1 && batch.front().symbol == u"AAPL";
            count_ += static_cast<unsigned>(batch.size());
        }
        changed_.notify_all();
    }

    /** Await an actual callback, without progressing the SDK.
     * @return True after one valid Quote arrived within five seconds.
     */
    bool wait() {
        std::unique_lock lock(mutex_);

        return changed_.wait_for(lock, 5s,
                                 [this] {
                                     return count_ != 0;
                                 }) &&
               valid_ && count_ == 1;
    }

    /** Copy the callback count under the same application mutex.
     * @return Number of observed owning Quote values.
     */
    unsigned count() {
        std::lock_guard lock(mutex_);

        return count_;
    }
};

/** Main-owned result outlives callback capture destruction and runtime quiescence. */
struct EmbeddingCaptureObservation {
    std::atomic<bool> reentered{false};
    std::atomic<bool> retired{false};
    std::atomic<bool> failed{false};
};

/** Strong application handle cycle that shutdown must retire outside both port/control locks. */
struct EmbeddingReentrantCapture {
    std::shared_ptr<api::Endpoint> endpoint;
    std::shared_ptr<api::Subscription<root::events::Quote>> subscription;
    EmbeddingCaptureObservation &observation;

    /** Retain application handles until the last registered/selected function releases this capture.
     * @param endpoint Endpoint used for nonblocking callback reentry.
     * @param subscription Subscription whose own capture will query and close it during final release.
     * @param observation Main-owned results, declared before the runtime.
     */
    EmbeddingReentrantCapture(std::shared_ptr<api::Endpoint> endpoint,
                              std::shared_ptr<api::Subscription<root::events::Quote>> subscription,
                              EmbeddingCaptureObservation &observation)
        : endpoint(std::move(endpoint)), subscription(std::move(subscription)), observation(observation) {
    }

    /** Reacquire both public query locks and reenter close; a lock-held destruction would hit the test timeout. */
    ~EmbeddingReentrantCapture() {
        try {
            static_cast<void>(endpoint->getDiagnostic());
            static_cast<void>(subscription->getDiagnostic());
            endpoint->close();
            subscription->close();
            observation.reentered.store(true, std::memory_order_release);
        } catch (...) {
            observation.failed.store(true, std::memory_order_release);
        }
        observation.retired.store(true, std::memory_order_release);
    }
};
} // namespace

TEST_CASE("borrowed feed supports host threads and multiple idle runtimes across owner facade moves") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<transport::RuntimeBootstrap> transferredOwner;
    auto probe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(probe);
    std::jthread host([](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(1ms);
        }
    });
    {
        api::ApiRuntime first(*bootstrap, options());
        transferredOwner.emplace(std::move(*bootstrap));
        {
            api::ApiRuntime second(*transferredOwner, options());
            CHECK(probe->state()->borrowers.load() == 3);
            first.closeAndAwaitTermination();
            second.closeAndAwaitTermination();
            CHECK(probe->state()->borrowers.load() == 3);
        }
        CHECK(probe->state()->borrowers.load() == 2);
    }
    CHECK(probe->state()->borrowers.load() == 1);
}

TEST_CASE("borrowed feed rejects moved owner and wrong thread before resolver snapshot") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto movedOwner = std::move(*bootstrap);
    const auto moved = diagnostic([&] {
        api::ApiRuntime runtime(*bootstrap);
    });
    CHECK(moved.code == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(moved.stage == "bootstrap-lease-configuration");
    auto probe = transport::detail::BootstrapLease::acquire(movedOwner);
    REQUIRE(probe);
    api::Diagnostic foreign;
    std::thread worker([&] {
        foreign = diagnostic([&] {
            api::ApiRuntime runtime(movedOwner);
        });
    });
    worker.join();
    CHECK(foreign.code == api::ErrorCode::WRONG_THREAD);
    CHECK(foreign.stage == "bootstrap-borrow-thread");
    CHECK(probe->state()->borrowers.load() == 1);
}

TEST_CASE("borrowed feed checks same-instance provenance and exhausted lease capacity") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto probe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(probe);
    auto *state = probe->state();
    const auto *original = state->provenance;
    const unsigned char otherInstance = 0;
    state->provenance = &otherInstance;
    const auto incompatible = diagnostic([&] {
        api::ApiRuntime runtime(*bootstrap, options());
    });
    state->provenance = original;
    CHECK(incompatible.code == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(incompatible.stage == "bootstrap-lease-configuration");
    CHECK(state->borrowers.load() == 1);
    api::Diagnostic exhausted;
    api::Diagnostic foreign;
    {
        ScopedBorrowCount saturation(state->borrowers, std::numeric_limits<std::size_t>::max());
        exhausted = diagnostic([&] {
            api::ApiRuntime runtime(*bootstrap, options());
        });
        std::thread worker([&] {
            foreign = diagnostic([&] {
                api::ApiRuntime runtime(*bootstrap, options());
            });
        });
        worker.join();
    }
    CHECK(exhausted.code == api::ErrorCode::RESOURCE_LIMIT);
    CHECK(exhausted.stage == "bootstrap-lease-borrowers");
    CHECK(foreign.code == api::ErrorCode::WRONG_THREAD);
    CHECK(foreign.stage == "bootstrap-borrow-thread");
    CHECK(state->borrowers.load() == 1);
}

TEST_CASE("failed borrowed feed configuration rolls back only its lease") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto probe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(probe);
    auto invalid = options();
    invalid.maximumEndpoints = 0;
    CHECK(diagnostic([&] {
              api::ApiRuntime runtime(*bootstrap, invalid);
          }).code == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(probe->state()->borrowers.load() == 1);
    invalid = options();
    invalid.maximumPendingStateChangesPerEndpoint = 0;
    const auto owned = diagnostic([&] {
        api::ApiRuntime runtime(invalid);
    });
    CHECK(owned.code == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(owned.stage == "runtime-options");
    CHECK(probe->state()->borrowers.load() == 1);
    const auto borrowed = diagnostic([&] {
        api::ApiRuntime runtime(*bootstrap, invalid);
    });
    CHECK(borrowed.code == api::ErrorCode::INVALID_ARGUMENT);
    CHECK(borrowed.stage == "runtime-options");
    CHECK(probe->state()->borrowers.load() == 1);
    api::ApiRuntime subsequent(*bootstrap, options());
    subsequent.closeAndAwaitTermination();
}

TEST_CASE("borrowed feed activates after owner move and retires callback capture before releasing lease") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<transport::RuntimeBootstrap> transferredOwner;
    auto probe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(probe);
    std::atomic<bool> called{false};
    std::atomic<bool> retired{false};
    bool throwCallback = false;
    SUBCASE("callback close") {
    }
    SUBCASE("callback throw") {
        throwCallback = true;
    }
    {
        api::ApiRuntime runtime(*bootstrap, options());
        transferredOwner.emplace(std::move(*bootstrap));
        auto declaration = schema();
        std::vector<root::core::FieldValue> values;

        for (const auto &field : declaration.records()[0].fields) {
            values.push_back({static_cast<root::core::SerialFieldType>(field.serialTypeId), 0});
        }

        const std::vector<peer::Record> records{{u"AAPL", std::move(values)}};
        const std::vector<std::u16string> symbols{u"AAPL"};
        peer::PeerScript script;
        script.family = root::core::SubscriptionFamily::TICKER;
        script.canonicalSymbols = true;
        peer::Peer source(declaration, records, symbols, 8192, peer::PeerMode::HOLD, script);
        auto endpoint = runtime.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<root::events::Quote>();
        auto capture = std::make_shared<Capture>(retired);
        subscription->addEventListener(
            [capture, endpoint, &called, throwCallback](std::span<const root::events::Quote> batch) {
                called.store(!batch.empty(), std::memory_order_release);

                if (throwCallback) {
                    throw std::runtime_error("controlled callback refusal");
                }

                endpoint->close();
            });
        capture.reset();
        subscription->addSymbols({"AAPL"});
        endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
        CHECK(eventually([&] {
            return called.load(std::memory_order_acquire);
        }));
        runtime.closeAndAwaitTermination();
        CHECK(retired.load(std::memory_order_acquire));
        CHECK(probe->state()->borrowers.load() == 2);
        CHECK(source.adds() == 1);
        CHECK_FALSE(source.failed());
    }
    CHECK(probe->state()->borrowers.load() == 1);
}

TEST_CASE("borrowed feed retires an actually pending DNS query before releasing its lease") {
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    auto probe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(probe);
    SilentDns resolver;
    REQUIRE(resolver.port() != 0);
    {
        auto settings = options();
        settings.dnsPort = resolver.port();
        api::ApiRuntime runtime(*bootstrap, settings);
        api::EndpointOptions policy;
        policy.maximumAttempts = 1;
        policy.dnsTimeout = 30s;
        auto endpoint = runtime.createEndpoint(policy);
        endpoint->connect("bootstrap.invalid:12345");
        CHECK(eventually([&] {
            return resolver.received();
        }));
        runtime.closeAndAwaitTermination();
        CHECK(probe->state()->borrowers.load() == 2);
        const auto terminalDiagnostic = endpoint->getDiagnostic();
        CAPTURE(terminalDiagnostic.stage);
        CAPTURE(terminalDiagnostic.nativeCode);
        CAPTURE(terminalDiagnostic.detailCode);
        CAPTURE(static_cast<int>(terminalDiagnostic.code));
        // Existing plain cutoff maps a no-error native terminal to recovery-peer-end; preserve that evidence.
        CHECK(endpoint->getState() == api::EndpointState::FAILED);
        CHECK(terminalDiagnostic.code == api::ErrorCode::TRANSPORT_FAILURE);
        CHECK(terminalDiagnostic.stage == "recovery-peer-end");
        CHECK(terminalDiagnostic.nativeCode == 0);
        CHECK(terminalDiagnostic.detailCode == static_cast<std::uint32_t>(transport::RecoveryError::TRANSPORT_FAILURE));
    }
    CHECK(probe->state()->borrowers.load() == 1);
}

TEST_CASE("runtime embedding late borrow preserves a live native owner across quiescence") {
    bool separateOwner = false;
    SUBCASE("one pre-thread host owner") {
    }
    SUBCASE("two pre-thread same-image host owners") {
        separateOwner = true;
    }
    auto bootstrap = transport::RuntimeBootstrap::create();
    REQUIRE(bootstrap);
    std::optional<transport::RuntimeBootstrap> other;

    if (separateOwner) {
        auto created = transport::RuntimeBootstrap::create();
        REQUIRE(created);
        other.emplace(std::move(*created));
    }

    auto firstProbe = transport::detail::BootstrapLease::acquire(*bootstrap);
    REQUIRE(firstProbe);
    std::optional<transport::detail::BootstrapLease> otherProbe;

    if (other) {
        auto acquired = transport::detail::BootstrapLease::acquire(*other);
        REQUIRE(acquired);
        otherProbe.emplace(std::move(*acquired));
    }

    EmbeddingQuoteSignal firstSignal;
    EmbeddingQuoteSignal lateSignal;
    EmbeddingHostThread host;
    auto declaration = schema();
    std::vector<root::core::FieldValue> values;

    for (const auto &field : declaration.records()[0].fields) {
        values.push_back({static_cast<root::core::SerialFieldType>(field.serialTypeId), 0});
    }

    const std::vector<peer::Record> records{{u"AAPL", std::move(values)}};
    const std::vector<std::u16string> symbols{u"AAPL"};
    peer::PeerScript gated;
    gated.family = root::core::SubscriptionFamily::TICKER;
    gated.canonicalSymbols = true;
    gated.waitForDataRelease = true;
    auto ready = gated;
    ready.waitForDataRelease = false;
    peer::Peer firstSource(declaration, records, symbols, 8192, peer::PeerMode::HOLD, gated);
    peer::Peer lateSource(declaration, records, symbols, 8192, peer::PeerMode::HOLD, ready);
    {
        api::ApiRuntime first(*bootstrap, options());
        auto endpoint = first.createEndpoint();
        auto subscription = endpoint->getFeed()->createSubscription<root::events::Quote>();
        subscription->addEventListener([&firstSignal](auto batch) {
            firstSignal.record(batch);
        });
        subscription->addSymbols("AAPL");
        endpoint->connect("127.0.0.1:" + std::to_string(firstSource.port()));
        REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 5s).status ==
                api::WaitStatus::REACHED);
        REQUIRE(eventually([&] {
            return firstSource.adds() == 1;
        }));
        CHECK(firstSignal.count() == 0);
        const auto firstBaseline = firstProbe->state()->borrowers.load();
        {
            api::ApiRuntime late(other ? *other : *bootstrap, options());
            auto lateEndpoint = late.createEndpoint();
            auto lateSubscription = lateEndpoint->getFeed()->createSubscription<root::events::Quote>();
            lateSubscription->addEventListener([&lateSignal](auto batch) {
                lateSignal.record(batch);
            });
            lateSubscription->addSymbols("AAPL");
            lateEndpoint->connect("127.0.0.1:" + std::to_string(lateSource.port()));
            REQUIRE(lateSignal.wait());
            CHECK(host.alive());
        }
        CHECK(firstProbe->state()->borrowers.load() == firstBaseline);

        if (otherProbe) {
            CHECK(otherProbe->state()->borrowers.load() == 1);
        }

        CHECK(host.alive());
        CHECK(endpoint->getState() == api::EndpointState::CONNECTED);
        CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
        firstSource.releaseData();
        REQUIRE(firstSignal.wait());
        first.closeAndAwaitTermination();
        CHECK_FALSE(firstSource.failed());
        CHECK_FALSE(lateSource.failed());
    }
    CHECK(firstProbe->state()->borrowers.load() == 1);

    if (otherProbe) {
        CHECK(otherProbe->state()->borrowers.load() == 1);
    }
}

TEST_CASE("runtime embedding native detached handles survive owner retirement and foreign final release") {
    bool borrowed = false;
    SUBCASE("owned runtime") {
    }
    SUBCASE("borrowed runtime") {
        borrowed = true;
    }
    std::optional<transport::RuntimeBootstrap> host;
    std::optional<transport::detail::BootstrapLease> probe;

    if (borrowed) {
        auto created = transport::RuntimeBootstrap::create();
        REQUIRE(created);
        host.emplace(std::move(*created));
        auto acquired = transport::detail::BootstrapLease::acquire(*host);
        REQUIRE(acquired);
        probe.emplace(std::move(*acquired));
    }

    EmbeddingQuoteSignal signal;
    EmbeddingQuoteSignal pausedReplay;
    EmbeddingCaptureObservation observation;
    std::weak_ptr<EmbeddingReentrantCapture> captureWitness;
    std::shared_ptr<api::Endpoint> endpoint;
    std::shared_ptr<api::Subscription<root::events::Quote>> subscription;
    {
        auto runtime = borrowed ? std::make_unique<api::ApiRuntime>(*host, options())
                                : std::make_unique<api::ApiRuntime>(options());
        auto declaration = schema();
        std::vector<root::core::FieldValue> values;

        for (const auto &field : declaration.records()[0].fields) {
            values.push_back({static_cast<root::core::SerialFieldType>(field.serialTypeId), 0});
        }

        const std::vector<peer::Record> records{{u"AAPL", std::move(values)}};
        const std::vector<std::u16string> symbols{u"AAPL"};
        {
            peer::PeerScript script;
            script.family = root::core::SubscriptionFamily::TICKER;
            script.canonicalSymbols = true;
            peer::Peer source(declaration, records, symbols, 8192, peer::PeerMode::HOLD, script);
            endpoint = runtime->createEndpoint();
            subscription = endpoint->getFeed()->createSubscription<root::events::Quote>();
            auto capture = std::make_shared<EmbeddingReentrantCapture>(endpoint, subscription, observation);
            captureWitness = capture;
            subscription->addEventListener([capture, &signal](auto batch) {
                capture->endpoint->disconnect();
                signal.record(batch);
            });
            capture.reset();
            subscription->addSymbols("AAPL");
            endpoint->connect("127.0.0.1:" + std::to_string(source.port()));
            REQUIRE(signal.wait());
            auto cached = endpoint->getFeed()->createSubscription<root::events::Quote>();
            cached->addEventListener([&pausedReplay](auto batch) {
                pausedReplay.record(batch);
            });
            cached->addSymbols("AAPL");
            // A genuine retained INITIAL requires physical SUSPENDED; logical NOT_CONNECTED alone is no barrier.
            REQUIRE(pausedReplay.wait());
            CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
            CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
            runtime->closeAndAwaitTermination();
            CHECK(observation.retired.load(std::memory_order_acquire));
            CHECK(observation.reentered.load(std::memory_order_acquire));
            CHECK_FALSE(observation.failed.load(std::memory_order_acquire));
            CHECK(captureWitness.expired());
            CHECK(source.adds() == 1);
            CHECK_FALSE(source.failed());

            if (probe) {
                CHECK(probe->state()->borrowers.load() == 2);
            }
        }
        // The owned bootstrap cannot be destroyed until the application peer thread above has joined.
    }

    if (probe) {
        CHECK(probe->state()->borrowers.load() == 1);
    }

    const auto detachedDiagnostic = endpoint->getDiagnostic();
    CAPTURE(detachedDiagnostic.stage);
    CAPTURE(detachedDiagnostic.nativeCode);
    CAPTURE(detachedDiagnostic.detailCode);
    CHECK(endpoint->getState() == api::EndpointState::CLOSED);
    CHECK(subscription->getState() == api::SubscriptionState::CLOSED);
    CHECK(endpoint->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(subscription->getDiagnostic().code == api::ErrorCode::NONE);
    CHECK(endpoint->awaitState(api::EndpointState::CLOSED).status == api::WaitStatus::REACHED);
    CHECK(subscription->awaitState(api::SubscriptionState::CLOSED).status == api::WaitStatus::REACHED);
    CHECK(diagnostic([&] {
              subscription->addSymbols("IBM");
          }).code == api::ErrorCode::CLOSED);
    const auto callbacks = signal.count();
    std::weak_ptr<api::Endpoint> endpointWitness = endpoint;
    std::weak_ptr<api::Subscription<root::events::Quote>> subscriptionWitness = subscription;
    std::atomic<bool> foreignClosed{false};
    std::atomic<api::ErrorCode> endpointWait{api::ErrorCode::NONE};
    std::atomic<api::ErrorCode> subscriptionWait{api::ErrorCode::NONE};
    std::thread foreign([endpoint = std::move(endpoint), subscription = std::move(subscription), &foreignClosed,
                         &endpointWait, &subscriptionWait]() mutable {
        foreignClosed = endpoint->getState() == api::EndpointState::CLOSED &&
                        subscription->getState() == api::SubscriptionState::CLOSED;
        endpointWait = diagnostic([&] {
                           static_cast<void>(endpoint->awaitState(api::EndpointState::CLOSED));
                       }).code;
        subscriptionWait = diagnostic([&] {
                               static_cast<void>(subscription->awaitState(api::SubscriptionState::CLOSED));
                           }).code;
        endpoint->disconnect();
        endpoint->disconnectAndClear();
        endpoint->reconnect();
        endpoint->close();
        subscription->close();
        subscription.reset();
        endpoint.reset();
    });
    foreign.join();
    CHECK(foreignClosed.load());
    CHECK(endpointWait.load() == api::ErrorCode::WRONG_THREAD);
    CHECK(subscriptionWait.load() == api::ErrorCode::WRONG_THREAD);
    CHECK(signal.count() == callbacks);
    CHECK(endpointWitness.expired());
    CHECK(subscriptionWitness.expired());

    if (probe) {
        CHECK(probe->state()->borrowers.load() == 1);
    }
}

/** Run bounded internal death modes or the ordinary doctest suite.
 * @param argc Native argument count.
 * @param argv CTest-owned arguments; death modes terminate with the exact sentinel73.
 * @return Ordinary test status; death scenarios return74 if the required fail-fast was missed.
 */
int main(int argc, char **argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--bootstrap-allocation-failures") {
        std::set_terminate([] {
            std::_Exit(76);
        });

        return bootstrap_allocations::run();
    }

    if (argc == 2 && (std::string_view(argv[1]) == "--bootstrap-owner-death" ||
                      std::string_view(argv[1]) == "--bootstrap-thread-death")) {
        std::set_terminate([] {
            std::_Exit(73);
        });
        auto bootstrap = transport::RuntimeBootstrap::create();

        if (!bootstrap) {
            return 75;
        }

        if (std::string_view(argv[1]) == "--bootstrap-owner-death") {
            api::ApiRuntime runtime(*bootstrap, options());
            auto dyingOwner = std::move(*bootstrap);
        } else {
            std::thread worker([&] {
                std::set_terminate([] {
                    std::_Exit(73);
                });
                auto dyingOwner = std::move(*bootstrap);
            });
            worker.join();
        }

        return 74;
    }

    doctest::Context context(argc, argv);

    return context.run();
}
