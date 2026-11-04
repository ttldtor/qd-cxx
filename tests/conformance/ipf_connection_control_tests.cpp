// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "collector_connection.hpp"
#include "connection_pending.hpp"
#ifndef _WIN32
#    include "transfer_file.hpp"
#endif
#include "connection_control.hpp"
#include "connection_wait.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
thread_local bool rejectAllocation = false;
thread_local std::size_t rejectedAllocations = 0;
thread_local bool trackAllocation = false;
thread_local std::size_t allocationCalls = 0;
thread_local std::size_t allocationBytes = 0;
thread_local std::size_t failAllocation = 0;
} // namespace

/** Instrument allocations on the test caller only.
 * @param bytes Requested bytes.
 * @return Ordinary storage; throws when this thread's failure gate is armed.
 */
void *operator new(std::size_t bytes) {
    if (trackAllocation) {
        ++allocationCalls;
        allocationBytes += bytes;

        if (failAllocation != 0 && allocationCalls == failAllocation) {
            throw std::bad_alloc{};
        }
    }

    if (rejectAllocation) {
        ++rejectedAllocations;
        throw std::bad_alloc{};
    }

    if (auto result = std::malloc(bytes ? bytes : 1)) {
        return result;
    }

    throw std::bad_alloc{};
}

/** @param bytes Array storage request.
 * @return Same instrumented allocation.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** @param memory Matching owned allocation. */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** @param memory Matching owned allocation.
 * @param bytes Original size, unused.
 */
