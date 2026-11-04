// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#include "qd/ipf/instrument_profile_collector.hpp"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <array>
#include <atomic>
#include <barrier>
#include <condition_variable>
#include <cstdlib>
#include <exception>
#include <limits>
#include <mutex>
#include <new>
#include <thread>

// Fail only allocations on the armed test thread, never another producer or the test framework.
thread_local std::ptrdiff_t collectorFailAfter = -1;
thread_local std::size_t collectorAllocationBytes = 0;
thread_local std::size_t collectorAllocationSkip = 0;

/** Allocate backing with deterministic test-only failure.
 * @param size Requested allocation bytes.
 * @return Backing released by the matching global delete.
 */
void *operator new(std::size_t size) {
    if (size == collectorAllocationBytes && collectorAllocationBytes != 0) {
        if (collectorAllocationSkip == 0) {
            throw std::bad_alloc();
        }

        --collectorAllocationSkip;
    }

    if (collectorFailAfter == 0) {
        collectorFailAfter = -1;
        throw std::bad_alloc();
    }

    if (collectorFailAfter > 0) {
        --collectorFailAfter;
    }

    if (void *memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }

    throw std::bad_alloc();
}

/** Release matching test allocation.
 * @param memory Backing or null.
 */
void operator delete(void *memory) noexcept {
    std::free(memory);
}

/** Release matching sized test allocation.
 * @param memory Backing or null.
 * @param size Ignored allocation size.
 */
void operator delete(void *memory, std::size_t size) noexcept {
    (void)size;
    std::free(memory);
}

namespace {
using namespace org::ttldtor::qdcxx::ipf;

/** Build an independent fixture value.
 * @param symbol Exact symbol.
 * @param type Exact type.
 * @return Complete owned value.
 */
InstrumentProfile value(std::u16string_view symbol, std::u16string_view type = u"STOCK") {
    InstrumentProfile profile;
    REQUIRE(profile.setSymbol(symbol));
    REQUIRE(profile.setType(type));

    return profile;
}

/** Apply a borrowed singleton.
 * @param collector Destination repository.
 * @param profile Stable source.
 * @param generation Explicit generation.
 * @return Checked mutation outcome.
 */
auto put(InstrumentProfileCollector &collector, const InstrumentProfile &profile, std::uint64_t generation = 0) {
    return collector.updateInstrumentProfiles(std::span(&profile, 1), generation);
}
} // namespace

TEST_CASE("collector owns all standard and custom values and publishes only changes") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"A");

    for (const auto &field : standardFields()) {
        if (field.name == u"TYPE" || field.name == u"SYMBOL") {
            continue;
        }

        if (field.kind == FieldKind::TEXT) {
            REQUIRE(profile.setField(field.name, u"exact;\u03a9"));
        } else {
            REQUIRE(profile.setNumericField(field.name, field.kind == FieldKind::DATE ? 20000 : 12.5));
        }
    }

    REQUIRE(profile.setField(u"custom\u03a9", u"owned\U0001f642"));
    std::vector<std::shared_ptr<const ProfileBatch>> received;
    auto listener = collector->addUpdateListener([&](auto batch) {
        received.push_back(std::move(batch));
    });
    REQUIRE(listener);
    CHECK(received.empty());
    auto first = put(*collector, profile, 1);
    REQUIRE(first);
    CHECK(first->updates == 1);
    REQUIRE(received.size() == 1);
    CHECK(received[0]->profiles()[0]->profile == profile);
    const auto observed = collector->lastUpdateTime();
    auto unchanged = put(*collector, profile, 2);
    REQUIRE(unchanged);
    CHECK(unchanged->updates == 0);
    CHECK(unchanged->revision == first->revision);
    CHECK(collector->lastUpdateTime() == observed);
    CHECK(collector->removeGenerations(std::array<std::uint64_t, 1>{1})->updates == 0);
    CHECK(collector->removeGenerations(std::array<std::uint64_t, 1>{2})->updates == 1);
    REQUIRE(received.size() == 2);
    const auto &removed = received[1]->profiles()[0]->profile;
    CHECK(removed.getType() == u"REMOVED");
    CHECK(removed.getSymbol() == u"A");
    CHECK(removed.customFieldNames().empty());
    CHECK(removed.getDescription().empty());
    CHECK(collector->lastUpdateTime() >= observed + 1000);
    REQUIRE(profile.setDescription(u"source mutation"));
    CHECK(received[0]->profiles()[0]->profile.getDescription() == u"exact;\u03a9");
    collector->close();
    CHECK(received[0]->profiles()[0]->profile.getSymbol() == u"A");
    CHECK(collector->snapshot().error() == ProfileCollectorError::CLOSED);
}

