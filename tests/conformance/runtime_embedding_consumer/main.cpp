// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal installed/relocated CTest fixture; only public headers, no input, network or application progress loop.
#include <qd/api/feed.hpp>
#include <qd/transport/tcp_runtime.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
using namespace std::chrono_literals;
using Subscription = q::api::Subscription<q::events::Quote>;

/** Observe capture retirement without retaining any SDK capability. */
struct Observation {
    std::atomic<unsigned> calls{0};
    std::atomic<bool> retired{false};
    std::atomic<bool> reentered{false};
};

/** Application capture cycle; shutdown must clear it before returning from quiescence. */
struct Capture {
    std::shared_ptr<q::api::Endpoint> endpoint;
    std::shared_ptr<Subscription> subscription;
    Observation &observation;

    /** Retain public handles without adopting the runtime or bootstrap.
     * @param endpoint Closed before final foreign-thread destruction.
     * @param subscription Independent public handle, outliving its runtime.
     * @param observation Main-owned storage declared before the runtime.
     */
    Capture(std::shared_ptr<q::api::Endpoint> endpoint, std::shared_ptr<Subscription> subscription,
            Observation &observation)
        : endpoint(std::move(endpoint)), subscription(std::move(subscription)), observation(observation) {
    }

    /** Reenter public query/close outside control locks, without throwing during unwind. */
    ~Capture() {
        static_cast<void>(endpoint->getDiagnostic());
        static_cast<void>(subscription->getDiagnostic());
        endpoint->close();
        subscription->close();
        observation.reentered.store(true, std::memory_order_release);
        observation.retired.store(true, std::memory_order_release);
    }
};

/** Preserve typed public refusal without matching arbitrary exception text.
 * @tparam Action Synchronous public operation.
 * @param action Operation expected to refuse in the current lifetime/thread context.
 * @return Public code, or NONE when the operation succeeded.
 */
template <class Action> q::api::ErrorCode refusal(Action action) {
    try {
        action();
    } catch (const q::api::Error &error) {
        return error.diagnostic().code;
    }

    return q::api::ErrorCode::NONE;
}

/** Exercise one supported ownership mode with terminal handles surviving the runtime.
 * @param borrowed Create a pre-thread host bootstrap and a controlled host thread before the late borrow.
 * @return Zero after all lifetime/thread/refusal checks, otherwise a stable fixture stage number.
 */