void operator delete(void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

/** @param memory Matching array allocation. */
void operator delete[](void *memory) noexcept {
    std::free(memory);
}

/** @param memory Matching array allocation.
 * @param bytes Original size, unused.
 */
void operator delete[](void *memory, [[maybe_unused]] std::size_t bytes) noexcept {
    std::free(memory);
}

namespace i = org::ttldtor::qdcxx::ipf;
namespace c = i::connection_detail;

TEST_CASE("production connection control pre-admits listener slots before allocation") {
    auto gate = std::make_shared<c::Control>();
    i::IpfConnectionLimits limits;
    limits.listenerSlots = 0;
    auto port = std::make_shared<c::Port>(gate, limits);
    i::InstrumentProfileConnection::StateChangeListener callback = [](auto, auto) {
    };
    rejectAllocation = true;
    auto result = port->listen(std::move(callback));
    rejectAllocation = false;
    CHECK_FALSE(result);
    CHECK(rejectedAllocations == 0);
    port->close();
    port->retire();
}

TEST_CASE("production connection control close retirement and captures are allocation free outside locks") {
    auto gate = std::make_shared<c::Control>();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    std::atomic<unsigned> destroyed{0};

    struct Capture {
        std::weak_ptr<c::Port> port;
        std::atomic<unsigned> *destroyed;

        ~Capture() {
            if (auto value = port.lock()) {
                std::lock_guard lock(value->mutex);
                (void)value->observed;
            }

            destroyed->fetch_add(1);
        }
    };

    auto capture = std::make_shared<Capture>(Capture{port, &destroyed});
    const auto previous = destroyed.load();
    auto listener = port->listen([capture](auto, auto) {
    });
    REQUIRE(listener);
    capture.reset();
    rejectAllocation = true;
    port->close();
    port->retire();
    port.reset();
    rejectAllocation = false;
    CHECK(rejectedAllocations == 0);
    CHECK(destroyed.load() == previous + 1);
}

TEST_CASE("production control serializes concurrent observe register remove and request close") {
    auto gate = std::make_shared<c::Control>();
    i::IpfConnectionLimits limits;
    limits.listenerSlots = 8;
    auto port = std::make_shared<c::Port>(gate, limits);
    std::atomic<bool> begin{false};
    std::atomic<unsigned> callbacks{0};
    auto initial = port->listen([&](auto, auto) {
        ++callbacks;
        port->close();
    });
    REQUIRE(initial);
    std::thread observer([&] {
        while (!begin.load()) {
            std::this_thread::yield();
        }

        for (unsigned n = 0; n < 500; ++n) {
            std::lock_guard lock(port->mutex);
            CHECK(port->observed.attempts == 0);
        }
    });
    std::thread producer([&] {
        while (!begin.load()) {
            std::this_thread::yield();
        }

        for (unsigned n = 0; n < 500; ++n) {
            auto token = port->listen([](auto, auto) {
            });

            if (token) {
                (void)port->forget(*token);
            }
        }

        port->close();
    });
    {
        std::lock_guard lock(port->mutex);
        port->transition(i::IpfConnectionState::CONNECTING);
    }
    begin.store(true);
    port->dispatch();
    observer.join();
    producer.join();
    port->close();
    port->retire();
    CHECK(port->observed.retired);
    CHECK(port->observed.state == i::IpfConnectionState::CLOSED);
    CHECK(callbacks.load() <= 2);
    CHECK(port->observerLedger->used.load() == 0);
}

TEST_CASE("production listener reentry excludes removed callbacks and contains exceptions") {
    auto gate = std::make_shared<c::Control>();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    unsigned second = 0;
    std::uint64_t token = 0;
    auto first = port->listen([&](auto, auto) {
        (void)port->forget(token);
        auto added = port->listen([](auto, auto) {
        });
        (void)added;
        port->close();
        throw 1;
    });
    REQUIRE(first);
    auto removed = port->listen([&](auto, auto) {
        ++second;
    });
    REQUIRE(removed);
    token = *removed;
    {
        std::lock_guard lock(port->mutex);
        port->transition(i::IpfConnectionState::CONNECTING);
    }
    port->dispatch();
    port->retire();
    CHECK(second == 0);
    CHECK(port->observed.callbackFailures == 2);
}

TEST_CASE("collector connection domains isolate integer tags and equal value ownership refresh") {
    auto model = i::InstrumentProfileCollector::create();
    auto first = i::collector_detail::ConnectionCollectorAccess::reserve(*model);
    REQUIRE(first);
    auto second = i::collector_detail::ConnectionCollectorAccess::reserve(*model);
    REQUIRE(second);
    CHECK(*first != *second);
    i::InstrumentProfile a, b, c;
    REQUIRE(a.setType(u"STOCK"));
    REQUIRE(a.setSymbol(u"A"));
    REQUIRE(b.setType(u"STOCK"));
    REQUIRE(b.setSymbol(u"B"));
    REQUIRE(c.setType(u"STOCK"));
    REQUIRE(c.setSymbol(u"C"));
    const auto update = [&](const i::InstrumentProfile &profile, std::uint64_t domain) {
        return i::collector_detail::ConnectionCollectorAccess::update(*model, std::span(&profile, 1), domain, 1);
    };
    REQUIRE(update(a, *first));
    REQUIRE(update(b, *second));
    REQUIRE(model->updateInstrumentProfiles(std::span(&c, 1), 1));
    const std::array<std::uint64_t, 1> tag{1};
    auto removed = i::collector_detail::ConnectionCollectorAccess::remove(*model, tag, *first);
    REQUIRE(removed);
    CHECK(removed->updates == 1);
    auto snapshot = model->snapshot();
    REQUIRE(snapshot);
    REQUIRE((*snapshot)->profiles().size() == 2);
    CHECK((*snapshot)->profiles()[0]->profile.getSymbol() == u"B");
    CHECK((*snapshot)->profiles()[1]->profile.getSymbol() == u"C");
    REQUIRE(update(a, *first));
    unsigned callbacks = 0;
    auto listener = model->addUpdateListener([&](auto) {
        ++callbacks;
    });
    REQUIRE(listener);
    const auto previous = callbacks;
    auto equal = update(a, *second);
    REQUIRE(equal);
    CHECK(equal->updates == 0);
    CHECK(callbacks == previous);
    removed = i::collector_detail::ConnectionCollectorAccess::remove(*model, tag, *first);
    REQUIRE(removed);
    CHECK(removed->updates == 0);
    auto manual = model->updateInstrumentProfiles(std::span(&a, 1), 1);
    REQUIRE(manual);
    CHECK(manual->updates == 0);
    removed = i::collector_detail::ConnectionCollectorAccess::remove(*model, tag, *second);
    REQUIRE(removed);
    CHECK(removed->updates == 1);
    snapshot = model->snapshot();
    REQUIRE(snapshot);
    REQUIRE((*snapshot)->profiles().size() == 2);
    CHECK((*snapshot)->profiles()[0]->profile.getSymbol() == u"A");
    CHECK((*snapshot)->profiles()[1]->profile.getSymbol() == u"C");
    auto publicRemoved = model->removeGenerations(tag);
    REQUIRE(publicRemoved);
    CHECK(publicRemoved->updates == 2);
}

TEST_CASE("pending copy backing covers populated map string sentinel requests and exact refusal") {
    i::InstrumentProfile profile;
    REQUIRE(profile.setType(u"STOCK"));
    REQUIRE(profile.setSymbol(u"A"));

    for (unsigned n = 0; n < 254; ++n) {
        const std::u16string name{u'X', char16_t(u'0' + n / 100), char16_t(u'0' + (n / 10) % 10),
                                  char16_t(u'0' + n % 10)};
        REQUIRE(profile.setField(name, u"v"));
    }

    REQUIRE(profile.setDescription(std::u16string(4096, u'd')));
    const auto before = profile;
    const auto bytes = i::collector_detail::CollectorProfileAccess::copyBacking(profile);
    REQUIRE(bytes);
    std::vector<i::InstrumentProfile> values;
    values.reserve(2);
    std::size_t used = 0;
    allocationCalls = 0;
    allocationBytes = 0;
    failAllocation = 0;
    trackAllocation = true;
    const auto accepted = c::appendPendingProfile(values, profile, used, *bytes);
    trackAllocation = false;
    CHECK(accepted);
    CHECK(used == *bytes);
    CHECK(values.size() == 1);
    std::cout << "pending-copy bound=" << *bytes << " requested=" << allocationBytes << " calls=" << allocationCalls
              << '\n';
    CHECK(allocationBytes <= *bytes);
    CHECK(allocationBytes > profile.retainedUnits() * sizeof(char16_t));
    const auto calls = allocationCalls;
    REQUIRE(calls > 254);
    values.clear();
    used = 0;
    rejectAllocation = true;
    const auto refused = c::appendPendingProfile(values, profile, used, *bytes - 1);
    rejectAllocation = false;
    CHECK_FALSE(refused);
    CHECK(values.empty());
    CHECK(used == 0);
    CHECK(profile == before);
    REQUIRE(c::appendPendingProfile(values, profile, used, *bytes * 2));
    const auto prefixUsed = used;

    for (std::size_t failure = 1; failure <= calls; ++failure) {
        allocationCalls = 0;
        allocationBytes = 0;
        failAllocation = failure;
        bool threw = false;
        trackAllocation = true;
        try {
            (void)c::appendPendingProfile(values, profile, used, *bytes * 2);
        } catch (const std::bad_alloc &) {
            threw = true;
        }
        trackAllocation = false;
        failAllocation = 0;
        CHECK(threw);
        CHECK(values.size() == 1);
        CHECK(used == prefixUsed);
        CHECK(values.front() == before);
        CHECK(profile == before);
    }

    REQUIRE(c::appendPendingProfile(values, profile, used, *bytes * 2));
    CHECK(values.size() == 2);
    CHECK(used == *bytes * 2);
}

#ifndef _WIN32
TEST_CASE("native file timestamp preserves full signed64 milliseconds and negative floor remainders") {
    namespace d = i::detail;
    constexpr auto MINIMUM = std::numeric_limits<std::int64_t>::min();
    constexpr auto MAXIMUM = std::numeric_limits<std::int64_t>::max();
    const auto zero = d::transferFileModificationTime(0, 0);
    REQUIRE(zero);
    CHECK(zero->time_since_epoch().count() == 0);
    const auto negative = d::transferFileModificationTime(-1, 999999999);
    REQUIRE(negative);
    CHECK(negative->time_since_epoch().count() == -1);
    const auto minimum = d::transferFileModificationTime(MINIMUM / 1000 - 1, (1000 + MINIMUM % 1000) * 1000000);
    REQUIRE(minimum);
    CHECK(minimum->time_since_epoch().count() == MINIMUM);
    CHECK_FALSE(d::transferFileModificationTime(MINIMUM / 1000 - 1, (999 + MINIMUM % 1000) * 1000000));
    const auto maximum = d::transferFileModificationTime(MAXIMUM / 1000, (MAXIMUM % 1000) * 1000000 + 999999);
    REQUIRE(maximum);
    CHECK(maximum->time_since_epoch().count() == MAXIMUM);
    CHECK_FALSE(d::transferFileModificationTime(MAXIMUM / 1000, (MAXIMUM % 1000 + 1) * 1000000));
    CHECK_FALSE(d::transferFileModificationTime(MINIMUM, 0));
    CHECK_FALSE(d::transferFileModificationTime(MAXIMUM, 0));
    CHECK_FALSE(d::transferFileModificationTime(0, -1));
    CHECK_FALSE(d::transferFileModificationTime(0, 1000000000));
}
#endif

namespace {
/** Wait only for an internal admission barrier in finite tests.
 * @param predicate Atomic observation without SDK/user callbacks.
 * @return True before the two-second fixture bound.
 */
template <class Predicate> bool waitForAdmission(Predicate predicate) {
    const auto end = c::Clock::now() + std::chrono::seconds(2);

    while (!predicate() && c::Clock::now() < end) {
        std::this_thread::yield();
    }

    return predicate();
}
} // namespace

TEST_CASE("completion wait selects sticky first-snapshot outcomes without allocation or retained listener") {
    using S = i::IpfCompletionWaitStatus;
    auto gate = std::make_shared<c::Control>();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    std::stop_source stop;
    stop.request_stop();
    rejectAllocation = true;
    auto cancelled = c::awaitCompleted(port, c::Clock::time_point::min(), stop.get_token());
    auto expired = c::awaitCompleted(port, c::Clock::time_point::min(), {});
    port->close();
    auto closed = c::awaitCompleted(port, c::Clock::time_point::min(), stop.get_token());
    rejectAllocation = false;
    CHECK(cancelled == S::CANCELLED);
    CHECK(expired == S::TIMED_OUT);
    CHECK(closed == S::CLOSED);
    CHECK(port->observers.empty());
    CHECK(gate->completionWaiters.load() == 0);
    CHECK(port->observed.workUnits == 0);

    auto completed = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    completed->transition(i::IpfConnectionState::CONNECTING);
    completed->transition(i::IpfConnectionState::CONNECTED);
    completed->completeInitialSnapshot();
    completed->close();
    completed->retire();
    gate.reset();
    rejectAllocation = true;
    auto result = c::awaitCompleted(completed, c::Clock::time_point::min(), stop.get_token());
    rejectAllocation = false;
    CHECK(result == S::COMPLETED);
    CHECK(rejectedAllocations == 0);
}

TEST_CASE("completion wait refuses only blocking worker self-waits and preserves terminal observations") {
    auto gate = std::make_shared<c::Control>();
    gate->worker = std::this_thread::get_id();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    auto self = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(1), {});
    REQUIRE_FALSE(self);
    CHECK(self.error().transfer.stage == "connection-wait-self");
    CHECK(c::awaitCompleted(port, c::Clock::time_point::min(), {}) == i::IpfCompletionWaitStatus::TIMED_OUT);
    port->close();
    CHECK(c::awaitCompleted(port, c::Clock::time_point::max(), {}) == i::IpfCompletionWaitStatus::CLOSED);
    CHECK(gate->completionWaiters.load() == 0);
    CHECK_FALSE(port->observed.diagnostic);
}

