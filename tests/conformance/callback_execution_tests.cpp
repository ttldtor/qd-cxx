// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
// CTest runner: finite owned host policies and actual production pool/observer paths; no host thread pool.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../core/delivery_statistics.hpp"
#include "callback_execution.hpp"
#include "connect_peer.hpp"
#include "feed_internal.hpp"
#include "history_observation.hpp"
#include "qd/api/statistics.hpp"
#include "subscription_observer.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <condition_variable>
#include <doctest/doctest.h>
#include <functional>
#include <optional>
#include <thread>

namespace qd = org::ttldtor::qdcxx;
namespace api = qd::api;
namespace detail = qd::api::detail;
using namespace std::chrono_literals;

namespace {
class Policy final : public api::CallbackExecutor {
    public:
    std::function<api::CallbackPostResult(api::CallbackTask)> action;

    api::CallbackPostResult post(api::CallbackTask task) override {
        return action(std::move(task));
    }
};

/** Join the SDK before any test-owned callback state declared before this guard can disappear. */
struct RuntimeJoin {
    api::ApiRuntime &runtime;

    ~RuntimeJoin() {
        runtime.closeAndAwaitTermination();
    }
};

struct Observation {
    unsigned calls = 0;
    unsigned retirements = 0;
    api::Diagnostic diagnostic;
};

struct Invocation final : detail::CallbackInvocation {
    std::shared_ptr<Policy> policy;
    Observation &observed;
    std::function<void()> function;
    std::function<void()> retirement;

    Invocation(std::shared_ptr<Policy> policy, Observation &observed, std::function<void()> function,
               std::function<void()> retirement)
        : policy(std::move(policy)), observed(observed), function(std::move(function)),
          retirement(std::move(retirement)) {
    }

    void prepare() override {
    }

    api::CallbackPostResult post(api::CallbackTask task) override {
        return policy->post(std::move(task));
    }

    void execute() noexcept override {
        ++observed.calls;

        if (function)
            function();
    }

    void finish(api::Diagnostic diagnostic) noexcept override {
        observed.diagnostic = diagnostic;
        function = {};
        policy.reset();

        if (retirement)
            retirement();

        retirement = {};
        ++observed.retirements;
    }
};

struct Harness {
    std::shared_ptr<detail::CallbackSlotPool> pool = std::make_shared<detail::CallbackSlotPool>(2, 2);
    std::shared_ptr<detail::SubscriptionChangeDomain> domain = std::make_shared<detail::SubscriptionChangeDomain>();
    std::shared_ptr<qd::transport::detail::CallbackRetirementFence> fence =
        std::make_shared<qd::transport::detail::CallbackRetirementFence>();
    std::shared_ptr<Policy> policy = std::make_shared<Policy>();
    std::optional<api::CallbackTask> task;
    Observation observed;
    std::size_t slot;

    Harness() : slot(pool->assign()) {
        domain->continuations.resize(8);
        domain->maximumWork = domain->remainingWork = 8;
        pool->reserve(slot, true);
        policy->action = [&](api::CallbackTask value) {
            task.emplace(std::move(value));

            return api::CallbackPostResult::ACCEPTED;
        };
    }

    ~Harness() {
        pool->cancel(slot, {api::ErrorCode::CALLBACK_CANCELLED, "test-retirement"});
        task.reset();
    }

    void submit(std::function<void()> action = {}, std::function<void()> retire = {}) {
        pool->begin(slot, {domain, fence.get(), {}});
        pool->submit(slot, std::make_unique<Invocation>(policy, observed, std::move(action), std::move(retire)));
    }
};

template <class Predicate> bool eventually(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;

    while (!predicate()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;

        std::this_thread::sleep_for(1ms);
    }

    return true;
}

api::RuntimeOptions runtimeOptions() {
    api::RuntimeOptions options;
    options.dnsServers.push_back({{127, 0, 0, 1}});

    return options;
}

api::EndpointOptions endpointOptions() {
    api::EndpointOptions options;
    options.contract = api::DeliveryContract::STREAM;
    options.maximumAttempts = 1;
    options.readTimeout = 10s;

    return options;
}

qd::core::LocalSchema quoteSchema() {
    constexpr qd::core::SchemaLimits limits{16, 128, 256, 128, 16384};
    auto value = qd::events::defaultQuoteSchema(limits);
    REQUIRE(value);
    auto schema = qd::core::LocalSchema::create(value->records(), limits, qd::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);

    return std::move(*schema);
}

std::vector<qd::core::FieldValue> quoteFields(const qd::core::LocalSchema &schema) {
    std::vector<qd::core::FieldValue> result;

    for (const auto &field : schema.records()[0].fields) {
        auto type = static_cast<qd::core::SerialFieldType>(field.serialTypeId);
        result.push_back({type, type == qd::core::SerialFieldType::TIME_SECONDS ? 3 : 0});
    }

    return result;
}
} // namespace

TEST_CASE("callback tasks run once, cancel without host progress and reject old generations") {
    Harness h;
    h.submit();
    CHECK_FALSE(h.pool->snapshot(h.slot).quiescent);
    REQUIRE(h.task);
    CHECK(h.task->run() == api::CallbackTaskRunResult::COMPLETED);
    CHECK(h.task->run() == api::CallbackTaskRunResult::CANCELLED);
    CHECK(h.observed.calls == 1);
    CHECK(h.observed.retirements == 1);
    CHECK(h.pool->snapshot(h.slot).retirementRevision == 1);
    auto old = std::move(h.task);
    h.task.reset();
    h.submit();
    CHECK(old->run() == api::CallbackTaskRunResult::CANCELLED);
    h.pool->cancel(h.slot, {api::ErrorCode::CALLBACK_CANCELLED, "test-close"});
    CHECK(h.task->run() == api::CallbackTaskRunResult::CANCELLED);
    CHECK(h.observed.calls == 1);
    CHECK(h.observed.retirements == 2);
    CHECK(h.fence->outstanding.load() == 0);
}

