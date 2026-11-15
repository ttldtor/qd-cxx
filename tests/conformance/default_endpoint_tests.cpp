// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// Internal CTest runner: exercise the process-default runtime through normal process exit.
#include "qd/api/feed.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace api = org::ttldtor::qdcxx::api;
namespace events = org::ttldtor::qdcxx::events;

/** Fail the process even when an assertion runs after main has returned.
 * @param passed Expected condition.
 * @param message Static description of the assertion.
 */
void require(bool passed, const char *message) noexcept {
    if (!passed) {
        std::fprintf(stderr, "Default endpoint: %s\n", message);
        std::abort();
    }
}

/** Inspect a typed public rejection without relying on exception text.
 * @tparam Action Public operation returning normally or throwing api::Error.
 * @param action Operation to execute once.
 * @return Diagnostic retained by the exception, or NONE on success.
 */
template <class Action> api::Diagnostic diagnostic(Action action) {
    try {
        action();
    } catch (const api::Error &error) {
        return error.diagnostic();
    }

    return {};
}

/** A callback capture whose final release runs while the default runtime retires. */
struct CreateOnRelease {
    bool &rejected;        ///< Owned by the older static verification guard.
    std::thread::id owner; ///< Main thread that initialized the default runtime.
    bool expectWorker;     ///< True when the process started the runtime worker.

    ~CreateOnRelease() {
        require((std::this_thread::get_id() != owner) == expectWorker, "capture did not retire on the expected thread");

        const auto result = diagnostic([] {
            static_cast<void>(api::Endpoint::create());
        });
        require(result.code == api::ErrorCode::CLOSED && result.stage == "default-endpoint-runtime-stopping",
                "factory reentry during capture release did not reject before reaching the runtime");
        rejected = true;
    }
};

/** Start and observe a worker without contacting any network peer.
 * @param endpoint Disposable endpoint for an intentionally refused local directory.
 */
void startWorker(api::Endpoint &endpoint) {
    endpoint.connectTape(std::filesystem::path("."));

    constexpr auto DEADLINE = std::chrono::seconds(5);
    const auto result = endpoint.awaitState(api::EndpointState::FAILED, std::chrono::steady_clock::now() + DEADLINE);

    require(result.status == api::WaitStatus::REACHED && result.diagnostic.code == api::ErrorCode::TAPE_IO_FAILURE,
            "worker did not report the local tape path refusal");
    require(endpoint.closeAndAwaitTermination().diagnostic.code == api::ErrorCode::TAPE_IO_FAILURE,
            "disposable worker endpoint lost its terminal diagnostic");
}

/** Holds endpoint state across destruction of the younger default runtime. */
struct VerifyProcessExit {
    std::shared_ptr<api::Endpoint> endpoint;
    std::shared_ptr<api::Subscription<events::Quote>> subscription;
    std::shared_ptr<api::Subscription<events::Quote>> foreignReleasedSubscription;
    bool reentryRejected = false;

    ~VerifyProcessExit() {
        if (!endpoint) {
            return;
        }

        require(reentryRejected, "default runtime did not retire its callback captures before static verification");
        require(endpoint->getState() == api::EndpointState::CLOSED && endpoint->getStatistics().final,
                "endpoint facade did not detach in its final closed state after process teardown");
        require(subscription->getState() == api::SubscriptionState::CLOSED,
                "retained subscription did not detach on teardown");
        require(foreignReleasedSubscription->getState() == api::SubscriptionState::CLOSED,
                "subscription from foreign-thread endpoint release did not detach");
        require(endpoint->closeAndAwaitTermination().drained, "detached endpoint lost its terminal outcome");
        const auto result = diagnostic([] {
            static_cast<void>(api::Endpoint::create());
        });
        require(result.code == api::ErrorCode::CLOSED && result.stage == "default-endpoint-runtime-stopping",
                "factory creation after teardown touched the destroyed runtime");
    }
};
} // namespace

/** Verify default endpoint lifetime, thread policy and finite shared admission.
 * @param argc Count including the program name.
 * @param argv Optional --worker runs initialized-worker teardown instead of inactive retirement.
 * @return Zero on success including static verification, or 2 on invalid arguments.
 */
int main(int argc, char **argv) {
    const bool withWorker = argc == 2 && std::string_view(argv[1]) == "--worker";

    if (argc != 1 && !withWorker) {
        return 2;
    }

    static VerifyProcessExit exitCheck;
    const auto capacity = api::RuntimeOptions{}.maximumEndpoints;
    require(capacity >= 3, "test requires at least three default endpoint slots");

    std::vector<std::shared_ptr<api::Endpoint>> endpoints;
    endpoints.reserve(capacity);

    for (std::size_t index = 0; index < capacity; ++index) {
        auto endpoint = api::Endpoint::create();
        require(endpoint->getState() == api::EndpointState::NOT_CONNECTED, "factory endpoint connected implicitly");
        endpoints.push_back(std::move(endpoint));
    }

    const auto overflow = diagnostic([] {
        static_cast<void>(api::Endpoint::create());
    });
    require(overflow.code == api::ErrorCode::RESOURCE_LIMIT && overflow.stage == "runtime-endpoint-count",
            "factory endpoints did not share one bounded runtime");
    require(endpoints.front()->getFeed() != endpoints[1]->getFeed(), "factory calls reused the same endpoint feed");

    auto retired = endpoints.front()->getFeed()->createSubscription<events::Quote>();
    require(endpoints.front()->closeAndAwaitTermination().drained, "inactive endpoint did not drain");
    require(endpoints.front()->closeAndAwaitTermination().drained, "repeated close lost the terminal result");
    require(retired->getState() == api::SubscriptionState::CLOSED, "first endpoint left its subscription open");

    auto independent = endpoints[1]->getFeed()->createSubscription<events::Quote>();
    independent->addSymbols("AAPL");
    require(independent->getState() == api::SubscriptionState::CONFIGURED,
            "closing first endpoint affected the second");

    auto replacement = api::Endpoint::create();
    require(replacement->getState() == api::EndpointState::NOT_CONNECTED, "factory did not reuse a retired slot");

    if (withWorker) {
        startWorker(*endpoints[1]);
    }

    exitCheck.foreignReleasedSubscription = endpoints.back()->getFeed()->createSubscription<events::Quote>();
    auto foreignEndpoint = std::move(endpoints.back());

    std::jthread foreign([endpoint = std::move(foreignEndpoint)]() mutable {
        const auto create = diagnostic([] {
            static_cast<void>(api::Endpoint::create());
        });
        require(create.code == api::ErrorCode::WRONG_THREAD && create.stage == "default-endpoint-create-thread",
                "factory accepted creation on a foreign thread");
        const auto wait = diagnostic([&] {
            static_cast<void>(endpoint->closeAndAwaitTermination());
        });
        require(wait.code == api::ErrorCode::WRONG_THREAD, "foreign endpoint close wait was accepted");
        endpoint.reset();
    });
    foreign.join();

    auto pending = replacement->getFeed()->createSubscription<events::Quote>();
    pending->addEventListener(
        [capture = std::make_shared<CreateOnRelease>(exitCheck.reentryRejected, std::this_thread::get_id(),
                                                     withWorker)](std::span<const events::Quote>) {
        });
    exitCheck.subscription = std::move(pending);
    exitCheck.endpoint = std::move(replacement);

    return 0;
}