TEST_CASE("collector absent removal and supplied removal preserve QD semantics") {
    auto collector = InstrumentProfileCollector::create();
    auto removed = value(u"A", u"REMOVED");
    REQUIRE(removed.setField(u"reason", u"expired"));
    CHECK(put(*collector, removed)->updates == 0);
    auto profile = value(u"A");
    REQUIRE(put(*collector, profile));
    std::shared_ptr<const ProfileBatch> initial;
    auto listener = collector->addUpdateListener([&](auto batch) {
        initial = std::move(batch);
    });
    REQUIRE(listener);
    REQUIRE(initial);
    CHECK(initial->profiles().size() == 1);
    REQUIRE(put(*collector, removed));
    REQUIRE(initial->profiles().size() == 1);
    CHECK(initial->profiles()[0]->profile == removed);
    CHECK(collector->snapshot().value()->profiles().empty());
    CHECK(put(*collector, removed)->updates == 0);
    CHECK(collector->removeUpdateListener(*listener));
    CHECK_FALSE(collector->removeUpdateListener(*listener));
}

TEST_CASE("collector reentry skips superseded revisions and contains callback throw") {
    auto collector = InstrumentProfileCollector::create();
    auto a = value(u"A");
    auto b = value(u"B");
    auto nestedInput = a;
    std::vector<std::u16string> order;
    bool nested = false;
    auto first = collector->addUpdateListener([&](auto batch) {
        order.emplace_back(batch->profiles()[0]->profile.getSymbol());

        if (!nested) {
            nested = true;
            REQUIRE(nestedInput.setDescription(u"new"));
            REQUIRE(put(*collector, nestedInput));
            REQUIRE(put(*collector, b));
        }
    });
    REQUIRE(first);
    std::vector<std::shared_ptr<const ProfileBatch>> second;
    REQUIRE(collector->addUpdateListener([&](auto batch) {
        second.push_back(std::move(batch));
    }));
    REQUIRE(collector->addUpdateListener([](auto) {
        throw 7;
    }));
    REQUIRE(put(*collector, a));
    CHECK(order == std::vector<std::u16string>{u"A", u"A", u"B"});
    REQUIRE(second.size() == 2);
    CHECK(second[0]->profiles()[0]->profile.getDescription() == u"new");
    CHECK(second[1]->profiles()[0]->profile.getSymbol() == u"B");
    CHECK(collector->callbackFailures() == 2);
}

TEST_CASE("collector callback self remove close delete and capture destruction reenter safely") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"A");
    std::uint64_t token = 0;
    std::size_t calls = 0;
    auto registered = collector->addUpdateListener([&](auto batch) {
        ++calls;
        CHECK(collector->removeUpdateListener(token));
        collector->close();
        CHECK(batch->profiles()[0]->profile.getSymbol() == u"A");
        collector.reset();
        CHECK(batch->profiles()[0]->profile.getSymbol() == u"A");
    });
    REQUIRE(registered);
    token = *registered;
    REQUIRE(put(*collector, profile));
    CHECK_FALSE(collector);
    CHECK(calls == 1);

    auto owner = InstrumentProfileCollector::create();

    struct Capture {
        InstrumentProfileCollector *collector;
        std::atomic<bool> *destroyed;

        ~Capture() {
            collector->close();
            destroyed->store(true);
        }
    };

    std::atomic<bool> destroyed = false;
    auto capture = std::make_shared<Capture>();
    capture->collector = owner.get();
    capture->destroyed = &destroyed;
    auto id = owner->addUpdateListener([capture](auto) {
    });
    REQUIRE(id);
    capture.reset();
    CHECK(owner->removeUpdateListener(*id));
    CHECK(destroyed.load());
}