TEST_CASE("callback scheduling rejection throw drop and contradictory inline outcome are explicit") {
    for (int mode = 0; mode != 5; ++mode) {
        Harness h;
        h.policy->action = [mode](api::CallbackTask task) {
            if (mode == 1)
                throw std::runtime_error("host private text");

            if (mode >= 3)
                CHECK(task.run() == api::CallbackTaskRunResult::COMPLETED);

            return mode == 0 || mode == 3 ? api::CallbackPostResult::REJECTED : api::CallbackPostResult::ACCEPTED;
        };
        h.submit();
        const auto expected = mode == 0   ? api::ErrorCode::EXECUTOR_REJECTED
                              : mode == 1 ? api::ErrorCode::EXECUTOR_FAILURE
                              : mode == 2 ? api::ErrorCode::EXECUTOR_DROPPED
                              : mode == 3 ? api::ErrorCode::EXECUTOR_CONTRACT
                                          : api::ErrorCode::NONE;
        CHECK(h.observed.diagnostic.code == expected);
        CHECK(h.observed.calls == (mode >= 3 ? 1 : 0));
        CHECK(h.observed.retirements == 1);
        CHECK(h.pool->snapshot(h.slot).quiescent);
    }
}

TEST_CASE("callback post guard survives parallel run before post returns and close cannot preempt running") {
    Harness h;
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false, release = false;
    std::jthread worker;
    h.policy->action = [&](api::CallbackTask task) {
        worker = std::jthread([task = std::move(task), &h, &mutex, &condition, &entered, &release] {
            static_cast<void>(task.run());
        });
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return entered;
        }));
        h.pool->cancel(h.slot, {api::ErrorCode::CALLBACK_CANCELLED, "test-running"});
        CHECK_FALSE(h.pool->snapshot(h.slot).quiescent);
        CHECK(h.observed.retirements == 0);
        release = true;
        condition.notify_all();
        lock.unlock();
        worker.join();
        CHECK(h.observed.retirements == 0); // User returned; post guard still owns the selected packet.

        return api::CallbackPostResult::ACCEPTED;
    };
    h.submit([&] {
        std::unique_lock lock(mutex);
        entered = true;
        condition.notify_all();
        condition.wait_for(lock, 5s, [&] {
            return release;
        });
    });
    CHECK(h.observed.calls == 1);
    CHECK(h.observed.retirements == 1);
    CHECK(h.fence->outstanding.load() == 0);
}

TEST_CASE("nested task delivery waits for final selected capture retirement and shares finite turn credits") {
    Harness outer, inner;
    bool user = false, capture = false;
    inner.submit([&] {
        CHECK_FALSE(user);
        CHECK(capture);
    });
    outer.policy->action = [](api::CallbackTask task) {
        CHECK(task.run() == api::CallbackTaskRunResult::COMPLETED);

        return api::CallbackPostResult::ACCEPTED;
    };
    outer.submit(
        [&] {
            user = true;
            CHECK(inner.task->run() == api::CallbackTaskRunResult::DEFERRED);
            CHECK(inner.observed.calls == 0);
            user = false;
        },
        [&] {
            capture = true;
            CHECK(inner.observed.calls == 0);
        });
    CHECK(inner.observed.calls == 1);
    CHECK(inner.observed.retirements == 1);
    CHECK(outer.observed.retirements == 1);
    CHECK(inner.fence->outstanding.load() == 0);
}

TEST_CASE("nested queued tasks in one runtime do not keep their own after-return driver blocked") {
    Harness outer, inner;
    inner.domain = outer.domain;
    bool captured = false;
    inner.submit([&] {
        CHECK(captured);
    });
    outer.policy->action = [](api::CallbackTask task) {
        CHECK(task.run() == api::CallbackTaskRunResult::COMPLETED);

        return api::CallbackPostResult::ACCEPTED;
    };
    outer.submit(
        [&] {
            CHECK(inner.task->run() == api::CallbackTaskRunResult::DEFERRED);
        },
        [&] {
            captured = true;
        });
    CHECK(inner.observed.calls == 1);
    CHECK(inner.observed.retirements == 1);
}

TEST_CASE("nested A B A tasks are iterative and exhausted root admission cancels only the refused work") {
    Harness a, b, refused;
    b.domain = a.domain;
    refused.domain = a.domain;
    a.domain->maximumWork = a.domain->remainingWork = 2;
    unsigned depth = 0, maximumDepth = 0, aCalls = 0;
    a.policy->action = [](api::CallbackTask task) {
        const auto result = task.run();
        CHECK(result != api::CallbackTaskRunResult::CANCELLED);

        return api::CallbackPostResult::ACCEPTED;
    };
    refused.submit();
    b.submit([&] {
        ++depth;
        maximumDepth = std::max(maximumDepth, depth);
        a.submit([&] {
            ++depth;
            maximumDepth = std::max(maximumDepth, depth);
            ++aCalls;
            --depth;
        });
        CHECK(refused.task->run() == api::CallbackTaskRunResult::CANCELLED);
        --depth;
    });
    a.submit([&] {
        ++depth;
        maximumDepth = std::max(maximumDepth, depth);
        CHECK(b.task->run() == api::CallbackTaskRunResult::DEFERRED);
        --depth;
    });
    CHECK(aCalls == 1);
    CHECK(maximumDepth == 1);
    CHECK(refused.observed.calls == 0);
    CHECK(refused.observed.diagnostic.code == api::ErrorCode::WORK_LIMIT);
    CHECK(a.observed.retirements == 2);
    CHECK(b.observed.retirements == 1);
}