TEST_CASE("completion wait pre-admits capacity and zero turns before any callback backing") {
    for (bool noTurns : {false, true}) {
        auto gate = std::make_shared<c::Control>();
        i::IpfConnectionLimits limits;
        limits.completionWaiters = noTurns ? 1 : 0;
        limits.completionWaitTurns = noTurns ? 0 : 1;
        auto port = std::make_shared<c::Port>(gate, limits);
        const auto oldRejected = rejectedAllocations;
        rejectAllocation = true;
        auto result = c::awaitCompleted(port, c::Clock::time_point::max(), {});
        rejectAllocation = false;
        REQUIRE_FALSE(result);
        CHECK(result.error().transfer.error ==
              (noTurns ? i::IpfTransferError::WORK_LIMIT : i::IpfTransferError::RESOURCE_LIMIT));
        CHECK(rejectedAllocations == oldRejected);
        CHECK(gate->completionWaiters.load() == 0);
        CHECK_FALSE(port->closed.load());
        CHECK_FALSE(port->observed.diagnostic);
    }
}

TEST_CASE("completion wait admits one turn and times out without allocations or connection cutoff") {
    auto gate = std::make_shared<c::Control>();
    i::IpfConnectionLimits limits;
    limits.completionWaiters = 1;
    limits.completionWaitTurns = 1;
    auto port = std::make_shared<c::Port>(gate, limits);
    const auto oldRejected = rejectedAllocations;
    rejectAllocation = true;
    auto result = c::awaitCompleted(port, c::Clock::now() + std::chrono::milliseconds(10), {});
    rejectAllocation = false;
    CHECK(result == i::IpfCompletionWaitStatus::TIMED_OUT);
    CHECK(rejectedAllocations == oldRejected);
    CHECK(gate->completionWaiters.load() == 0);
    CHECK_FALSE(port->closed.load());
}