TEST_CASE("collector finite retained leases recover only after external revisions release") {
    auto profile = value(u"A");
    ProfileCollectorLimits limits;
    limits.modelProfiles = 1;
    limits.liveRevisions = 2;
    limits.retainedUnits = 12;
    limits.liveBatches = 1;
    limits.batchReferences = 1;
    auto collector = InstrumentProfileCollector::create(limits);
    REQUIRE(put(*collector, profile));
    auto snapshot = collector->snapshot();
    REQUIRE(snapshot);
    CHECK(collector->snapshot().error() == ProfileCollectorError::LIMIT_EXCEEDED);
    REQUIRE(profile.setDescription(u"x"));
    CHECK(put(*collector, profile).error() == ProfileCollectorError::LIMIT_EXCEEDED);
    CHECK(snapshot.value()->profiles()[0]->profile.getDescription().empty());
    auto revision = snapshot.value()->profiles()[0];
    snapshot->reset();
    limits.retainedUnits = 14;
    auto exact = InstrumentProfileCollector::create(limits);
    auto original = value(u"A");
    REQUIRE(put(*exact, original));
    auto retained = exact->snapshot();
    REQUIRE(retained);
    REQUIRE(put(*exact, profile));
    CHECK(retained.value()->profiles()[0]->profile.getDescription().empty());
    REQUIRE(profile.setDescription(u"y"));
    CHECK(put(*exact, profile).error() == ProfileCollectorError::LIMIT_EXCEEDED);
    retained->reset();
    REQUIRE(put(*exact, profile));
    collector->close();
    CHECK(revision->profile.getSymbol() == u"A");
}

TEST_CASE("collector transaction preserves model and generations at exact and one-below caps") {
    auto a = value(u"A");
    auto b = value(u"B");
    const std::array input{a, b};
    ProfileCollectorLimits limits;
    limits.modelProfiles = 2;
    limits.stagingProfiles = 2;
    limits.liveRevisions = 2;
    limits.retainedUnits = 12;
    auto exact = InstrumentProfileCollector::create(limits);
    REQUIRE(exact->updateInstrumentProfiles(input, 7));
    CHECK(exact->snapshot().value()->profiles().size() == 2);

    for (int cap = 0; cap < 4; ++cap) {
        auto below = limits;

        if (cap == 0) {
            below.modelProfiles = 1;
        }

        if (cap == 1) {
            below.stagingProfiles = 1;
        }

        if (cap == 2) {
            below.liveRevisions = 1;
        }

        if (cap == 3) {
            below.retainedUnits = 11;
        }

        auto rejected = InstrumentProfileCollector::create(below);
        CHECK(rejected->updateInstrumentProfiles(input, 7).error() == ProfileCollectorError::LIMIT_EXCEEDED);
        CHECK(rejected->snapshot().value()->profiles().empty());
    }

    auto empty = InstrumentProfileCollector::create(ProfileCollectorLimits{0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(empty->updateInstrumentProfiles({})->updates == 0);
    CHECK(empty->removeGenerations({})->updates == 0);
    CHECK(empty->addUpdateListener({}).error() == ProfileCollectorError::INVALID_LISTENER);
    CHECK(empty
              ->addUpdateListener([](auto) {
              })
              .error() == ProfileCollectorError::LIMIT_EXCEEDED);
    limits.customFields = 0;
    auto custom = InstrumentProfileCollector::create(limits);
    REQUIRE(a.setField(u"x", u"y"));
    CHECK(put(*custom, a).error() == ProfileCollectorError::LIMIT_EXCEEDED);
}

TEST_CASE("collector finite dispatch cycle rejects unbounded reentrant publication") {
    ProfileCollectorLimits limits;
    limits.deliveryCalls = 2;
    auto collector = InstrumentProfileCollector::create(limits);
    auto profile = value(u"A");
    unsigned calls = 0;
    auto nestedInput = profile;
    REQUIRE(collector->addUpdateListener([&](auto) {
        ++calls;
        REQUIRE(nestedInput.setDescription(calls == 1 ? u"one" : u"two"));
        auto next = put(*collector, nestedInput);
        CHECK((calls == 1 ? next.has_value() : next.error() == ProfileCollectorError::LIMIT_EXCEEDED));
    }));
    REQUIRE(put(*collector, profile));
    CHECK(calls == 2);
    CHECK(collector->snapshot().value()->profiles()[0]->profile.getDescription() == u"one");
}

TEST_CASE("collector concurrent snapshot update removal close keeps active callback lease") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"A");
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    std::shared_ptr<const ProfileBatch> held;
    auto id = collector->addUpdateListener([&](auto batch) {
        std::unique_lock lock(mutex);
        held = std::move(batch);
        entered = true;
        changed.notify_one();
        changed.wait(lock, [&] {
            return release;
        });
    });
    REQUIRE(id);
    std::atomic<bool> updated = false;
    std::thread producer([&] {
        updated = put(*collector, profile).has_value();
    });
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] {
            return entered;
        });
    }

    auto snapshot = collector->snapshot();
    REQUIRE(snapshot);
    CHECK(snapshot.value()->profiles()[0]->profile == profile);
    CHECK(collector->removeUpdateListener(*id));
    collector->close();
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    changed.notify_one();
    producer.join();
    CHECK(updated.load());
    CHECK(held->profiles()[0]->profile == profile);
    CHECK(snapshot.value()->profiles()[0]->profile == profile);
}