TEST_CASE("cancellation of a deferred target releases captures before its stale continuation is visited") {
    Harness a, b;
    b.submit();
    a.policy->action = [](api::CallbackTask task) {
        static_cast<void>(task.run());

        return api::CallbackPostResult::ACCEPTED;
    };
    a.submit([&] {
        CHECK(b.task->run() == api::CallbackTaskRunResult::DEFERRED);
        b.pool->cancel(b.slot, {api::ErrorCode::CALLBACK_CANCELLED, "test-detach"});
        CHECK(b.observed.retirements == 1);
        CHECK(b.fence->outstanding.load() == 0);
    });
    CHECK(b.observed.calls == 0);
    CHECK(b.observed.retirements == 1);
}

TEST_CASE("parallel data cohort clears its lifetime fence only on last exit") {
    Harness h;
    std::barrier barrier(2);
    std::atomic<bool> firstLeft{false};
    std::jthread worker([&] {
        detail::SubscriptionDataCallbackScope scope(h.domain);
        barrier.arrive_and_wait();

        while (!firstLeft.load())
            std::this_thread::yield();

        CHECK(h.domain->fences[0].active.load());
    });
    {
        detail::SubscriptionDataCallbackScope scope(h.domain);
        barrier.arrive_and_wait();
    }
    CHECK(h.domain->fences[0].active.load());
    firstLeft.store(true);
    worker.join();
    CHECK_FALSE(h.domain->fences[0].active.load());
}

TEST_CASE("pure task stubs retain a single pool scope across many retired logical registrations") {
    Harness h;
    auto pool = h.pool;
    std::vector<api::CallbackTask> old;
    old.reserve(64);

    for (unsigned i = 0; i != 64; ++i) {
        h.submit();
        CHECK(h.task->run() == api::CallbackTaskRunResult::COMPLETED);
        old.push_back(std::move(*h.task));
        h.task.reset();
    }

    CHECK(pool->snapshot(h.slot).retirementRevision == 64);
    CHECK(h.fence->outstanding.load() == 0);

    for (const auto &task : old)
        CHECK(task.run() == api::CallbackTaskRunResult::CANCELLED);
}

TEST_CASE("public event policy validates transactional capacity before activation and keeps overrides separate") {
    auto options = runtimeOptions();
    options.maximumCustomCallbackSubscriptions = 1;
    api::ApiRuntime runtime(options);
    auto endpoint = runtime.createEndpoint();
    auto a = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    auto b = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    auto policy = std::make_shared<Policy>();
    policy->action = [](api::CallbackTask) {
        return api::CallbackPostResult::REJECTED;
    };
    CHECK_THROWS_AS(endpoint->setEventExecutor(policy), api::Error);
    CHECK_FALSE(endpoint->getEventExecutor());
    CHECK_FALSE(a->getEventExecutor());
    a->setEventExecutor(policy);
    CHECK(a->getEventExecutor() == policy);
    CHECK_THROWS_AS(b->setEventExecutor(policy), api::Error);
    a->setEventExecutor(nullptr);
    b->setEventExecutor(policy);
    CHECK(b->getEventExecutor() == policy);
    CHECK(endpoint->getState() == api::EndpointState::NOT_CONNECTED);
    runtime.closeAndAwaitTermination();
    CHECK_THROWS_AS(b->setEventExecutor(nullptr), api::Error);
}

TEST_CASE("actual event packet remains in flight until host task runs or SDK close cancels it") {
    for (const bool cancel : {false, true}) {
        api::ApiRuntime runtime(runtimeOptions());
        auto schema = quoteSchema();
        std::vector<std::u16string> symbols{u"IBM"};
        std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
        qd::connect_tests::PeerScript script;
        script.canonicalSymbols = true;
        qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
        auto endpoint = runtime.createEndpoint(endpointOptions());
        std::mutex mutex;
        std::condition_variable ready;
        std::optional<api::CallbackTask> task;
        auto policy = std::make_shared<Policy>();
        policy->action = [&](api::CallbackTask value) {
            {
                std::lock_guard lock(mutex);
                task.emplace(std::move(value));
            }
            ready.notify_one();

            return api::CallbackPostResult::ACCEPTED;
        };
        endpoint->setEventExecutor(policy);
        auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
        std::atomic<unsigned> calls{0};
        sub->addEventListener([&](std::span<const qd::events::Quote> values) {
            CHECK(values.size() == 1);
            CHECK(values.front().symbol == u"IBM");
            const auto selfAwaitCode = [](auto action) {
                try {
                    action();
                } catch (const api::Error &error) {
                    return error.diagnostic().code;
                }

                return api::ErrorCode::NONE;
            };
            CHECK(selfAwaitCode([&] {
                      runtime.closeAndAwaitTermination();
                  }) == api::ErrorCode::INVALID_STATE);
            CHECK(selfAwaitCode([&] {
                      static_cast<void>(endpoint->closeAndAwaitTermination());
                  }) == api::ErrorCode::INVALID_STATE);
            ++calls;
        });
        sub->addSymbols("IBM");
        RuntimeJoin join{runtime};
        endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
        {
            std::unique_lock lock(mutex);
            REQUIRE(ready.wait_for(lock, 5s, [&] {
                return task.has_value();
            }));
        }
        auto pending = sub->getStatistics();
        CHECK(pending.inFlightEvents == 1);
        CHECK(pending.deliveredEvents.value == 0);
        CHECK(pending.callbackCalls.value == 0);

        if (cancel) {
            endpoint->close();
            CHECK(eventually([&] {
                return sub->getStatistics().inFlightEvents == 0;
            }));
            CHECK(task->run() == api::CallbackTaskRunResult::CANCELLED);
            CHECK(sub->getCallbackDispatchDiagnostic().code == api::ErrorCode::CALLBACK_CANCELLED);
        } else {
            CHECK(task->run() == api::CallbackTaskRunResult::COMPLETED);
            CHECK(eventually([&] {
                return sub->getStatistics().deliveredEvents.value == 1;
            }));
        }

        static_cast<void>(endpoint->closeAndAwaitTermination());
        CHECK(calls.load() == (cancel ? 0 : 1));
        const auto final = sub->getStatistics();
        CHECK(final.inFlightEvents == 0);
        CHECK(final.final);
        CHECK(final.deliveredEvents.value + final.discardedEvents.value + final.callbackFailedEvents.value == 1);
        runtime.closeAndAwaitTermination();
    }
}