TEST_CASE("completion wait cancellation uses a stable gate and leaves source state running") {
    auto gate = std::make_shared<c::Control>();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    std::stop_source stop;
    std::optional<std::expected<i::IpfCompletionWaitStatus, i::IpfConnectionDiagnostic>> result;
    std::atomic<std::size_t> allocations{0};
    std::jthread waiter([&] {
        rejectAllocation = true;
        result = c::awaitCompleted(port, c::Clock::time_point::max(), stop.get_token());
        allocations.store(rejectedAllocations);
        rejectAllocation = false;
    });
    CHECK(waitForAdmission([&] {
        return gate->completionWaiters.load() == 1;
    }));
    stop.request_stop();
    waiter.join();
    REQUIRE(result);
    CHECK(*result == i::IpfCompletionWaitStatus::CANCELLED);
    CHECK(allocations.load() == 0);
    CHECK(gate->completionWaiters.load() == 0);
    CHECK_FALSE(port->closed.load());
    CHECK(port->observed.state == i::IpfConnectionState::NOT_CONNECTED);
    CHECK(port->observed.workUnits == 0);
    CHECK(port->observers.empty());
}

TEST_CASE("completion wait slots include callback retirement and are reusable after return") {
    auto gate = std::make_shared<c::Control>();
    i::IpfConnectionLimits limits;
    limits.completionWaiters = 1;
    auto port = std::make_shared<c::Port>(gate, limits);
    std::stop_source stop;
    std::optional<std::expected<i::IpfCompletionWaitStatus, i::IpfConnectionDiagnostic>> first;
    std::jthread waiter([&] {
        first = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(2), stop.get_token());
    });
    CHECK(waitForAdmission([&] {
        return gate->completionWaiters.load() == 1;
    }));
    auto refused = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(1), {});
    CHECK_FALSE(refused);

    if (!refused) {
        CHECK(refused.error().transfer.stage == "connection-wait-capacity");
    }

    stop.request_stop();
    waiter.join();
    REQUIRE(first);
    CHECK(*first == i::IpfCompletionWaitStatus::CANCELLED);
    CHECK(gate->completionWaiters.load() == 0);
    CHECK(c::awaitCompleted(port, c::Clock::now() + std::chrono::milliseconds(1), {}) ==
          i::IpfCompletionWaitStatus::TIMED_OUT);
    CHECK(gate->completionWaiters.load() == 0);
}