TEST_CASE("collector concurrent producers and owning observations are serialized") {
    auto collector = InstrumentProfileCollector::create();
    std::atomic<bool> okay = true;
    std::barrier start(3);
    auto writer = [&](std::u16string_view symbol) {
        auto profile = value(symbol);
        start.arrive_and_wait();

        for (unsigned i = 0; i < 50; ++i) {
            if (!profile.setNumericField(u"SIC", i) || !put(*collector, profile)) {
                okay = false;
            }
        }
    };
    std::thread left(writer, u"A");
    std::thread right(writer, u"B");
    start.arrive_and_wait();

    for (unsigned i = 0; i < 50; ++i) {
        auto snapshot = collector->snapshot();

        if (!snapshot || snapshot.value()->profiles().size() > 2) {
            okay = false;
        }
    }

    left.join();
    right.join();
    CHECK(okay.load());
    CHECK(collector->snapshot().value()->profiles().size() == 2);
}

TEST_CASE("collector batch reference listener and work exact boundaries are actual operations") {
    auto a = value(u"A");
    ProfileCollectorLimits limits;
    limits.listeners = 1;
    limits.deliveryCalls = 1;
    limits.batchReferences = 1;
    limits.liveBatches = 1;
    auto exact = InstrumentProfileCollector::create(limits);
    REQUIRE(put(*exact, a));
    std::shared_ptr<const ProfileBatch> retained;
    auto id = exact->addUpdateListener([&](auto batch) {
        retained = std::move(batch);
    });
    REQUIRE(id);
    CHECK(exact
              ->addUpdateListener([](auto) {
              })
              .error() == ProfileCollectorError::LIMIT_EXCEEDED);
    REQUIRE(a.setDescription(u"changed"));
    CHECK(put(*exact, a).error() == ProfileCollectorError::LIMIT_EXCEEDED);
    CHECK(retained->profiles()[0]->profile.getDescription().empty());
    retained.reset();
    REQUIRE(put(*exact, a));

    limits.batchReferences = 0;
    auto below = InstrumentProfileCollector::create(limits);
    REQUIRE(put(*below, a));
    CHECK(below
              ->addUpdateListener([](auto) {
              })
              .error() == ProfileCollectorError::LIMIT_EXCEEDED);
    REQUIRE(below->removeGenerations(std::array<std::uint64_t, 1>{1}));
    CHECK(below
              ->addUpdateListener([](auto) {
              })
              .error() == ProfileCollectorError::LIMIT_EXCEEDED);

    // One empty-model singleton: count=1, retained units=6, symbol units=1.
    limits.workUnits = 8;
    auto workExact = InstrumentProfileCollector::create(limits);
    auto original = value(u"A");
    REQUIRE(put(*workExact, original));
    limits.workUnits = 7;
    auto workBelow = InstrumentProfileCollector::create(limits);
    CHECK(put(*workBelow, original).error() == ProfileCollectorError::LIMIT_EXCEEDED);
    CHECK(workBelow->snapshot().value()->profiles().empty());
}