TEST_CASE("actual scheduling refusal and user throw conserve selected events without silent retry") {
    for (unsigned mode = 0; mode != 6; ++mode) {
        api::ApiRuntime runtime(runtimeOptions());
        auto schema = quoteSchema();
        std::vector<std::u16string> symbols{u"IBM"};
        std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
        qd::connect_tests::PeerScript script;
        script.canonicalSymbols = true;
        qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
        auto policy = std::make_shared<Policy>();
        policy->action = [mode](api::CallbackTask task) {
            if (mode == 1)
                throw std::runtime_error("host private message");

            if (mode >= 3)
                static_cast<void>(task.run());

            if (mode == 4)
                throw std::runtime_error("contradictory host result");

            return mode == 0 || mode == 3 ? api::CallbackPostResult::REJECTED : api::CallbackPostResult::ACCEPTED;
        };
        sub->setEventExecutor(policy);
        sub->addEventListener([mode](std::span<const qd::events::Quote>) {
            if (mode == 5)
                throw std::runtime_error("application private message");
        });
        sub->addSymbols("IBM");
        RuntimeJoin join{runtime};
        endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
        REQUIRE(eventually([&] {
            return sub->getStatistics().final;
        }));
        auto stats = sub->getStatistics();
        CHECK(stats.inFlightEvents == 0);
        CHECK(stats.deliveredEvents.value + stats.discardedEvents.value + stats.callbackFailedEvents.value == 1);
        CHECK(stats.callbackCalls.value == (mode >= 3 ? 1 : 0));
        CHECK(stats.deliveredEvents.value == (mode == 3 || mode == 4 ? 1 : 0));
        CHECK(stats.callbackFailedEvents.value == (mode == 5 ? 1 : 0));
        const auto expected = mode == 0   ? api::ErrorCode::EXECUTOR_REJECTED
                              : mode == 1 ? api::ErrorCode::EXECUTOR_FAILURE
                              : mode == 2 ? api::ErrorCode::EXECUTOR_DROPPED
                              : mode == 5 ? api::ErrorCode::NONE
                                          : api::ErrorCode::EXECUTOR_CONTRACT;
        CHECK(sub->getCallbackDispatchDiagnostic().code == expected);
        CHECK(sub->getState() == api::SubscriptionState::FAILED);
        static_cast<void>(endpoint->closeAndAwaitTermination());
        runtime.closeAndAwaitTermination();
    }
}

TEST_CASE("a slow queued custom subscription leaves the SDK worker and sibling delivery available") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = quoteSchema();
    std::vector<std::u16string> symbols{u"IBM"};
    std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
    qd::connect_tests::PeerScript script;
    script.canonicalSymbols = true;
    script.waitForDataRelease = true;
    qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto slow = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    auto fast = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    std::mutex mutex;
    std::optional<api::CallbackTask> task;
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask value) {
        std::lock_guard lock(mutex);
        task.emplace(std::move(value));

        return api::CallbackPostResult::ACCEPTED;
    };
    slow->setEventExecutor(policy);
    std::atomic<unsigned> slowCalls{0}, fastCalls{0};
    slow->addEventListener([&](auto) {
        ++slowCalls;
    });
    fast->addEventListener([&](auto) {
        ++fastCalls;
    });
    slow->addSymbols("IBM");
    fast->addSymbols("IBM");
    RuntimeJoin join{runtime};
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    REQUIRE(eventually([&] {
        const auto a = slow->getStatistics(), b = fast->getStatistics();

        return a.activeSelectors == 1 && b.activeSelectors == 1 &&
               a.activeDesiredRevision == slow->getSnapshot().revision &&
               b.activeDesiredRevision == fast->getSnapshot().revision;
    }));
    peer.releaseData();
    REQUIRE(eventually([&] {
        return fastCalls.load() == 1;
    }));
    CHECK(slowCalls.load() == 0);
    CHECK(slow->getStatistics().inFlightEvents == 1);
    slow->close();
    REQUIRE(eventually([&] {
        return slow->getStatistics().final;
    }));
    {
        std::lock_guard lock(mutex);
        REQUIRE(task);
        CHECK(task->run() == api::CallbackTaskRunResult::CANCELLED);
    }
    CHECK(fastCalls.load() == 1);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
}