TEST_CASE("completion publication wakes all admitted waiters before final callbacks or native retirement") {
    auto gate = std::make_shared<c::Control>();
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    port->transition(i::IpfConnectionState::CONNECTING);
    port->transition(i::IpfConnectionState::CONNECTED);
    std::array<std::optional<std::expected<i::IpfCompletionWaitStatus, i::IpfConnectionDiagnostic>>, 2> results;
    std::jthread first([&] {
        results[0] = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(2), {});
    });
    std::jthread second([&] {
        results[1] = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(2), {});
    });
    CHECK(waitForAdmission([&] {
        return gate->completionWaiters.load() == 2;
    }));
    port->completeInitialSnapshot();
    first.join();
    second.join();

    for (const auto &result : results) {
        REQUIRE(result);
        CHECK(*result == i::IpfCompletionWaitStatus::COMPLETED);
    }

    CHECK_FALSE(port->observed.retired);
    CHECK(gate->completionWaiters.load() == 0);
}

TEST_CASE("admitted completion wait retains control through owner disappearance and close") {
    auto gate = std::make_shared<c::Control>();
    std::weak_ptr<c::Control> identity = gate;
    auto port = std::make_shared<c::Port>(gate, i::IpfConnectionLimits{});
    std::optional<std::expected<i::IpfCompletionWaitStatus, i::IpfConnectionDiagnostic>> result;
    std::jthread waiter([&] {
        result = c::awaitCompleted(port, c::Clock::now() + std::chrono::seconds(2), {});
    });
    CHECK(waitForAdmission([&] {
        return gate->completionWaiters.load() == 1;
    }));
    gate.reset();
    CHECK_FALSE(identity.expired());
    port->close();
    waiter.join();
    REQUIRE(result);
    CHECK(*result == i::IpfCompletionWaitStatus::CLOSED);
    CHECK(identity.expired());
}