TEST_CASE("collector allocation failure preserves source model generation and publication") {
    auto original = value(u"A");
    auto changed = value(u"B");
    REQUIRE(changed.setField(u"long-custom-key-with-owned-storage", u"long-custom-value-with-owned-storage"));
    const std::array input{original, changed};
    unsigned failures = 0;
    bool completed = false;

    for (std::ptrdiff_t point = 0; point < 128; ++point) {
        auto collector = InstrumentProfileCollector::create();
        unsigned calls = 0;
        REQUIRE(collector->addUpdateListener([&](auto) {
            ++calls;
        }));
        REQUIRE(put(*collector, original, 1));
        calls = 0;
        bool failed = false;
        collectorFailAfter = point;

        try {
            const auto result = collector->updateInstrumentProfiles(input, 2);

            if (!result) {
                collectorFailAfter = -1;
                FAIL("unexpected admission failure");
            }
        } catch (const std::bad_alloc &) {
            failed = true;
        }

        collectorFailAfter = -1;
        CHECK(input[0] == original);
        CHECK(input[1] == changed);

        if (failed) {
            ++failures;
            CHECK(calls == 0);
            auto snapshot = collector->snapshot();
            REQUIRE(snapshot);
            REQUIRE(snapshot.value()->profiles().size() == 1);
            CHECK(snapshot.value()->profiles()[0]->profile == original);
            CHECK(collector->removeGenerations(std::array<std::uint64_t, 1>{2})->updates == 0);
            CHECK(collector->removeGenerations(std::array<std::uint64_t, 1>{1})->updates == 1);
        } else {
            CHECK(calls == 1);
            CHECK(collector->snapshot().value()->profiles().size() == 2);
            completed = true;
            break;
        }
    }

    CHECK(completed);
    CHECK(failures > 10);
}

TEST_CASE("collector queued listener removal cuts off starts while concurrent update returns") {
    auto collector = InstrumentProfileCollector::create();
    auto firstValue = value(u"A");
    auto laterValue = firstValue;
    REQUIRE(laterValue.setDescription(u"later"));
    std::mutex mutex;
    std::condition_variable signal;
    bool entered = false;
    bool release = false;
    std::atomic<unsigned> secondCalls = 0;
    unsigned firstCalls = 0;
    REQUIRE(collector->addUpdateListener([&](auto batch) {
        if (++firstCalls == 1) {
            std::unique_lock lock(mutex);
            entered = true;
            signal.notify_one();
            signal.wait(lock, [&] {
                return release;
            });
            CHECK(batch->profiles()[0]->profile.getDescription().empty());
        }
    }));
    auto second = collector->addUpdateListener([&](auto) {
        ++secondCalls;
    });
    REQUIRE(second);
    std::thread producer([&] {
        (void)put(*collector, firstValue);
    });
    {
        std::unique_lock lock(mutex);
        signal.wait(lock, [&] {
            return entered;
        });
    }

    REQUIRE(put(*collector, laterValue));
    CHECK(collector->snapshot().value()->profiles()[0]->profile == laterValue);
    CHECK(collector->removeUpdateListener(*second));
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    signal.notify_one();
    producer.join();
    CHECK(firstCalls == 2);
    CHECK(secondCalls.load() == 0);
}

TEST_CASE("collector empty symbol NaN and signed zero preserve value identity") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"");
    REQUIRE(profile.setNumericField(u"MULTIPLIER", std::numeric_limits<double>::quiet_NaN()));
    CHECK(put(*collector, profile)->updates == 1);
    CHECK(put(*collector, profile)->updates == 0);
    auto observed = collector->snapshot();
    REQUIRE(observed);
    CHECK(put(*collector, observed.value()->profiles()[0]->profile)->updates == 0);
    REQUIRE(profile.setNumericField(u"MULTIPLIER", 0.0));
    CHECK(put(*collector, profile)->updates == 1);
    REQUIRE(profile.setNumericField(u"MULTIPLIER", -0.0));
    CHECK(put(*collector, profile)->updates == 1);
    CHECK(collector->snapshot().value()->profiles()[0]->profile.getSymbol().empty());
}

TEST_CASE("collector initial callback deletion leaves registration operation stable") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"A");
    REQUIRE(put(*collector, profile));
    std::shared_ptr<const ProfileBatch> held;
    auto token = collector->addUpdateListener([&](auto batch) {
        held = std::move(batch);
        collector.reset();
        CHECK(held->profiles()[0]->profile == profile);
    });
    CHECK(token.has_value());
    CHECK_FALSE(collector);
    CHECK(held->profiles()[0]->profile == profile);
}