TEST_CASE("running callback and final capture retirement fence the next positive source epoch") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = quoteSchema();
    std::vector<std::u16string> symbols{u"IBM"};
    std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
    qd::connect_tests::PeerScript script;
    script.canonicalSymbols = true;
    script.afterReconnect = records;
    qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto settings = endpointOptions();
    settings.maximumAttempts = 2; // Explicit reconnect consumes another original attempt; never replenishes it.
    auto endpoint = runtime.createEndpoint(settings);
    auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    std::optional<api::CallbackTask> task;
    bool inUser = false, releaseUser = false, inCapture = false, releaseCapture = false;
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask value) {
        {
            std::lock_guard lock(mutex);
            task.emplace(std::move(value));
        }
        condition.notify_all();

        return api::CallbackPostResult::ACCEPTED;
    };
    sub->setEventExecutor(policy);

    struct Capture {
        std::function<void()> action;

        ~Capture() {
            action();
        }
    };

    auto capture = std::make_shared<Capture>();
    capture->action = [&] {
        // Reentry here proves the selected capture is released outside port/pool locks.
        static_cast<void>(sub->getStatistics());
        sub->setEventExecutor(nullptr);
        std::unique_lock lock(mutex);
        inCapture = true;
        condition.notify_all();
        CHECK(condition.wait_for(lock, 5s, [&] {
            return releaseCapture;
        }));
    };
    auto id = sub->addEventListener([&, capture](auto) {
        std::unique_lock lock(mutex);
        inUser = true;
        condition.notify_all();
        CHECK(condition.wait_for(lock, 5s, [&] {
            return releaseUser;
        }));
    });
    capture.reset();
    sub->addSymbols("IBM");
    RuntimeJoin join{runtime};
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return task.has_value();
        }));
    }
    REQUIRE(endpoint->awaitState(api::EndpointState::CONNECTED, std::chrono::steady_clock::now() + 5s).status ==
            api::WaitStatus::REACHED);
    auto old = sub->getStatistics().sourceEpoch;
    REQUIRE(old);
    CHECK(*old > 0);
    auto selected = std::move(*task);
    task.reset();
    std::jthread host([task = std::move(selected)] {
        CHECK(task.run() == api::CallbackTaskRunResult::COMPLETED);
    });
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return inUser;
        }));
    }
    sub->removeEventListener(id);
    endpoint->reconnect();
    REQUIRE(eventually([&] {
        return endpoint->getState() == api::EndpointState::CONNECTING;
    }));
    const auto runningStatistics = sub->getStatistics();
    CHECK((runningStatistics.sourceEpoch.value_or(0) == 0 || runningStatistics.sourceEpoch == old));
    CHECK(sub->getStatistics().inFlightEvents == 1);
    {
        std::lock_guard lock(mutex);
        releaseUser = true;
    }
    condition.notify_all();
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return inCapture;
        }));
    }
    // Physical close can publish an inactive cut; it must not establish the replacement source yet.
    REQUIRE(eventually([&] {
        return endpoint->getStatistics().connection.activeConnections == 0;
    }));
    const auto captureStatistics = sub->getStatistics();
    CAPTURE(*old);
    CAPTURE(captureStatistics.sourceEpoch.has_value());
    CAPTURE(captureStatistics.sourceEpoch.value_or(0));
    CAPTURE(captureStatistics.inFlightEvents);
    CAPTURE(static_cast<int>(endpoint->getState()));
    CHECK((captureStatistics.sourceEpoch.value_or(0) == 0 || captureStatistics.sourceEpoch == old));
    CHECK(captureStatistics.inFlightEvents == 1);
    CHECK(peer.adds() == 1);
    {
        std::lock_guard lock(mutex);
        releaseCapture = true;
    }
    condition.notify_all();
    host.join();
    REQUIRE(eventually([&] {
        const auto stats = sub->getStatistics();

        return stats.sourceEpoch && *stats.sourceEpoch > 0 && stats.sourceEpoch != old;
    }));
    CHECK(eventually([&] {
        return peer.adds() == 2;
    })); // Positive local epoch is not a peer ADD receipt.
    CHECK(sub->getStatistics().deliveredEvents.value == 1);
    static_cast<void>(endpoint->closeAndAwaitTermination());
    runtime.closeAndAwaitTermination();
    const auto final = sub->getStatistics();
    CHECK(final.final);
    CHECK(final.inFlightEvents == 0);
    CHECK(final.pendingEvents == 0);
    CHECK(final.admittedEvents.value == final.deliveredEvents.value + final.discardedEvents.value);
}