TEST_CASE("actual parser partial UTF8 and control rows return NEED_INPUT without charging an empty drain") {
    const std::array<std::string, 2> prefixes = {"#STOCK::=TYPE,SYMBOL,DESCRIPTION\nSTOCK,A,\xe2",
                                                 "#STOCK::=TYPE,SYMBOL\n##COMP"};

    for (const auto &prefix : prefixes) {
        i::IpfReadOptions options;
        i::InstrumentProfileParser parser(options);
        const auto input = std::as_bytes(std::span(prefix.data(), prefix.size()));
        std::size_t consumed = 0;
        bool needInput = false;

        for (unsigned turn = 0; turn < 8 && !needInput; ++turn) {
            auto work = options.turnWork;
            work.records = 1;
            auto step = parser.feed(input.subspan(consumed), work);
            CHECK(step.status != i::IpfReadStatus::FAILED);
            consumed += step.accepted;
            needInput = step.status == i::IpfReadStatus::NEED_INPUT;
        }

        REQUIRE(needInput);
        REQUIRE(consumed == input.size());
        const auto progress = parser.progress();
        auto work = options.turnWork;
        work.records = 1;
        const auto before = work;
        auto empty = parser.feed({}, work);
        CHECK(empty.status == i::IpfReadStatus::NEED_INPUT);
        CHECK(empty.accepted == 0);
        CHECK_FALSE(empty.item);
        CHECK(work.inputBytes == before.inputBytes);
        CHECK(work.decodedUnits == before.decodedUnits);
        CHECK(work.fields == before.fields);
        CHECK(work.records == before.records);
        CHECK(parser.progress().inputBytes == progress.inputBytes);
        CHECK(parser.progress().records == progress.records);
        CHECK_FALSE(parser.progress().completionSeen);
    }
}

TEST_CASE("in-flight observer removal retains its slot until the selected callback and capture retire") {
    for (unsigned iteration = 0; iteration != 32; ++iteration) {
        auto gate = std::make_shared<c::Control>();
        i::IpfConnectionLimits limits;
        limits.listenerSlots = 1;
        auto port = std::make_shared<c::Port>(gate, limits);
        std::mutex barrier;
        std::condition_variable changed;
        bool entered = false;
        bool release = false;
        std::atomic<unsigned> destroyed{0};

        struct Capture {
            std::weak_ptr<c::Port> port;      ///< Only a weak reentry target; no ownership cycle.
            std::atomic<unsigned> *destroyed; ///< Joined test participant counter.

            /** Prove last user-capture destruction can safely reenter the port mutex. */
            ~Capture() {
                if (auto value = port.lock()) {
                    std::lock_guard lock(value->mutex);
                    (void)value->observed;
                }

                destroyed->fetch_add(1);
            }
        };

        auto capture = std::make_shared<Capture>();
        capture->port = port;
        capture->destroyed = &destroyed;
        auto token = port->listen([&, capture](auto, auto) {
            std::unique_lock lock(barrier);
            entered = true;
            changed.notify_all();

            if (!changed.wait_for(lock, std::chrono::seconds(3), [&] {
                    return release;
                })) {
                throw std::runtime_error("owned callback release deadline");
            }

            throw std::runtime_error("contained in-flight failure");
        });
        REQUIRE(token);
        capture.reset();
        {
            std::lock_guard lock(port->mutex);
            port->transition(i::IpfConnectionState::CONNECTING);
        }
        std::jthread producer([&] {
            port->dispatch();
        });
        {
            std::unique_lock lock(barrier);
            REQUIRE(changed.wait_for(lock, std::chrono::seconds(3), [&] {
                return entered;
            }));
        }
        CHECK(port->forget(*token));
        CHECK(destroyed.load() == 0);
        CHECK_FALSE(port->listen([](auto, auto) {
        }));
        CHECK(port->observerLedger->used.load() == 1);
        {
            std::lock_guard lock(barrier);
            release = true;
        }
        changed.notify_all();
        producer.join();
        CHECK(destroyed.load() == 1);
        CHECK(port->observerLedger->used.load() == 0);
        CHECK(port->observed.callbackFailures == 1);
        auto replacement = port->listen([](auto, auto) {
        });
        REQUIRE(replacement);
        port->close();
        port->retire();
        CHECK(port->observerLedger->used.load() == 0);
    }
}
