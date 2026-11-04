// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "collector_connection.hpp"
#include "connection_pending.hpp"
#ifndef _WIN32
#    include "transfer_file.hpp"
#endif
#include "connection_control.hpp"
#include <atomic>
#include <cstdlib>
#include <doctest/doctest.h>
#include <iostream>
#include <new>
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
