// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/api/publication_queue.hpp"
#include "qd/api/publisher.hpp"
#include "qd/transport/tcp_runtime.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <doctest/doctest.h>
#include <limits>
#include <new>
#include <thread>

namespace {
namespace q = org::ttldtor::qdcxx;
namespace d = q::core::detail;
namespace p = q::api::detail;
using namespace std::chrono_literals;

/** Thread-local allocation gate never stalls the publisher worker or an unrelated test thread. */
struct AllocationGate {
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
};

thread_local AllocationGate *allocationGate = nullptr;
thread_local bool observeAllocations = false;
thread_local std::size_t allocationsBeforeFailure = std::numeric_limits<std::size_t>::max();
thread_local std::size_t allocationCount = 0;

/** Observe one production call without counting test assertions or worker allocations. */
struct AllocationScope {
    explicit AllocationScope(std::size_t successful) {
        allocationsBeforeFailure = successful;
        allocationCount = 0;
        observeAllocations = true;
    }

    ~AllocationScope() {
        observeAllocations = false;
    }
};

q::events::TimeAndSale sale() {
    q::events::TimeAndSale event;
    event.symbol = u"IBM";
    event.index = (std::int64_t{17} << 32) + 31;
    event.price.raw = 123456789;
    event.size.raw = -7654321;

    return event;
}

q::core::LocalSchema schema() {
    q::core::LocalRecord record{
        0, u"Custom", false, {{static_cast<std::int32_t>(q::core::SerialFieldType::COMPACT_INT)}}, {{u"Value", 0}}};
    auto result =
        q::core::LocalSchema::create(std::span(&record, 1), {1, 1, 1, 32, 64}, q::core::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

/** Allocate normally except inside an explicit per-thread failure or preparation gate. */
void *operator new(std::size_t bytes) {
    if (allocationGate) {
        auto *gate = std::exchange(allocationGate, nullptr);
        gate->entered.store(true, std::memory_order_release);

        while (!gate->release.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    if (observeAllocations) {
        if (allocationsBeforeFailure == 0) {
            throw std::bad_alloc{};
        }

        --allocationsBeforeFailure;
        ++allocationCount;
    }

    if (auto *pointer = std::malloc(bytes ? bytes : 1)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

void operator delete(void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

void operator delete[](void *pointer) noexcept {
    std::free(pointer);
}

void operator delete[](void *pointer, std::size_t) noexcept {
    std::free(pointer);
}

TEST_CASE("publication preparation fails atomically at every data allocation and restores admission") {
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    q::api::PublisherOptions options;
    options.mixed = true;
    options.streamBatchRecords = 8;
    options.pendingQuotes = 64;
    auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
    REQUIRE(endpoint);
    const auto publisher = (*endpoint)->getPublisher();
    const std::array events{sale(), sale(), sale()};
    REQUIRE(publisher.publish(events)); // Warm layout probes outside the measured call.
    std::expected<void, q::api::Diagnostic> result;
    std::size_t count = 0;
    {
        AllocationScope scope(std::numeric_limits<std::size_t>::max());
        result = publisher.publish(events);
        count = allocationCount;
    }
    REQUIRE(result);
    CHECK(count == 3); // One block backing, one shared block control, one queue node/control.

    for (std::size_t failure = 0; failure < count; ++failure) {
        {
            AllocationScope scope(failure);
            result = publisher.publish(events);
        }
        REQUIRE_FALSE(result);
        CHECK(result.error().code == q::api::ErrorCode::ALLOCATION_FAILURE);
        REQUIRE(publisher.publish(events));
    }

    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
}

TEST_CASE("close completes while a producer prepares and rejects its later commit without a native lifetime") {
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    q::api::PublisherOptions options;
    options.mixed = true;
    auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
    REQUIRE(endpoint);
    auto publisher = (*endpoint)->getPublisher();
    const auto event = sale();
    REQUIRE(publisher.publish(event));
    AllocationGate gate;
    std::expected<void, q::api::Diagnostic> result;
    std::jthread producer([&] {
        allocationGate = &gate;
        result = publisher.publish(event);
    });
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (!gate.entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    const bool entered = gate.entered.load(std::memory_order_acquire);
    std::atomic<bool> closed{false};
    std::jthread closer([&] {
        endpoint->reset();
        closed.store(true, std::memory_order_release);
    });

    while (!closed.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    const bool closedDuringPreparation = closed.load(std::memory_order_acquire);
    gate.release.store(true, std::memory_order_release);
    producer.join();
    closer.join();
    CHECK(entered);
    CHECK(closedDuringPreparation);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == q::api::ErrorCode::CLOSED);
    std::optional<q::transport::RuntimeBootstrap> retiredRuntime(std::move(*runtime));
    retiredRuntime.reset();
    const auto afterRuntime = publisher.publish(event);
    REQUIRE_FALSE(afterRuntime);
    CHECK(afterRuntime.error().code == q::api::ErrorCode::CLOSED);
}

TEST_CASE("whole INPUT credit survives FIFO detachment and physical aliases survive final publication discard") {
    d::RecordByteBudget physical(1024 * 1024);
    d::RecordByteBudget input(1024 * 1024);
    d::RecordByteBudget count(16);
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    auto binding = d::RecordBlockBinding::create(std::move(catalog), {}, physical);
    REQUIRE(binding);
    const auto bindingBytes = physical.usedBytes();
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, -42, {}, {}}};
    const std::array rows{d::RecordBlockInput{context, u"IBM", fields, {}},
                          d::RecordBlockInput{context, u"IBM", fields, {}}};
    auto block = d::SharedRecordBlock::copy(*binding, rows, physical);
    REQUIRE(block);
    const auto bytes = (*block)->accounting.backingAllocationBytes + (*block)->accounting.controlAllocationBytes;
    const auto nodeBytes =
        d::recordControlBytes<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                              std::shared_ptr<const d::BoundRecordBatch>{}, std::size_t{});
    auto node = std::allocate_shared<p::Publication>(d::RecordBlockAllocator<p::Publication>{physical.state()},
                                                     d::RecordByteReservation(input.state(), bytes + nodeBytes),
                                                     d::RecordByteReservation(count.state(), rows.size()),
                                                     std::move(*block), std::size_t{1});
    p::PublicationQueue queue;
    queue.commit(std::move(node));
    auto detached = queue.detach();
    CHECK_FALSE(queue.ready());
    CHECK(input.usedBytes() == bytes + nodeBytes);
    CHECK(count.usedBytes() == rows.size());
    auto retained = d::SharedRecordPayload::row(detached->block, 0);
    CHECK(retained->fields()[0].raw == -42);
    p::PublicationCursor cursor;
    cursor.publication = std::move(detached);
    cursor.staged(1);
    CHECK(input.usedBytes() == bytes + nodeBytes);
    cursor.collected();
    CHECK(input.usedBytes() == bytes + nodeBytes);
    CHECK(count.usedBytes() == rows.size());
    cursor.staged(1);
    CHECK(input.usedBytes() == bytes + nodeBytes);
    cursor.collected();
    CHECK(input.usedBytes() == 0);
    CHECK(count.usedBytes() == 0);
    CHECK(physical.usedBytes() == bindingBytes + bytes);
    binding->reset();
    CHECK(retained->symbol() == u"IBM");
    retained.reset();
    CHECK(physical.usedBytes() == 0);
}

TEST_CASE("long publication FIFO is reclaimed iteratively") {
    p::PublicationQueue queue;
    constexpr std::size_t COUNT = 100000;

    for (std::size_t index = 0; index < COUNT; ++index) {
        queue.commit(std::make_shared<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                                      std::shared_ptr<const d::BoundRecordBatch>{}, index));
    }

    auto chain = queue.detach();

    for (std::size_t index = 0; index < COUNT; ++index) {
        REQUIRE(chain);
        CHECK(chain->family == index);
        auto next = std::move(chain->next);
        chain.reset();
        chain = std::move(next);
    }

    CHECK_FALSE(chain);

    // Exercise the actual queue destructor on another long chain as well.
    for (std::size_t index = 0; index < COUNT; ++index) {
        queue.commit(std::make_shared<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                                      std::shared_ptr<const d::BoundRecordBatch>{}, index));
    }
}

TEST_CASE("atomic INPUT reservation distinguishes occupied capacity from a permanently oversized publication") {
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const std::array input{d::RecordBlockInput{context, u"IBM", fields, {}}};
    d::RecordByteBudget physical(1024 * 1024);
    auto copy = catalog;
    auto binding = d::RecordBlockBinding::create(std::move(copy), {}, physical);
    REQUIRE(binding);
    const auto measured = d::SharedRecordBlock::measure(**binding, input);
    REQUIRE(measured);
    const auto nodeBytes =
        d::recordControlBytes<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                              std::shared_ptr<const d::BoundRecordBatch>{}, std::size_t{});
    const std::vector<std::vector<std::u16string>> names{{u"Value"}};
    q::api::PublisherOptions options;
    options.catalog.emplace(catalog, names);
    options.pendingQuotes = 2;
    options.streamBytes.emplace(512, 1024);
    options.recordLimits.pendingBytes = (*binding)->retainedCapacityBytes() + measured->backingAllocationBytes +
                                        measured->controlAllocationBytes + nodeBytes;
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
    REQUIRE(endpoint);
    const auto publisher = (*endpoint)->getPublisher();
    const std::array rows{q::api::PublisherStreamRecordView{context, u"IBM", fields}};
    const std::array tooLarge{rows[0], rows[0]};
    const auto oversized = publisher.publishStreamRecords(tooLarge);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error().stage == "publisher-publication-capacity");
    AllocationGate gate;
    std::expected<void, q::api::Diagnostic> prepared;
    std::jthread producer([&] {
        allocationGate = &gate;
        prepared = publisher.publishStreamRecords(rows);
    });
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (!gate.entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    const bool entered = gate.entered.load(std::memory_order_acquire);
    const auto pending = (*endpoint)->getStatistics().pendingPublications;
    const auto occupied = publisher.publishStreamRecords(rows);
    gate.release.store(true, std::memory_order_release);
    producer.join();
    CHECK(entered);
    CHECK(pending == 0); // Reservations do not publish rows before commit.
    REQUIRE_FALSE(occupied);
    CHECK(occupied.error().stage == "publisher-pending-records");
    REQUIRE(prepared);

    for (;;) {
        const auto retried = publisher.publishStreamRecords(rows);

        if (retried) {
            break;
        }

        REQUIRE(retried.error().stage == "publisher-pending-records");
        REQUIRE(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }

    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
}

TEST_CASE("blocked detached STREAM head prevents later ready contracts from making the worker runnable") {
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    d::RecordByteBudget physical(1024 * 1024);
    auto bound = d::RecordBlockBinding::create(std::move(catalog), {}, physical);
    REQUIRE(bound);
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const std::array rows{d::RecordBlockInput{context, u"IBM", fields, {}},
                          d::RecordBlockInput{context, u"IBM", fields, {}}};
    auto stream = d::SharedRecordBlock::copy(*bound, rows, physical);
    auto later = d::SharedRecordBlock::copy(*bound, std::span(rows).first(1), physical);
    REQUIRE(stream);
    REQUIRE(later);
    constexpr std::size_t STREAM = 1;
    std::size_t trailing = 0;
    SUBCASE("ready TICKER behind STREAM") {
    }
    SUBCASE("ready HISTORY behind STREAM") {
        trailing = 2;
    }
    p::PublicationQueue mailbox;
    mailbox.commit(
        std::make_shared<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{}, *stream, STREAM));
    p::PublicationCursor cursor;
    cursor.publication = mailbox.detach();
    mailbox.commit(
        std::make_shared<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{}, *later, trailing));
    std::array credits{true, false, true};
    const auto ready = [&](std::size_t family) {
        return credits[family];
    };
    REQUIRE(mailbox.ready());
    CHECK(mailbox.firstFamily() == trailing);
    CHECK_FALSE(p::runnablePublicationHead(cursor, mailbox, ready));
    cursor.staged(1);
    cursor.collected();
    REQUIRE(cursor.publication);
    CHECK_FALSE(p::runnablePublicationHead(cursor, mailbox, ready));
    credits[STREAM] = true;
    CHECK(p::runnablePublicationHead(cursor, mailbox, ready));
    credits[STREAM] = false;
    cursor.staged(1);
    cursor.collected();
    CHECK_FALSE(cursor.publication);
    CHECK(p::runnablePublicationHead(cursor, mailbox, ready));
    cursor.publication = mailbox.detach();
    CHECK(p::runnablePublicationHead(cursor, mailbox, ready));
}

TEST_CASE("whole singleton selection retains its original owner without allocation while a partial block is copied") {
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    d::RecordByteBudget physical(1024 * 1024);
    auto bound = d::RecordBlockBinding::create(std::move(catalog), {}, physical);
    REQUIRE(bound);
    const std::u16string symbol(256, u'W');
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const std::array rows{d::RecordBlockInput{context, symbol, fields, {}},
                          d::RecordBlockInput{context, symbol, fields, {}},
                          d::RecordBlockInput{context, symbol, fields, {}}};
    auto single = d::SharedRecordBlock::copy(*bound, std::span(rows).first(1), physical,
                                             d::RecordPayloadResidency::SEPARATE_BINDING);
    auto multiple = d::SharedRecordBlock::copy(*bound, rows, physical, d::RecordPayloadResidency::SEPARATE_BINDING);
    REQUIRE(single);
    REQUIRE(multiple);
    REQUIRE((*single)->rows[0].residentBytes() > 1); // A valid row can exceed a configured soft window.
    const auto before = physical.usedBytes();
    std::expected<std::shared_ptr<const d::BoundRecordBatch>, d::RecordBlockError> selected;
    {
        AllocationScope noAllocation(0);
        selected = p::selectPublicationRange(*single, 0, 1, physical);
    }
    REQUIRE(selected);
    CHECK(selected->get() == single->get());
    CHECK((*selected)->rows[0].fields().data() == (*single)->rows[0].fields().data());
    CHECK(physical.usedBytes() == before);
    {
        AllocationScope noAllocation(0);
        selected = p::selectPublicationRange(*multiple, 1, 1, physical);
    }
    REQUIRE_FALSE(selected);
    CHECK(selected.error() == d::RecordBlockError::ALLOCATION_FAILURE);
    CHECK(physical.usedBytes() == before);
    selected = p::selectPublicationRange(*multiple, 1, 1, physical);
    REQUIRE(selected);
    CHECK(selected->get() != multiple->get());
    CHECK((*selected)->count == 1);
    CHECK((*selected)->rows[0].residentBytes() < (*multiple)->rows[0].residentBytes());
    CHECK((*selected)->rows[0].fields()[0].raw == 42);
}

TEST_CASE("row admission distinguishes a request larger than its quota from temporarily reserved rows") {
    bool catalogMode = false;
    SUBCASE("typed source") {
    }
    SUBCASE("catalog source") {
        catalogMode = true;
    }
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    const std::vector<std::vector<std::u16string>> names{{u"Value"}};
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const q::api::PublisherStreamRecordView row{context, u"IBM", fields};
    const std::array records{row, row, row};
    const std::array events{sale(), sale(), sale()};
    q::api::PublisherOptions options;

    if (catalogMode) {
        options.catalog.emplace(catalog, names);
    } else {
        options.mixed = true;
    }

    options.pendingQuotes = 2;
    options.streamBytes.emplace(512, 1024);
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
    REQUIRE(endpoint);
    const auto publisher = (*endpoint)->getPublisher();
    const auto attempt = [&](std::size_t count) {
        if (catalogMode) {
            return publisher.publishStreamRecords(std::span(records).first(count));
        }

        return publisher.publish(std::span(events).first(count));
    };
    const auto oversized = attempt(3);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error().code == q::api::ErrorCode::RESOURCE_LIMIT);
    CHECK(oversized.error().stage == "publisher-publication-capacity");
    AllocationGate gate;
    std::expected<void, q::api::Diagnostic> first;
    std::jthread producer([&] {
        allocationGate = &gate;
        first = attempt(1);
    });
    const auto deadline = std::chrono::steady_clock::now() + 3s;

    while (!gate.entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    const bool entered = gate.entered.load(std::memory_order_acquire);
    const auto occupied = attempt(2);
    auto invalidRecords = records;
    invalidRecords[1].fields = {};
    auto invalidEvents = events;
    invalidEvents[1].symbol.clear();
    const auto invalidAttempt = [&] {
        if (catalogMode) {
            return publisher.publishStreamRecords(std::span(invalidRecords).first(2));
        }

        return publisher.publish(std::span(invalidEvents).first(2));
    };
    const auto blockedInvalid = invalidAttempt();
    gate.release.store(true, std::memory_order_release);
    producer.join();
    CHECK(entered);
    REQUIRE(first);
    REQUIRE_FALSE(blockedInvalid);
    CHECK(blockedInvalid.error().stage == (catalogMode ? "publisher-pending-records" : "publisher-pending-quotes"));
    REQUIRE_FALSE(occupied);
    CHECK(occupied.error().code == q::api::ErrorCode::RESOURCE_LIMIT);
    CHECK(occupied.error().stage == (catalogMode ? "publisher-pending-records" : "publisher-pending-quotes"));

    while ((*endpoint)->getStatistics().families[1].ingressRecords.value != 1 &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    CHECK((*endpoint)->getStatistics().families[1].ingressRecords.value == 1);
    auto unblockedInvalid = invalidAttempt();

    while (!unblockedInvalid && unblockedInvalid.error().code == q::api::ErrorCode::RESOURCE_LIMIT &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        unblockedInvalid = invalidAttempt();
    }

    REQUIRE_FALSE(unblockedInvalid);
    CHECK(unblockedInvalid.error().code == q::api::ErrorCode::INVALID_ARGUMENT);
    CHECK(unblockedInvalid.error().stage == (catalogMode ? "publisher-record-shape" : "publisher-sale-shape"));
    CHECK((*endpoint)->getStatistics().families[1].ingressRecords.value == 1);
    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getStatistics().pendingPublications == 0);
}

TEST_CASE("remote publisher ignores local publication byte limits including SIZE_MAX") {
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    q::api::PublisherOptions options;
    options.source.emplace("127.0.0.1", 1, 1s);
    options.recordLimits.rowBytes = std::numeric_limits<std::size_t>::max();
    options.recordLimits.pendingBytes = std::numeric_limits<std::size_t>::max();
    auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
    INFO((endpoint ? std::string_view{} : endpoint.error().stage));
    REQUIRE(endpoint);
    const auto refused = (*endpoint)->getPublisher().publish(q::events::Quote{});
    REQUIRE_FALSE(refused);
    CHECK(refused.error().stage == "publisher-remote-source");
    (*endpoint)->closeAndAwaitTermination();
    CHECK((*endpoint)->getDiagnostic().code == q::api::ErrorCode::NONE);
}

TEST_CASE("one collector selection spans whole publications while a partial final block retains full INPUT credit") {
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    d::RecordByteBudget physical(1024 * 1024);
    auto bound = d::RecordBlockBinding::create(std::move(catalog), {}, physical);
    REQUIRE(bound);
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const std::array rows{
        d::RecordBlockInput{context, u"IBM", fields, {}}, d::RecordBlockInput{context, u"IBM", fields, {}},
        d::RecordBlockInput{context, u"IBM", fields, {}}, d::RecordBlockInput{context, u"IBM", fields, {}}};
    std::array<std::shared_ptr<const d::BoundRecordBatch>, 4> blocks;
    const std::array<std::size_t, 4> rowCounts{3, 2, 4, 1};
    std::array<std::size_t, 4> charges{};
    const auto nodeBytes =
        d::recordControlBytes<p::Publication>(d::RecordByteReservation{}, d::RecordByteReservation{},
                                              std::shared_ptr<const d::BoundRecordBatch>{}, std::size_t{});
    std::size_t total = 0;

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        auto copied = d::SharedRecordBlock::copy(*bound, std::span(rows).first(rowCounts[i]), physical);
        REQUIRE(copied);
        blocks[i] = std::move(*copied);
        charges[i] =
            nodeBytes + blocks[i]->accounting.backingAllocationBytes + blocks[i]->accounting.controlAllocationBytes;
        total += charges[i];
    }

    d::RecordByteBudget input(total);
    d::RecordByteBudget count(10);
    p::PublicationQueue mailbox;
    auto cursorOwner = std::make_unique<p::PublicationCursor>();
    auto &cursor = *cursorOwner;

    for (std::size_t i = 0; i < blocks.size(); ++i) {
        mailbox.commit(std::allocate_shared<p::Publication>(
            d::RecordBlockAllocator<p::Publication>{physical.state()},
            d::RecordByteReservation(input.state(), charges[i]), d::RecordByteReservation(count.state(), rowCounts[i]),
            blocks[i], i + 1 == blocks.size() ? std::size_t{0} : std::size_t{1}));

        if (i == 0) {
            cursor.publication = mailbox.detach();
        }
    }

    {
        const auto select = [](const p::Publication &publication, std::size_t first, std::size_t maximum, std::size_t) {
            return std::min(maximum, publication.block->count - first);
        };
        const auto append = [&](p::Publication &tail) {
            tail.next = mailbox.detach();
        };
        // Exercise the same ready-FIFO traversal called by prepareLocal, including a fresh mailbox detachment.
        const auto familyRows = p::selectReadyPublicationRows(cursor, 10, select, append);
        CHECK(familyRows == 9); // A ready TICKER cannot join the STREAM source event.
        CHECK_FALSE(mailbox.ready());
        const auto selected = p::selectReadyPublicationRows(cursor, 6, select, append);
        REQUIRE(selected == 6);
        cursor.staged(selected); // Two whole calls, then one row from the next four-row call.
        CHECK(input.usedBytes() == total);
        CHECK(count.usedBytes() == 10);
        SUBCASE("collector consumes the composed selection") {
            cursor.collected();
            REQUIRE(cursor.publication);
            CHECK(cursor.publication->block.get() == blocks[2].get());
            CHECK(cursor.row == 1);
            CHECK(input.usedBytes() == charges[2] + charges[3]);
            CHECK(count.usedBytes() == 5); // The four-row publication remains charged in full.
            cursor.staged(3);
            CHECK(input.usedBytes() == charges[2] + charges[3]);
            cursor.collected();
            REQUIRE(cursor.publication);
            CHECK(cursor.publication->family == 0); // The later TICKER keeps FIFO position.
            CHECK(input.usedBytes() == charges[3]);
            CHECK(count.usedBytes() == 1);
            cursor.staged(1);
            cursor.collected();
            CHECK_FALSE(cursor.publication);
        }
        SUBCASE("definite discard returns the complete staged cohort") {
            CHECK(input.usedBytes() == total);
        }
    }
    cursorOwner.reset();
    CHECK(input.usedBytes() == 0);
    CHECK(count.usedBytes() == 0);
    CHECK(physical.usedBytes() > 0); // Separate immutable owners still retain their physical storage.
}

TEST_CASE("byte oversize stays permanent when a valid bulk call also meets temporarily occupied row quota") {
    auto runtime = q::transport::RuntimeBootstrap::create();
    REQUIRE(runtime);
    auto catalog = schema();
    const auto context = catalog.recordContext(0);
    const std::vector<std::vector<std::u16string>> names{{u"Value"}};
    const std::array fields{q::core::FieldValueView{q::core::SerialFieldType::COMPACT_INT, 42, {}, {}}};
    const q::api::PublisherStreamRecordView row{context, u"IBM", fields};
    const std::array records{row, row};
    const std::array events{sale(), sale()};

    for (const bool catalogMode : {false, true}) {
        INFO("catalog mode ", catalogMode);
        q::api::PublisherOptions options;

        if (catalogMode) {
            options.catalog.emplace(catalog, names);
        } else {
            options.mixed = true;
        }

        options.pendingQuotes = 2;
        options.streamBytes.emplace(512, 1024);
        const auto attempt = [&](const q::api::Publisher &publisher, std::size_t count) {
            if (catalogMode) {
                return publisher.publishStreamRecords(std::span(records).first(count));
            }

            return publisher.publish(std::span(events).first(count));
        };
        // Find the public two-row admission boundary without rebuilding the private typed schema in the test.
        const auto fitsEmpty = [&](std::size_t bytes) {
            options.recordLimits.pendingBytes = bytes;
            auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);

            if (!endpoint) {
                CHECK((endpoint.error().stage == "publisher-publication-capacity" ||
                       endpoint.error().stage == "publisher-record-limits"));

                return false;
            }

            const auto result = attempt((*endpoint)->getPublisher(), 2);
            (*endpoint)->closeAndAwaitTermination();

            if (!result) {
                CHECK(result.error().stage == "publisher-publication-capacity");
            }

            return result.has_value();
        };
        std::size_t low = 1;
        auto high = options.recordLimits.pendingBytes;
        REQUIRE(fitsEmpty(high));

        while (low < high) {
            const auto middle = low + (high - low) / 2;

            if (fitsEmpty(middle)) {
                high = middle;
            } else {
                low = middle + 1;
            }
        }

        const auto exactTwoRowBytes = low;

        for (const std::size_t deficit : {std::size_t{0}, std::size_t{1}}) {
            INFO("bytes below exact boundary ", deficit);
            options.recordLimits.pendingBytes = exactTwoRowBytes - deficit;
            auto endpoint = q::api::PublisherEndpoint::create(*runtime, options);
            REQUIRE(endpoint);
            const auto publisher = (*endpoint)->getPublisher();
            AllocationGate gate;
            std::expected<void, q::api::Diagnostic> first;
            std::jthread producer([&] {
                allocationGate = &gate;
                first = attempt(publisher, 1);
            });
            const auto deadline = std::chrono::steady_clock::now() + 3s;

            while (!gate.entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            const bool entered = gate.entered.load(std::memory_order_acquire);
            const auto simultaneous = attempt(publisher, 2);
            gate.release.store(true, std::memory_order_release);
            producer.join();
            CHECK(entered);
            REQUIRE(first);
            REQUIRE_FALSE(simultaneous);
            CHECK(simultaneous.error().code == q::api::ErrorCode::RESOURCE_LIMIT);
            CHECK(simultaneous.error().stage == (deficit       ? "publisher-publication-capacity"
                                                 : catalogMode ? "publisher-pending-records"
                                                               : "publisher-pending-quotes"));

            while ((*endpoint)->getStatistics().families[1].ingressRecords.value != 1 &&
                   std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            CHECK((*endpoint)->getStatistics().families[1].ingressRecords.value == 1);
            (*endpoint)->closeAndAwaitTermination();
            CHECK((*endpoint)->getStatistics().pendingPublications == 0);
        }
    }
}