int exercise(bool borrowed) {
    std::optional<q::transport::RuntimeBootstrap> host;

    if (borrowed) {
        auto created = q::transport::RuntimeBootstrap::create();

        if (!created) {
            return 1;
        }

        host.emplace(std::move(*created));
    }

    std::mutex hostMutex;
    std::condition_variable_any hostChanged;
    std::promise<void> hostReady;
    auto started = hostReady.get_future();
    std::atomic<bool> hostAlive{false};
    std::optional<std::jthread> hostThread;

    if (borrowed) {
        hostThread.emplace([&](std::stop_token stop) {
            std::unique_lock lock(hostMutex);
            hostAlive.store(true, std::memory_order_release);
            hostReady.set_value();
            hostChanged.wait(lock, stop, [] {
                return false;
            });
            hostAlive.store(false, std::memory_order_release);
        });

        if (started.wait_for(5s) != std::future_status::ready) {
            return 2;
        }
    }

    Observation observation;
    std::weak_ptr<Capture> captureWitness;
    std::shared_ptr<q::api::Endpoint> endpoint;
    std::shared_ptr<Subscription> subscription;
    q::api::Endpoint::StateChangeListenerId observer = 0;
    {
        q::api::RuntimeOptions options;
        options.dnsServers.push_back({{127, 0, 0, 1}});
        auto runtime = borrowed ? std::make_unique<q::api::ApiRuntime>(*host, options)
                                : std::make_unique<q::api::ApiRuntime>(options);

        if (borrowed && !hostAlive.load(std::memory_order_acquire)) {
            return 3;
        }

        endpoint = runtime->createEndpoint();
        subscription = endpoint->getFeed()->createSubscription<q::events::Quote>();
        auto capture = std::make_shared<Capture>(endpoint, subscription, observation);
        captureWitness = capture;
        observer = endpoint->addStateChangeListener([capture](auto, auto current) {
            ++capture->observation.calls;

            if (current == q::api::EndpointState::CLOSING) {
                capture->endpoint->close();
                capture->subscription->close();
            }
        });
        capture.reset();

        if (observation.calls.load() != 0) {
            return 4;
        }

        runtime->closeAndAwaitTermination();

        if (!captureWitness.expired() || !observation.retired.load(std::memory_order_acquire) ||
            !observation.reentered.load(std::memory_order_acquire)) {
            return 5;
        }
    }

    if (borrowed && !hostAlive.load(std::memory_order_acquire)) {
        return 6;
    }

    if (endpoint->getState() != q::api::EndpointState::CLOSED ||
        subscription->getState() != q::api::SubscriptionState::CLOSED ||
        endpoint->getDiagnostic().code != q::api::ErrorCode::NONE ||
        subscription->getDiagnostic().code != q::api::ErrorCode::NONE ||
        endpoint->getStateChangeDiagnostic().code != q::api::ErrorCode::NONE) {
        return 7;
    }

    if (endpoint->awaitState(q::api::EndpointState::CLOSED, std::chrono::steady_clock::now()).status !=
            q::api::WaitStatus::REACHED ||
        subscription->awaitState(q::api::SubscriptionState::CLOSED, std::chrono::steady_clock::now()).status !=
            q::api::WaitStatus::REACHED) {
        return 8;
    }

    if (refusal([&] {
            subscription->addSymbols("IBM");
        }) != q::api::ErrorCode::CLOSED) {
        return 9;
    }

    const auto calls = observation.calls.load();
    const auto statistics = endpoint->getStatistics();
    const auto delivery = subscription->getStatistics();

    if (!delivery.final || !delivery.ownerIdentity || delivery.generation != 1 || delivery.pendingEvents != 0 ||
        delivery.inFlightEvents != 0 || delivery.activeSelectors != 0 || delivery.activeDesiredRevision ||
        delivery.callbackCalls.value != 0 ||
        delivery.callbackDurationMaximum.availability != q::core::StatisticAvailability::NO_SAMPLE) {
        return 15;
    }

    if (!statistics.final || !statistics.ownerIdentity || statistics.generation != 1 ||
        statistics.connection.connectionAttempts.value != 0 || statistics.connection.activeConnections != 0 ||
        statistics.connection.writtenDataRecords.availability != q::core::StatisticAvailability::UNSUPPORTED) {
        return 13;
    }

    std::weak_ptr<q::api::Endpoint> endpointWitness = endpoint;
    std::weak_ptr<Subscription> subscriptionWitness = subscription;
    std::atomic<bool> foreignPassed{false};
    std::thread foreign([endpoint = std::move(endpoint), subscription = std::move(subscription), observer,
                         &foreignPassed]() mutable {
        const bool closed = endpoint->getState() == q::api::EndpointState::CLOSED &&
                            subscription->getState() == q::api::SubscriptionState::CLOSED;
        const auto endpointWait = refusal([&] {
            static_cast<void>(endpoint->awaitState(q::api::EndpointState::CLOSED));
        });
        const auto subscriptionWait = refusal([&] {
            static_cast<void>(subscription->awaitState(q::api::SubscriptionState::CLOSED));
        });
        const auto mutation = refusal([&] {
            subscription->addSymbols("IBM");
        });
        endpoint->removeStateChangeListener(observer);
        endpoint->disconnect();
        endpoint->disconnectAndClear();
        endpoint->reconnect();
        endpoint->close();
        subscription->close();
        foreignPassed = closed && endpointWait == q::api::ErrorCode::WRONG_THREAD &&
                        subscriptionWait == q::api::ErrorCode::WRONG_THREAD && mutation == q::api::ErrorCode::CLOSED;
        subscription.reset();
        endpoint.reset();
    });
    foreign.join();

    if (!foreignPassed.load() || !endpointWitness.expired() || !subscriptionWitness.expired() ||
        observation.calls.load() != calls) {
        return 10;
    }

    if (!statistics.final || statistics.connection.readBytes.value != 0 || !delivery.final ||
        delivery.admittedEvents.value != 0 || !delivery.ownerIdentity) {
        return 14;
    }

    // The host thread joins before its pre-thread bootstrap is destroyed; closed handles never own SDK cleanup.
    return 0;
}
} // namespace

/** Run the finite public-header acceptance fixture through CTest.
 * @return Zero after both ownership modes; nonzero prints the mode and fixture stage, without SDK internals.
 */
int main() {
    try {
        for (const bool borrowed : {false, true}) {
            const auto code = exercise(borrowed);

            if (code != 0) {
                std::fprintf(stderr, "embedding mode=%s stage=%d\n", borrowed ? "borrowed" : "owned", code);

                return code;
            }
        }
    } catch (const q::api::Error &error) {
        const auto &value = error.diagnostic();
        std::fprintf(stderr, "embedding API=%u stage=%.*s native=%d detail=%u\n", static_cast<unsigned>(value.code),
                     static_cast<int>(value.stage.size()), value.stage.data(), value.nativeCode, value.detailCode);

        return 11;
    } catch (...) {
        std::fputs("embedding unexpected exception\n", stderr);

        return 12;
    }

    return 0;
}