TEST_CASE("custom Order HISTORY completion waits for accepted task and final selected capture") {
    using Type = qd::core::SerialFieldType;
    constexpr std::int32_t SNAPSHOT_BEGIN = 4;
    constexpr std::int32_t SNAPSHOT_END = 8;
    // Actual default Order physical slots: VOID high word, Index low word and wide-value tails.
    constexpr std::array names{u"Void", u"Index",         u"Time",  u"Sequence",       u"Price", u"Price$VoidTail",
                               u"Size", u"Size$VoidTail", u"Count", u"Count$VoidTail", u"Flags", u"MMID"};
    constexpr std::array types{Type::VOID_FIELD,   Type::COMPACT_INT, Type::TIME_SECONDS, Type::SEQUENCE,
                               Type::WIDE_DECIMAL, Type::VOID_FIELD,  Type::WIDE_DECIMAL, Type::VOID_FIELD,
                               Type::WIDE_DECIMAL, Type::VOID_FIELD,  Type::COMPACT_INT,  Type::SHORT_STRING};
    qd::core::LocalRecord physical{0, u"Order", false, {}, {}};
    std::vector<qd::core::FieldValue> fields;

    for (std::size_t i = 0; i < names.size(); ++i) {
        physical.fields.push_back({static_cast<std::int32_t>(types[i])});
        physical.lookups.push_back({names[i], i});
        fields.push_back({types[i], 0});
    }

    const std::array records{std::move(physical)};
    auto prepared =
        qd::core::LocalSchema::create(records, {16, 128, 256, 128, 16384}, qd::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(prepared);
    auto schema = std::move(*prepared);
    auto limits = runtimeOptions();
    limits.maximumSubscriptionsPerEndpoint = 1;
    limits.maximumCustomCallbackSubscriptions = 1;
    api::ApiRuntime runtime(limits);
    qd::connect_tests::PeerScript script;
    script.family = qd::core::SubscriptionFamily::HISTORY;
    script.historyBound = 0;
    script.canonicalSymbols = true;
    const std::array source{qd::connect_tests::Record{u"IBM", std::move(fields), SNAPSHOT_BEGIN | SNAPSHOT_END}};
    const std::array<std::u16string, 1> symbols{u"IBM"};
    qd::connect_tests::Peer peer(schema, source, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto settings = endpointOptions();
    settings.contract = api::DeliveryContract::HISTORY;
    auto endpoint = runtime.createEndpoint(settings);
    auto sub = endpoint->getFeed()->createSubscription<qd::events::Order>();
    std::mutex mutex;
    std::condition_variable condition;
    std::optional<api::CallbackTask> task;
    bool inCapture = false, releaseCapture = false;
    std::atomic<unsigned> calls{0};
    std::atomic<bool> returned{false};
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask value) {
        {
            std::lock_guard lock(mutex);
            task.emplace(std::move(value));
        }
        condition.notify_all();

        return api::CallbackPostResult::ACCEPTED;
    };
    sub->setEventExecutor(policy);

    struct Capture {
        std::function<void()> action;

        ~Capture() {
            action();
        }
    };

    auto capture = std::make_shared<Capture>();
    capture->action = [&] {
        CHECK(calls.load() == 1);
        CHECK(sub->getHistorySnapshot().state == api::HistoryObservationState::CALLBACK_PENDING);
        sub->setEventExecutor(nullptr); // Reentry proves selected capture release is outside control/pool locks.
        std::unique_lock lock(mutex);
        inCapture = true;
        condition.notify_all();
        CHECK(condition.wait_for(lock, 5s, [&] {
            return releaseCapture;
        }));
    };
    api::Subscription<qd::events::Order>::ListenerId id = 0;
    id = sub->addEventListener([&, capture](std::span<const qd::events::Order> batch) {
        REQUIRE(batch.size() == 1);
        CHECK(batch[0].symbol == u"IBM");
        CHECK(batch[0].eventFlags == (SNAPSHOT_BEGIN | SNAPSHOT_END));
        ++calls;
        sub->removeEventListener(id);
    });
    capture.reset();
    sub->addSymbols("IBM");
    RuntimeJoin join{runtime};
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return task.has_value();
        }));
    }
    api::HistorySubscriptionSnapshot held;
    REQUIRE(eventually([&] {
        held = sub->getHistorySnapshot();

        return held.publicationSerial > 0 && held.sourceEpoch && held.mappingLogicalRevision &&
               *held.mappingLogicalRevision == held.acceptedLogicalRevision;
    }));
    CHECK(held.state == api::HistoryObservationState::CALLBACK_PENDING);
    CHECK(calls.load() == 0);
    CHECK(sub->getStatistics().inFlightEvents == 1);

    for (const auto &range : held.ranges)
        CHECK_FALSE(range.status.completeDeliveredRange);

    auto selected = std::move(*task);
    task.reset();
    std::jthread host([&, selected = std::move(selected)] {
        CHECK(selected.run() == api::CallbackTaskRunResult::COMPLETED);
        returned = true;
    });
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return inCapture;
        }));
    }
    const auto retiring = sub->getHistorySnapshot();
    CHECK(retiring.state == api::HistoryObservationState::CALLBACK_PENDING);
    CHECK(retiring.callbackRetirementRevision == held.callbackRetirementRevision);
    CHECK(sub->getStatistics().inFlightEvents == 1);
    CHECK_FALSE(returned.load());
    {
        std::lock_guard lock(mutex);
        releaseCapture = true;
    }
    condition.notify_all();
    host.join();
    api::HistorySubscriptionSnapshot complete;
    REQUIRE(eventually([&] {
        complete = sub->getHistorySnapshot();

        return complete.state == api::HistoryObservationState::CURRENT && complete.ranges.size() == 1 &&
               complete.ranges[0].status.completeDeliveredRange;
    }));
    CHECK(complete.sourceEpoch == held.sourceEpoch);
    CHECK(complete.callbackRetirementRevision == held.callbackRetirementRevision + 1);
    CHECK(complete.ranges[0].symbol == u"IBM");
    CHECK(sub->getStatistics().deliveredEvents.value == 1);
    CHECK(sub->getStatistics().inFlightEvents == 0);
    CHECK_FALSE(peer.failed());
    runtime.closeAndAwaitTermination();
    CHECK(sub->getStatistics().final);
    CHECK(sub->getStatistics().admittedEvents.value == sub->getStatistics().deliveredEvents.value);
}