TEST_CASE("collector pre-growth snapshot and initial batch reject before vector allocation") {
    for (bool batchCap : {false, true}) {
        ProfileCollectorLimits limits;
        limits.batchReferences = batchCap ? 1000 : 0;
        limits.liveBatches = batchCap ? 0 : 1000;
        auto collector = InstrumentProfileCollector::create(limits);
        std::vector<InstrumentProfile> input;

        for (char16_t unit = u'A'; unit < u'A' + 37; ++unit) {
            input.push_back(value(std::u16string(2, unit)));
        }

        REQUIRE(collector->updateInstrumentProfiles(input));

        for (bool initialListener : {false, true}) {
            bool allocated = false;
            bool rejected = false;
            collectorAllocationBytes = 37 * sizeof(std::shared_ptr<const ProfileRevision>);
            collectorAllocationSkip = 0;
            try {
                if (initialListener) {
                    auto result = collector->addUpdateListener([](auto) {
                    });
                    rejected = !result && result.error() == ProfileCollectorError::LIMIT_EXCEEDED;
                } else {
                    auto result = collector->snapshot();
                    rejected = !result && result.error() == ProfileCollectorError::LIMIT_EXCEEDED;
                }
            } catch (const std::bad_alloc &) {
                allocated = true;
            }
            collectorAllocationBytes = 0;
            CHECK_FALSE(allocated);
            CHECK(rejected);
        }
    }
}

TEST_CASE("collector pre-growth update batch rejects before third reference-vector allocation") {
    ProfileCollectorLimits limits;
    limits.batchReferences = 1;
    auto collector = InstrumentProfileCollector::create(limits);
    unsigned calls = 0;
    REQUIRE(collector->addUpdateListener([&](auto) {
        ++calls;
    }));
    auto original = value(u"A");
    REQUIRE(put(*collector, original));
    auto retained = collector->snapshot();
    REQUIRE(retained);
    auto changed = original;
    REQUIRE(changed.setDescription(u"new"));
    bool allocated = false;
    bool rejected = false;
    collectorAllocationBytes = sizeof(std::shared_ptr<const ProfileRevision>);
    // The two bounded staging vectors are legitimate; a third vector would be batch backing.
    collectorAllocationSkip = 2;
    try {
        auto result = put(*collector, changed);
        rejected = !result && result.error() == ProfileCollectorError::LIMIT_EXCEEDED;
    } catch (const std::bad_alloc &) {
        allocated = true;
    }
    collectorAllocationBytes = 0;
    CHECK_FALSE(allocated);
    CHECK(rejected);
    CHECK(calls == 1);
    CHECK(retained.value()->profiles()[0]->profile == original);
}

TEST_CASE("collector OOM armed explicit close and destructor allocate nothing") {
    auto explicitClose = InstrumentProfileCollector::create();
    auto profile = value(u"A");
    REQUIRE(put(*explicitClose, profile));
    bool threw = false;
    collectorFailAfter = 0;
    try {
        explicitClose->close();
    } catch (const std::bad_alloc &) {
        threw = true;
    }
    collectorFailAfter = -1;
    CHECK_FALSE(threw);
    explicitClose.reset();

    auto destruction = InstrumentProfileCollector::create();
    REQUIRE(put(*destruction, profile));
    bool captureDestroyed = false;

    struct Capture {
        bool *destroyed;
        InstrumentProfileCollector *collector;

        ~Capture() {
            collector->close();
            *destroyed = true;
        }
    };

    auto capture = std::make_shared<Capture>();
    capture->destroyed = &captureDestroyed;
    capture->collector = destruction.get();
    REQUIRE(destruction->addUpdateListener([capture](auto) {
    }));
    capture.reset();
    // Keep RED deterministic/headless: an allocating noexcept destructor exits this test process.
    const auto previous = std::set_terminate([] {
        std::_Exit(86);
    });
    collectorFailAfter = 0;
    destruction.reset();
    collectorFailAfter = -1;
    std::set_terminate(previous);
    CHECK(captureDestroyed);
}

TEST_CASE("collector OOM callback deletion drains selected operation without allocation") {
    auto collector = InstrumentProfileCollector::create();
    auto profile = value(u"A");
    std::shared_ptr<const ProfileBatch> retained;
    REQUIRE(collector->addUpdateListener([&](auto batch) {
        retained = std::move(batch);
        collectorFailAfter = 0;
        collector.reset();
        collectorFailAfter = -1;
    }));
    const auto previous = std::set_terminate([] {
        std::_Exit(86);
    });
    auto result = put(*collector, profile);
    collectorFailAfter = -1;
    std::set_terminate(previous);
    REQUIRE(result);
    CHECK_FALSE(collector);
    CHECK(retained->profiles()[0]->profile == profile);
}