TEST_CASE("creator shutdown waits for actual running work and selected capture retirement") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = quoteSchema();
    std::vector<std::u16string> symbols{u"IBM"};
    std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
    qd::connect_tests::PeerScript script;
    script.canonicalSymbols = true;
    qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    bool entered = false, release = false;
    std::atomic<bool> captured{false}, awaited{false};
    std::jthread host;
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask task) {
        host = std::jthread([task = std::move(task)] {
            static_cast<void>(task.run());
        });

        return api::CallbackPostResult::ACCEPTED;
    };
    sub->setEventExecutor(policy);

    struct Capture {
        std::atomic<bool> &captured;
        std::atomic<bool> &awaited;

        ~Capture() {
            CHECK_FALSE(awaited.load());
            captured.store(true);
        }
    };

    auto capture = std::make_shared<Capture>(captured, awaited);
    const auto id = sub->addEventListener([&, capture](auto) {
        std::unique_lock lock(mutex);
        entered = true;
        condition.notify_all();
        CHECK(condition.wait_for(lock, 5s, [&] {
            return release;
        }));
    });
    capture.reset();
    sub->addSymbols("IBM");
    RuntimeJoin join{runtime};
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return entered;
        }));
    }
    sub->removeEventListener(id);
    std::jthread releaser([&] {
        CHECK(eventually([&] {
            return endpoint->getState() == api::EndpointState::CLOSING;
        }));
        CHECK_FALSE(captured.load());
        CHECK_FALSE(sub->getStatistics().final);
        CHECK_FALSE(awaited.load());
        {
            std::lock_guard lock(mutex);
            release = true;
        }
        condition.notify_all();
    });
    runtime.closeAndAwaitTermination();
    awaited.store(true);
    releaser.join();
    host.join();
    CHECK(captured.load());
    CHECK(sub->getStatistics().final);
    CHECK(sub->getStatistics().inFlightEvents == 0);
}

TEST_CASE("late host tasks outlive runtime as pure inert identities without retaining policy or captures") {
    std::optional<api::CallbackTask> stale;
    std::weak_ptr<Policy> weakPolicy;
    std::weak_ptr<int> weakCapture;
    {
        api::ApiRuntime runtime(runtimeOptions());
        auto schema = quoteSchema();
        std::vector<std::u16string> symbols{u"IBM"};
        std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
        qd::connect_tests::PeerScript script;
        script.canonicalSymbols = true;
        qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
        auto endpoint = runtime.createEndpoint(endpointOptions());
        auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
        auto policy = std::make_shared<Policy>();
        weakPolicy = policy;
        std::mutex mutex;
        std::condition_variable condition;
        policy->action = [&](api::CallbackTask task) {
            {
                std::lock_guard lock(mutex);
                stale.emplace(std::move(task));
            }
            condition.notify_all();

            return api::CallbackPostResult::ACCEPTED;
        };
        auto capture = std::make_shared<int>(1);
        weakCapture = capture;
        sub->setEventExecutor(policy);
        sub->addEventListener([capture](auto) {
        });
        capture.reset();
        policy.reset();
        sub->addSymbols("IBM");
        RuntimeJoin join{runtime};
        endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
        {
            std::unique_lock lock(mutex);
            REQUIRE(condition.wait_for(lock, 5s, [&] {
                return stale.has_value();
            }));
        }
        runtime.closeAndAwaitTermination();
    }
    CHECK(weakPolicy.expired());
    CHECK(weakCapture.expired());
    REQUIRE(stale);
    CHECK(stale->run() == api::CallbackTaskRunResult::CANCELLED);
    api::ApiRuntime next(runtimeOptions());
    next.closeAndAwaitTermination();
    CHECK(stale->run() == api::CallbackTaskRunResult::CANCELLED);
}

TEST_CASE("actual serial owning batches resume once and resist concurrent repeated host runs") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = quoteSchema();
    std::vector<std::u16string> symbols{u"IBM"};
    std::vector<qd::connect_tests::Record> records;

    for (unsigned i = 1; i <= 8; ++i) {
        auto fields = quoteFields(schema);

        for (auto &field : fields)
            if (field.type == qd::core::SerialFieldType::TIME_SECONDS)
                field.raw = i;

        records.push_back({u"IBM", std::move(fields)});
    }

    qd::connect_tests::PeerScript script;
    script.canonicalSymbols = true;
    qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    std::vector<api::CallbackTask> queue;
    queue.reserve(2);
    bool entered = false, release = false;
    std::atomic<unsigned> active{0}, maximum{0}, calls{0}, posts{0};
    std::vector<std::int64_t> observed;
    observed.reserve(8);
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask task) {
        std::lock_guard lock(mutex);
        CHECK(queue.empty());
        queue.push_back(std::move(task));
        ++posts;
        condition.notify_all();

        return api::CallbackPostResult::ACCEPTED;
    };
    sub->setEventExecutor(policy);
    sub->addEventListener([&](std::span<const qd::events::Quote> values) {
        const auto inside = ++active;
        maximum.store(std::max(maximum.load(), inside));
        const auto ordinal = ++calls;
        std::unique_lock lock(mutex);

        if (ordinal == 1) {
            entered = true;
            condition.notify_all();
            CHECK(condition.wait_for(lock, 5s, [&] {
                return release;
            }));
        }

        for (const auto &event : values)
            observed.push_back(event.bidTimeMs);

        --active;
    });
    RuntimeJoin join{runtime};
    sub->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    std::optional<api::CallbackTask> first;
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return !queue.empty();
        }));
        first.emplace(std::move(queue.front()));
        queue.clear();
    }
    std::jthread host([&] {
        CHECK(first->run() == api::CallbackTaskRunResult::COMPLETED);
    });
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return entered;
        }));
    }
    std::jthread duplicate([&] {
        CHECK(first->run() == api::CallbackTaskRunResult::CANCELLED);
    });
    duplicate.join();
    CHECK(posts.load() == 1);
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    condition.notify_all();
    host.join();
    std::optional<api::CallbackTask> second;
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return !queue.empty();
        }));
        second.emplace(std::move(queue.front()));
        queue.clear();
    }
    CHECK(second->run() == api::CallbackTaskRunResult::COMPLETED);
    CHECK(first->run() == api::CallbackTaskRunResult::CANCELLED);
    REQUIRE(observed.size() == 8);

    for (std::size_t i = 0; i < observed.size(); ++i)
        CHECK(observed[i] == static_cast<std::int64_t>((i + 1) * 1000));

    CHECK(maximum.load() == 1);
    CHECK(calls.load() == 2);
    CHECK(sub->getStatistics().deliveredEvents.value == 8);
    runtime.closeAndAwaitTermination();
}

TEST_CASE("passive retirement cut is busy for a selected offer even without listeners or a custom slot") {
    detail::SubscriptionPort port;
    port.statistics = std::make_shared<qd::core::detail::DeliveryStatisticsState>();
    port.statistics->update([](auto &state) {
        state.admittedEvents.add(1);
        state.inFlightEvents = 1;
    });
    CHECK_FALSE(port.callbackPool);
    CHECK_FALSE(port.callbackSynchronousExecuting);
    const auto selected = detail::callbackRetirementSnapshot(port);
    CHECK_FALSE(selected.quiescent);
    CHECK(selected.retirementRevision == 0);
    port.statistics->update([](auto &state) {
        state.inFlightEvents = 0;
        state.discardedEvents.add(1);
    });
    CHECK(detail::callbackRetirementSnapshot(port).quiescent);
}

TEST_CASE("history qualification waits for custom posting and final capture retirement") {
    Harness h;
    detail::SubscriptionPort port;
    port.statistics = std::make_shared<qd::core::detail::DeliveryStatisticsState>();
    port.callbackPool = h.pool;
    port.callbackSlot = h.slot;
    const auto initial = detail::historyCallbackRetirement(port);
    REQUIRE(initial.quiescent);
    h.submit({}, [&] {
        const auto retiring = detail::historyCallbackRetirement(port);
        CHECK_FALSE(retiring.quiescent);
        CHECK(retiring.revision == initial.revision);
        std::lock_guard lock(port.mutex);
        const auto locked = detail::historyCallbackRetirementLocked(port);
        CHECK_FALSE(locked.quiescent);
        CHECK(locked.revision == initial.revision);
    });
    // No callback is active yet: a held host task must still prevent range completion publication.
    CHECK_FALSE(h.domain->fences[0].active.load());
    CHECK(port.statistics->snapshot().inFlightEvents == 0);
    CHECK_FALSE(detail::historyCallbackRetirement(port).quiescent);
    REQUIRE(h.task);
    CHECK(h.task->run() == api::CallbackTaskRunResult::COMPLETED);
    const auto retired = detail::historyCallbackRetirement(port);
    CHECK(retired.quiescent);
    CHECK(retired.revision == initial.revision + 1);
}

TEST_CASE("async event failure preserves SDK worker publication of subscription state observers") {
    api::ApiRuntime runtime(runtimeOptions());
    auto schema = quoteSchema();
    std::vector<std::u16string> symbols{u"IBM"};
    std::vector<qd::connect_tests::Record> records{{u"IBM", quoteFields(schema)}};
    qd::connect_tests::PeerScript script;
    script.canonicalSymbols = true;
    qd::connect_tests::Peer peer(schema, records, symbols, 8192, qd::connect_tests::PeerMode::HOLD, script);
    auto endpoint = runtime.createEndpoint(endpointOptions());
    auto sub = endpoint->getFeed()->createSubscription<qd::events::Quote>();
    std::mutex mutex;
    std::condition_variable condition;
    std::optional<api::CallbackTask> task;
    std::thread::id activeThread, failureThread;
    bool failed = false;
    auto policy = std::make_shared<Policy>();
    policy->action = [&](api::CallbackTask value) {
        {
            std::lock_guard lock(mutex);
            task.emplace(std::move(value));
        }
        condition.notify_all();

        return api::CallbackPostResult::ACCEPTED;
    };
    sub->setEventExecutor(policy);
    sub->addStateChangeListener([&](api::SubscriptionState, api::SubscriptionState current) {
        std::lock_guard lock(mutex);

        if (current == api::SubscriptionState::ACTIVE)
            activeThread = std::this_thread::get_id();

        if (current == api::SubscriptionState::FAILED) {
            failed = true;
            failureThread = std::this_thread::get_id();
        }

        condition.notify_all();
    });
    sub->addEventListener([](auto) {
        throw std::runtime_error("private listener failure");
    });
    RuntimeJoin join{runtime};
    sub->addSymbols("IBM");
    endpoint->connect("127.0.0.1:" + std::to_string(peer.port()));
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return task.has_value() && activeThread != std::thread::id{};
        }));
        CHECK(activeThread != std::this_thread::get_id());
    }
    CHECK(task->run() == api::CallbackTaskRunResult::COMPLETED);
    {
        std::unique_lock lock(mutex);
        REQUIRE(condition.wait_for(lock, 5s, [&] {
            return failed;
        }));
        CHECK(failureThread == activeThread);
        CHECK(failureThread != std::this_thread::get_id());
    }
    CHECK(sub->getDiagnostic().code == api::ErrorCode::CALLBACK_FAILURE);
    CHECK(sub->getStatistics().callbackFailedEvents.value == 1);
    CHECK(sub->getStatistics().inFlightEvents == 0);
    runtime.closeAndAwaitTermination();
}
