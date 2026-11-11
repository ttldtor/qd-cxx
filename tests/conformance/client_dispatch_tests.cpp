// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/client/stream_dispatcher.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace e = org::ttldtor::qdcxx::encoding;
namespace s = org::ttldtor::qdcxx::collectors;
namespace d = org::ttldtor::qdcxx::client;
namespace q = org::ttldtor::qdcxx::events;
using Error = d::ClientError;
bool failAllocations = false;         ///< Used only by single-threaded, explicit production fault calls.
std::size_t allocationsRemaining = 0; ///< Successful ordinary allocation requests before fault.

/** Keep framework/caller allocations outside narrow production failure observations. */
struct AllocationFault {
    /** Enable ordinary allocation failure for one production call.
     * @param count Successful requests permitted first.
     */
    explicit AllocationFault(std::size_t count) {
        allocationsRemaining = count;
        failAllocations = true;
    }

    /** Restore ordinary allocation before assertions/callback observations. */
    ~AllocationFault() {
        failAllocations = false;
    }
};

/** Explicit independent owner turn, not production defaults.
 * @return Finite owner/nested credits.
 */
d::DispatchWork turn() {
    return {10000,
            10000,
            10000,
            1000000,
            1000000,
            1000000,
            100000000,
            {10000, 1000000, 1000000},
            {10000, 1000000, 1000000, 100000000}};
}

/** Explicit producer-local turn.
 * @return Finite submission/input/search credits.
 */
d::SubmitWork submitTurn() {
    return {10000, 1000000, 1000000};
}

/** Explicit live-driver turn, outside dispatcher ownership.
 * @return Finite protocol work.
 */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Existing desired-state limits for this synthetic shape.
 * @return Finite creation/command/peak bounds.
 */
c::SubscriptionLimits desiredLimits() {
    return {16, 32, 128, 2048, 64, 4096, 32};
}

/** Existing collector limits, with no implicit lifetime feed quota.
 * @return Finite whole-event/item/backing capacities.
 */
s::StreamLimits collectorLimits() {
    return {16, 128, 2 * 1024 * 1024, 4 * 1024 * 1024};
}

/** Explicit fixed port and detached multi-record batch limits.
 * @return Small finite test capacities.
 */
d::ClientLimits limits() {
    return {4, 8, 8, 128, 2, 64, 128, 512, 1024, 16384, 4, 131072, 1024 * 1024};
}

/** Complete actual live admission profile.
 * @return Finite ordinary binary-field limits, without runtime/socket dependency.
 */
p::LiveLimits liveLimits() {
    p::LiveLimits result{};
    result.frame = {8192, 8201};
    result.protocol = {128, 64, {8192, 1024}, 16384};
    result.descriptions = {8192, 32, 32, 256, {8192, 1024}, 8192};
    result.context = {{16, 128, 256, 128, 16384}, 32, {512, 1024, 16384}};
    result.data = {8192, 64,   512,   64,    512,   8192, {8192, 128, 128}, {8192, 1024}, 8192, 8192, 8192,
                   8192, 8192, 16384, 16384, 16384, 64};
    result.residentBytes = 4 * 1024 * 1024;
    result.owners = 128;
    result.snapshotBytes = 65536;
    result.scratchBytes = 65536;
    result.eventBytes = 65536;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::ZERO_FLAGS;
    result.historySnapshot = false;

    return result;
}

/** Fresh explicit indexed synthetic records; the separate standard Quote case is never reduced.
 * @return Validated immutable schema.
 */
c::LocalSchema schema() {
    const c::LocalRecord records[]{{0,
                                    u"Oracle#ONE",
                                    true,
                                    {{8}, {8}, {8}},
                                    {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"Value", 2}},
                                    c::IndexedTimePairInt32{0, 1}},
                                   {1,
                                    u"Oracle#TWO",
                                    true,
                                    {{8}, {8}, {8}},
                                    {{u"TimeHigh", 0}, {u"TimeLow", 1}, {u"Value", 2}},
                                    c::IndexedTimePairInt32{0, 1}}};
    auto result = c::LocalSchema::create(records, {2, 6, 6, 32, 256}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Append canonical compact-long bytes.
 * @param bytes Test-owned payload.
 * @param value Raw signed value.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto result = e::writeCompactLong(value, buffer);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append canonical exact UTFString bytes.
 * @param bytes Test-owned payload.
 * @param value Borrowed non-null exact text.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 8192> buffer{};
    const auto result = e::writeUtfString(value, buffer, {8192, 1024});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap a complete selected frame.
 * @param type Exact message identity.
 * @param payload Whole message payload.
 * @return Owned framed bytes.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> bytes;
    number(bytes, static_cast<std::int64_t>(body.size()));
    bytes.insert(bytes.end(), body.begin(), body.end());

    return bytes;
}

/** Compose a modern source-backed readiness descriptor.
 * @return Complete peer descriptor advertising STREAM_DATA and STREAM add/remove readiness.
 */
std::vector<std::byte> ready() {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(payload, 0);
    number(payload, 1);
    number(payload, 15);
    text(payload, u"STREAM_DATA");
    number(payload, 0);
    number(payload, 2);
    number(payload, 16);
    text(payload, u"STREAM_ADD_SUBSCRIPTION");
    number(payload, 0);
    number(payload, 17);
    text(payload, u"STREAM_REMOVE_SUBSCRIPTION");
    number(payload, 0);

    return frame(1, payload);
}

/** Build one whole synthetic peer description.
 * @param id Exact sparse peer identity.
 * @param local Selected local record name.
 * @return Canonical description envelope.
 */
std::vector<std::byte> description(std::int32_t id, std::int32_t local) {
    std::vector<std::byte> payload;
    number(payload, id);
    text(payload, local == 0 ? u"Oracle#ONE" : u"Oracle#TWO");
    number(payload, 3);

    for (const auto name : {u"TimeHigh", u"TimeLow", u"Value"}) {
        text(payload, name);
        number(payload, 8);
    }

    return frame(2, payload);
}

/** One exact synthetic input tuple. */
struct Input {
    std::int32_t record;
    std::u16string_view symbol;
    std::int64_t time;
    std::int32_t value;
};

/** Encode source tuples with real codecs rather than constructing fabricated event views.
 * @param inputs Whole ordered source input.
 * @return Complete canonical STREAM_DATA frame.
 */
std::vector<std::byte> data(std::span<const Input> inputs) {
    std::vector<std::byte> payload;

    for (const auto &input : inputs) {
        payload.push_back(std::byte{0xfd});
        number(payload, static_cast<std::int64_t>(input.symbol.size()));

        for (const auto unit : input.symbol) {
            std::array<std::byte, 4> bytes{};
            const auto written = e::writeUtfChar(unit, bytes).written;
            payload.insert(payload.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(written));
        }

        number(payload, input.record == 0 ? 41 : 7);
        const auto bits = std::bit_cast<std::uint64_t>(input.time);
        number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
        number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
        number(payload, input.value);
    }

    return frame(15, payload);
}

/** Admit one whole frame through the real external protocol session.
 * @param session Actual serialized driver owner, outside dispatcher.
 * @param bytes Complete source frame.
 * @return Immutable admitted owner.
 */
p::LiveEvent admitted(p::LiveSession &session, std::span<const std::byte> bytes) {
    auto work = liveTurn();
    auto result = session.feed(bytes, work);
    INFO("live error=", static_cast<int>(session.diagnostic().error), " stage=", session.diagnostic().stage);
    REQUIRE(result.status == p::LiveStatus::EVENT);
    REQUIRE(result.event);
    REQUIRE(result.accepted == bytes.size());

    return std::move(*result.event);
}

/** Prepare a real session using complete supplied outgoing declarations.
 * @param source Immutable local provenance, preserved through an owned copy.
 * @param declarations Explicit complete outgoing record layouts.
 * @return Fresh negotiated session, without synthetic descriptions when caller supplies a real fixture.
 */
p::LiveSession session(const c::LocalSchema &source, std::span<const p::OutgoingRecord> declarations) {
    auto copied = source;
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(source, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copied), std::move(*writer), liveLimits(), work);
    REQUIRE(result);
    (void)admitted(*result, ready());

    return std::move(*result);
}

/** Prepare complete synthetic layouts and bindings.
 * @param source Immutable schema.
 * @return Ready real session with both peer descriptions.
 */
p::LiveSession session(const c::LocalSchema &source) {
    const p::OutgoingRecord layouts[]{
        {41, 0, u"Oracle#ONE", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}},
        {7, 1, u"Oracle#TWO", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}}};
    auto live = session(source, layouts);
    (void)admitted(live, description(41, 0));
    (void)admitted(live, description(7, 1));

    return live;
}

/** Callback recorder with no invocation-time allocation unless the explicit test action allocates. */
struct Recorder final : d::Consumer {
    std::array<std::array<std::int64_t, 64>, 64> values{};
    std::array<std::size_t, 64> sizes{};
    std::size_t calls = 0;
    std::function<void(const d::BorrowedBatch &)> action;
    std::atomic<int> *destroyed = nullptr;

    /** Record callback values and invoke the optional preconfigured test action.
     * @param batch Real callback-only views.
     */
    void onBatch(const d::BorrowedBatch &batch) override {
        const auto index = calls++;

        if (index < values.size()) {
            sizes[index] = batch.records().size();

            for (std::size_t i = 0; i < batch.records().size() && i < values[index].size(); ++i) {
                const auto &record = batch.records()[i];
                values[index][i] = record.typedValues.empty() ? 0 : record.typedValues.back().raw;
            }
        }

        if (action) {
            action(batch);
        }
    }

    /** Probe that the library-held callable dies only after callback quiescence. */
    ~Recorder() override {
        if (destroyed) {
            ++*destroyed;
        }
    }
};

/** Complete independent real-driver/dispatcher ownership graph. */
struct Fixture {
    c::LocalSchema local;
    p::LiveSession live;
    std::shared_ptr<Recorder> consumer;
    d::StreamDispatcher dispatcher;
    d::ClientHandle handle;
};

/** Prepare one independent serialized graph with an external real live session.
 * @param bounds Finite dispatcher capacities.
 * @return Fresh source/consumer/dispatcher/public-port owners.
 */
Fixture fixture(d::ClientLimits bounds = limits()) {
    auto local = schema();
    auto live = session(local);
    auto consumer = std::make_shared<Recorder>();
    auto work = turn();
    auto dispatcher = d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), bounds, consumer, work);
    REQUIRE(dispatcher);
    auto handle = dispatcher->handle();
    REQUIRE(handle);

    return {std::move(local), std::move(live), std::move(consumer), std::move(*dispatcher), std::move(*handle)};
}

/** Run finite owner actions until a numeric command result is published.
 * @param f Existing owner graph.
 * @param ticket Accepted result lease.
 * @return Detached result value.
 */
d::CommandResult complete(Fixture &f, const d::CommandTicket &ticket) {
    auto work = turn();

    for (unsigned attempt = 0; attempt < 256 && !ticket.result(); ++attempt) {
        REQUIRE(f.dispatcher.progress(work));
    }

    REQUIRE(ticket.result());

    return *ticket.result();
}

/** Apply an exact ordinary add through the real copied command port.
 * @param f Existing owner graph.
 * @param symbol Exact non-null key.
 * @param local Selected record identity.
 * @return Applied owner revision/change evidence.
 */
d::CommandResult add(Fixture &f, std::u16string_view symbol, std::int32_t local = 0) {
    const c::SubscriptionRequest request{{f.local.recordContext(local), symbol}, {}};
    auto work = submitTurn();
    auto ticket = f.handle.add(std::span(&request, 1), work);
    REQUIRE(ticket);
    const auto result = complete(f, *ticket);
    REQUIRE(result.status == d::CommandStatus::APPLIED);

    return result;
}

/** Transfer a newly protocol-admitted event before any ordinary dispatcher progress.
 * @param f Existing owner graph.
 * @param inputs Exact whole data source batch.
 */
void accept(Fixture &f, std::span<const Input> inputs) {
    auto event = admitted(f.live, data(inputs));
    REQUIRE(f.dispatcher.admit(std::move(event)));
    CHECK(event.residentBytes() == 0);
}

/** Accept then stage one whole event, without invoking a callback yet.
 * @param f Existing owner graph.
 * @param inputs Exact source records.
 */
void stage(Fixture &f, std::span<const Input> inputs) {
    accept(f, inputs);
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
}

/** Continue the caller-owned serialized host until idle or truthful local terminal.
 * @param f Existing owner graph.
 */
void pump(Fixture &f) {
    auto work = turn();

    for (unsigned attempt = 0; attempt < 256; ++attempt) {
        auto result = f.dispatcher.progress(work);
        REQUIRE(result);

        if (result->kind == d::ProgressKind::IDLE || result->state == d::DispatchState::TERMINAL) {
            return;
        }
    }

    FAIL("test owner did not reach an idle/terminal boundary");
}
} // namespace

/** Fail ordinary allocation only within explicit single-threaded production fault scopes.
 * @param bytes Requested storage.
 * @return Ordinary allocation, or std::bad_alloc.
 */
void *operator new(std::size_t bytes) {
    if (failAllocations) {
        if (allocationsRemaining == 0) {
            throw std::bad_alloc{};
        }

        --allocationsRemaining;
    }

    if (auto *pointer = std::malloc(bytes == 0 ? 1 : bytes)) {
        return pointer;
    }

    throw std::bad_alloc{};
}

/** Release ordinary replacement allocation.
 * @param pointer Existing allocation.
 */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** Release sized ordinary replacement allocation.
 * @param pointer Existing allocation.
 * @param bytes Requested size, not required by free.
 */
void operator delete(void *pointer, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(pointer);
}

/** Route array fault injection through ordinary replacement allocation.
 * @param bytes Requested backing.
 * @return Ordinary allocation or std::bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release replacement array allocation symmetrically.
 * @param pointer Existing backing.
 */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** Release sized array allocation symmetrically.
 * @param pointer Existing backing.
 * @param bytes Requested size.
 */
void operator delete[](void *pointer, std::size_t bytes) noexcept {
    ::operator delete(pointer, bytes);
}

TEST_CASE("client callback self-remove preserves its whole multi-record batch and retires later queued records") {
    auto f = fixture();
    CHECK(add(f, u"ALPHA").revision == 1);
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}, {0, u"ALPHA", 1, 13}};
    stage(f, inputs);
    d::CommandTicket removal;
    bool accepted = false;
    bool pendingInside = false;
    bool secondRecordStillBorrowed = false;
    bool reentrantRejected = false;
    f.consumer->action = [&](const d::BorrowedBatch &batch) {
        auto submit = submitTurn();
        const c::SubscriptionKey key{f.local.recordContext(0), u"ALPHA"};
        auto result = f.handle.remove(std::span(&key, 1), submit);
        accepted = result.has_value();

        if (result) {
            removal = std::move(*result);
            pendingInside = !removal.result();
        }

        secondRecordStillBorrowed = batch.records().size() == 2 && batch.records()[1].typedValues[2].raw == 11 &&
                                    f.dispatcher.desired()->entries().size() == 1;
        auto nestedWork = turn();
        auto nested = f.dispatcher.progress(nestedWork);
        reentrantRejected = !nested && nested.error() == Error::REENTRANT;
    };
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    CHECK(accepted);
    CHECK(pendingInside);
    CHECK(secondRecordStillBorrowed);
    CHECK(reentrantRejected);
    CHECK(f.consumer->calls == 1);
    CHECK(f.dispatcher.counts().acknowledgedRecords == 2);
    CHECK_FALSE(removal.result());
    REQUIRE(f.dispatcher.progress(work));
    REQUIRE(removal.result());
    CHECK(removal.result()->status == d::CommandStatus::APPLIED);
    CHECK(removal.result()->revision == 2);
    pump(f);
    CHECK(f.consumer->calls == 1);
    CHECK(f.dispatcher.counts().stagedRecords == 4);
    CHECK(f.dispatcher.counts().startedRecords == 2);
    CHECK(f.dispatcher.counts().completedRecords == 2);
    CHECK(f.dispatcher.counts().withdrawnRecords == 2);
    CHECK(f.dispatcher.resources().queuedRecords == 0);
}

TEST_CASE("client callback graceful stop drains only admitted local prefix and does not self-wait") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}, {0, u"ALPHA", 1, 13}};
    stage(f, inputs);
    bool requestAccepted = false;
    bool terminalInside = true;
    f.consumer->action = [&](const d::BorrowedBatch &) {
        requestAccepted = f.handle.requestStop(d::StopMode::GRACEFUL).has_value();
        terminalInside = f.handle.terminal().has_value();
    };
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    CHECK(requestAccepted);
    CHECK_FALSE(terminalInside);
    CHECK_FALSE(f.handle.terminal());
    CHECK(f.consumer->calls == 1);
    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::DRAINED);
    CHECK(f.handle.terminal()->counts.startedBatches == 2);
    CHECK(f.handle.terminal()->counts.completedRecords == 4);
    CHECK(f.handle.terminal()->counts.discardedRecords == 0);
    CHECK(f.consumer->calls == 2);
}

TEST_CASE("client callback abort preserves first callback failure through last-public-handle release") {
    std::atomic<int> destroyed{0};
    auto f = fixture();
    f.consumer->destroyed = &destroyed;
    (void)add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}};
    stage(f, inputs);
    bool callableAlive = false;
    bool terminalInside = true;
    f.consumer->action = [&](const d::BorrowedBatch &) {
        (void)f.handle.requestStop(d::StopMode::GRACEFUL);
        terminalInside = f.handle.terminal().has_value();
        f.handle = {};
        f.consumer.reset();
        callableAlive = destroyed.load() == 0;
        throw std::runtime_error("synthetic-sensitive callback detail");
    };
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    CHECK(callableAlive);
    CHECK(destroyed.load() == 0);
    CHECK_FALSE(terminalInside);
    CHECK(f.dispatcher.state() == d::DispatchState::CLOSING);
    CHECK_FALSE(f.dispatcher.terminal());
    pump(f);
    REQUIRE(f.dispatcher.terminal());
    CHECK(f.dispatcher.terminal()->outcome == d::DispatchOutcome::FAILED);
    CHECK(f.dispatcher.terminal()->primary == Error::CALLBACK_FAILURE);
    CHECK(f.dispatcher.terminal()->counts.failedBatches == 1);
    CHECK(f.dispatcher.terminal()->counts.failedRecords == 2);
    CHECK(f.dispatcher.terminal()->counts.completedRecords == 0);
    CHECK(f.dispatcher.terminal()->counts.discardedRecords == 1);
    CHECK(destroyed.load() == 1);
    REQUIRE(f.dispatcher.progress(work));
    CHECK(destroyed.load() == 1);
}

TEST_CASE("client admitted pending stage precedes queued add and zero-selection release retry counts once") {
    auto f = fixture();
    const Input before[]{{0, u"ALPHA", 1, 10}};
    accept(f, before);
    const c::SubscriptionRequest request{{f.local.recordContext(0), u"ALPHA"}, {}};
    auto submit = submitTurn();
    auto ticket = f.handle.add(std::span(&request, 1), submit);
    REQUIRE(ticket);
    auto limited = turn();
    limited.collector.visits = 0;
    auto staged = f.dispatcher.progress(limited);
    REQUIRE_FALSE(staged);
    CHECK(staged.error() == Error::WORK_LIMIT);
    CHECK(f.dispatcher.resources().pendingRecords == 1);
    CHECK_FALSE(ticket->result());
    CHECK(f.dispatcher.desired()->entries().empty());
    limited = turn();
    limited.bytes = 0;
    auto releaseYield = f.dispatcher.progress(limited);
    REQUIRE_FALSE(releaseYield);
    CHECK(releaseYield.error() == Error::WORK_LIMIT);
    CHECK(f.dispatcher.counts().ignoredRecords == 1);
    CHECK_FALSE(ticket->result());
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    CHECK(f.dispatcher.counts().ignoredRecords == 1);
    CHECK(complete(f, *ticket).status == d::CommandStatus::APPLIED);
    pump(f);
    CHECK(f.consumer->calls == 0);
    CHECK(f.dispatcher.counts().acceptedRecords == 1);
    const Input after[]{{0, u"ALPHA", 2, 20}};
    stage(f, after);
    pump(f);
    CHECK(f.consumer->calls == 1);
    CHECK(f.consumer->values[0][0] == 20);
}

TEST_CASE("client graceful includes pending admission and cancels unapplied removal without changing membership") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}};
    accept(f, inputs);
    const c::SubscriptionKey key{f.local.recordContext(0), u"ALPHA"};
    auto submit = submitTurn();
    auto removal = f.handle.remove(std::span(&key, 1), submit);
    REQUIRE(removal);
    REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
    CHECK_FALSE(f.handle.terminal());
    pump(f);
    REQUIRE(removal->result());
    CHECK(removal->result()->status == d::CommandStatus::CANCELLED_CLOSING);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::DRAINED);
    CHECK(f.handle.terminal()->revision == 1);
    CHECK(f.handle.terminal()->counts.stagedRecords == 1);
    CHECK(f.handle.terminal()->counts.completedRecords == 1);
    CHECK(f.handle.terminal()->counts.discardedPendingRecords == 0);
}

TEST_CASE("client abort pending event reports unclassified discard and never fabricates no-loss completion") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"OTHER", 1, 99}};
    accept(f, inputs);
    REQUIRE(f.handle.requestStop(d::StopMode::ABORT));
    CHECK_FALSE(f.handle.terminal());
    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::ABORTED);
    CHECK(f.handle.terminal()->unknownEligibility);
    CHECK(f.handle.terminal()->counts.acceptedRecords == 2);
    CHECK(f.handle.terminal()->counts.discardedPendingRecords == 2);
    CHECK(f.handle.terminal()->counts.stagedRecords == 0);
    CHECK(f.handle.terminal()->counts.startedBatches == 0);
    REQUIRE(f.handle.gap());
    CHECK(f.handle.gap()->pendingRecords == 2);
    CHECK(f.handle.gap()->unknownEligibility);
}

TEST_CASE("client ordinary queue saturation cannot block reserved stop or truthful command cancellation") {
    auto bounds = limits();
    bounds.commandSlots = 1;
    bounds.resultSlots = 1;
    auto f = fixture(bounds);
    const c::SubscriptionRequest request{{f.local.recordContext(0), u"ALPHA"}, {}};
    auto submit = submitTurn();
    auto first = f.handle.add(std::span(&request, 1), submit);
    REQUIRE(first);
    CHECK(f.handle.clear(submit).error() == Error::COMMAND_LIMIT);
    REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
    CHECK_FALSE(first->result());
    CHECK_FALSE(f.handle.terminal());
    pump(f);
    REQUIRE(first->result());
    CHECK(first->result()->status == d::CommandStatus::CANCELLED_CLOSING);
    CHECK(f.handle.clear(submit).error() == Error::CLOSING);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->counts.completedCommands == 1);
}

TEST_CASE("client retained completion slots are not overwritten after application or dispatcher destruction") {
    d::CommandTicket retained;
    std::uint64_t id = 0;
    {
        auto bounds = limits();
        bounds.resultSlots = 1;
        auto f = fixture(bounds);
        const c::SubscriptionRequest request{{f.local.recordContext(0), u"ALPHA"}, {}};
        auto submit = submitTurn();
        auto ticket = f.handle.add(std::span(&request, 1), submit);
        REQUIRE(ticket);
        CHECK(complete(f, *ticket).revision == 1);
        retained = *ticket;
        id = retained.id();
        CHECK(f.handle.clear(submit).error() == Error::RESULT_LIMIT);
        REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
        pump(f);
        REQUIRE(f.handle.terminal());
    }
    REQUIRE(retained.result());
    CHECK(retained.id() == id);
    CHECK(retained.result()->status == d::CommandStatus::APPLIED);
    CHECK(retained.result()->revision == 1);
    std::atomic<bool> observed{false};
    std::jthread producer([ticket = std::move(retained), &observed, id]() mutable {
        observed = ticket.result().has_value() && ticket.id() == id && ticket.result()->revision == 1;
        ticket = {};
    });
    producer.join();
    CHECK(observed.load());
}

TEST_CASE("client callback abort discards later source records without retrying or claiming drained") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}, {0, u"ALPHA", 1, 13}};
    stage(f, inputs);
    f.consumer->action = [&](const d::BorrowedBatch &) {
        (void)f.handle.requestStop(d::StopMode::ABORT);
    };
    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::ABORTED);
    CHECK(f.handle.terminal()->counts.completedRecords == 2);
    CHECK(f.handle.terminal()->counts.discardedRecords == 2);
    CHECK(f.handle.terminal()->counts.startedBatches == 1);
}

TEST_CASE("client last public release from another thread cannot complete a still executing callback") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(f, input);
    std::atomic<bool> entered{false};
    std::atomic<bool> released{false};
    std::atomic<bool> sawIncomplete{false};
    auto producerHandle = std::move(f.handle);
    f.consumer->action = [&](const d::BorrowedBatch &) {
        entered.store(true, std::memory_order_release);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);

        while (!released.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
    };
    std::jthread producer([handle = std::move(producerHandle), &entered, &released, &sawIncomplete]() mutable {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);

        while (!entered.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        sawIncomplete = entered.load(std::memory_order_acquire) && !handle.terminal();
        handle = {};
        released.store(true, std::memory_order_release);
    });
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    producer.join();
    CHECK(entered.load());
    CHECK(released.load());
    CHECK(sawIncomplete.load());
    CHECK_FALSE(f.dispatcher.terminal());
    pump(f);
    REQUIRE(f.dispatcher.terminal());
    CHECK(f.dispatcher.terminal()->outcome == d::DispatchOutcome::ABORTED);
    CHECK(f.dispatcher.terminal()->counts.completedRecords == 1);
}

TEST_CASE("client detached batch retains original context membership generation and payload across terminal and "
          "foreign release") {
    d::OwnedBatch retained;
    d::ClientIdentity run;
    d::ClientIdentity epoch;
    c::RecordContext context;
    std::uint64_t firstGeneration = 0;
    std::uint64_t secondGeneration = 0;
    std::atomic<int> destroyed{0};
    {
        auto f = fixture();
        f.consumer->destroyed = &destroyed;
        context = f.local.recordContext(0);
        (void)add(f, u"ALPHA");
        const Input first[]{{0, u"ALPHA", 1, 10}};
        auto event = admitted(f.live, data(first));
        firstGeneration = event.snapshot().identity().generation();
        REQUIRE(f.dispatcher.admit(std::move(event)));
        auto work = turn();
        REQUIRE(f.dispatcher.progress(work));
        (void)admitted(f.live, description(41, 0));
        event = admitted(f.live, data(first));
        secondGeneration = event.snapshot().identity().generation();
        REQUIRE(f.dispatcher.admit(std::move(event)));
        REQUIRE(f.dispatcher.progress(work));
        f.consumer->action = [&](const d::BorrowedBatch &batch) {
            retained = batch.retain();
        };
        REQUIRE(f.dispatcher.progress(work));
        REQUIRE(retained.records().size() == 2);
        run = retained.runIdentity();
        epoch = retained.epochIdentity();
        CHECK_FALSE(run.matches(epoch));
        CHECK(run.matches(retained.runIdentity()));
        CHECK(firstGeneration != secondGeneration);
        CHECK(retained.records()[0].bindingGeneration == firstGeneration);
        CHECK(retained.records()[1].bindingGeneration == secondGeneration);
        CHECK(f.dispatcher.desired()->isCurrent(retained.records()[0].membership));
        REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
        f.consumer.reset();
        pump(f);
        REQUIRE(f.handle.terminal());
        CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::DRAINED);
        CHECK(f.dispatcher.resources().batchOwners == 1);
        CHECK(destroyed.load() == 1);
    }
    CHECK(retained.records()[0].context.matches(context));
    CHECK(retained.records()[0].symbol == u"ALPHA");
    CHECK(retained.records()[0].typedValues[2].raw == 10);
    std::atomic<bool> observed{false};
    std::jthread producer([batch = std::move(retained), run, epoch, context, &observed]() mutable {
        observed = batch.records().size() == 2 && batch.records()[0].context.matches(context) &&
                   batch.records()[0].typedValues[2].raw == 10 && run.matches(batch.runIdentity()) &&
                   epoch.matches(batch.epochIdentity());
        auto independentLease = batch;
        batch = {};
        observed = observed.load() && independentLease.records()[1].symbol == u"ALPHA";
        independentLease = {};
    });
    producer.join();
    CHECK(observed.load());
    CHECK(destroyed.load() == 1);
}

TEST_CASE("client retained batch quota fails visibly while terminal remains independent from caller-held values") {
    auto bounds = limits();
    bounds.batchOwners = 1;
    auto f = fixture(bounds);
    (void)add(f, u"ALPHA");
    d::OwnedBatch retained;
    f.consumer->action = [&](const d::BorrowedBatch &batch) {
        retained = batch.retain();
    };
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}, {0, u"ALPHA", 1, 13}};
    stage(f, inputs);
    auto work = turn();
    REQUIRE(f.dispatcher.progress(work));
    CHECK(f.dispatcher.resources().batchOwners == 1);
    REQUIRE(f.dispatcher.progress(work));
    CHECK(f.dispatcher.state() == d::DispatchState::CLOSING);
    CHECK(f.dispatcher.resources().queuedRecords == 2);
    CHECK(f.dispatcher.counts().acknowledgedRecords == 2);
    CHECK(f.consumer->calls == 1);
    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::FAILED);
    CHECK(f.handle.terminal()->primary == Error::RESOURCE_LIMIT);
    CHECK(f.handle.terminal()->counts.discardedRecords == 2);
    CHECK(f.dispatcher.resources().batchOwners == 1);
    CHECK(retained.records()[0].typedValues[2].raw == 10);
    std::jthread producer([batch = std::move(retained)]() mutable {
        batch = {};
    });
    producer.join();
    CHECK(f.dispatcher.resources().batchOwners == 0);
    CHECK(f.dispatcher.resources().batchBytes == 0);
}

TEST_CASE("client whole candidate allocation arena and work failures never acknowledge an earlier record prefix") {
    for (std::size_t fault = 0; fault < 2; ++fault) {
        auto f = fixture();
        (void)add(f, u"ALPHA");
        const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
        stage(f, inputs);
        auto work = turn();
        auto result = [&] {
            AllocationFault injection(fault);

            return f.dispatcher.progress(work);
        }();
        REQUIRE(result);
        CHECK(result->state == d::DispatchState::CLOSING);
        CHECK(f.dispatcher.counts().acknowledgedRecords == 0);
        CHECK(f.dispatcher.resources().queuedRecords == 2);
        CHECK(f.dispatcher.resources().batchOwners == 0);
        CHECK(f.consumer->calls == 0);
        pump(f);
        REQUIRE(f.handle.terminal());
        CHECK(f.handle.terminal()->primary == Error::ALLOCATION_FAILURE);
        CHECK(f.handle.terminal()->counts.discardedRecords == 2);
    }

    auto bounds = limits();
    bounds.batchArenaBytes = 1;
    auto tooSmall = fixture(bounds);
    (void)add(tooSmall, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
    stage(tooSmall, inputs);
    auto work = turn();
    REQUIRE(tooSmall.dispatcher.progress(work));
    CHECK(tooSmall.dispatcher.counts().acknowledgedRecords == 0);
    CHECK(tooSmall.dispatcher.resources().queuedRecords == 2);
    CHECK(tooSmall.dispatcher.resources().batchOwners == 0);
    CHECK(tooSmall.consumer->calls == 0);
    pump(tooSmall);
    CHECK(tooSmall.handle.terminal()->primary == Error::RESOURCE_LIMIT);
    auto retry = fixture();
    (void)add(retry, u"ALPHA");
    stage(retry, inputs);
    auto limited = turn();
    limited.fields = 0;
    CHECK(retry.dispatcher.progress(limited).error() == Error::WORK_LIMIT);
    CHECK(retry.dispatcher.state() == d::DispatchState::OPEN);
    CHECK(retry.dispatcher.resources().queuedRecords == 2);
    CHECK(retry.dispatcher.counts().acknowledgedRecords == 0);
    pump(retry);
    CHECK(retry.consumer->calls == 1);
    CHECK(retry.consumer->sizes[0] == 2);
}

TEST_CASE("client producer text is copied before return and submission remains allocation-free") {
    auto f = fixture();
    std::u16string symbol = u"ALPHA";
    const c::SubscriptionRequest request{{f.local.recordContext(0), symbol}, {}};
    auto work = submitTurn();
    auto ticket = [&] {
        AllocationFault injection(0);

        return f.handle.add(std::span(&request, 1), work);
    }();
    REQUIRE(ticket);
    symbol.assign(u"OTHER");
    CHECK(complete(f, *ticket).status == d::CommandStatus::APPLIED);
    REQUIRE(f.dispatcher.desired()->entries().size() == 1);
    CHECK(f.dispatcher.desired()->entries()[0].key.symbol == u"ALPHA");
    const c::SubscriptionRequest invalid{{f.local.recordContext(0), std::u16string_view{}}, {}};
    auto rejected = f.handle.add(std::span(&invalid, 1), work);
    REQUIRE(rejected);
    CHECK(complete(f, *rejected).error == c::SubscriptionError::INVALID_SELECTOR);
    CHECK(f.dispatcher.desired()->revision() == 1);
    CHECK(f.handle.requestStop(static_cast<d::StopMode>(99)).error() == Error::INVALID_CONFIGURATION);
}

TEST_CASE("client cross-thread submissions own bounded text and complete without producer access to live state") {
    auto f = fixture();
    const auto context = f.local.recordContext(0);
    std::atomic<unsigned> done{0};
    std::atomic<unsigned> successful{0};
    std::atomic<bool> failed{false};
    const auto producer = [&](d::ClientHandle handle, std::u16string symbol) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);

        for (unsigned index = 0; index < 32 && !failed.load(); ++index) {
            d::CommandTicket ticket;

            while (!ticket.id() && std::chrono::steady_clock::now() < deadline) {
                auto work = submitTurn();

                if (index % 2 == 0) {
                    const c::SubscriptionRequest request{{context, symbol}, {}};
                    auto accepted = handle.add(std::span(&request, 1), work);

                    if (accepted) {
                        ticket = std::move(*accepted);
                    } else if (accepted.error() != Error::COMMAND_LIMIT && accepted.error() != Error::RESULT_LIMIT) {
                        failed = true;
                        break;
                    }
                } else {
                    const c::SubscriptionKey key{context, symbol};
                    auto accepted = handle.remove(std::span(&key, 1), work);

                    if (accepted) {
                        ticket = std::move(*accepted);
                    } else if (accepted.error() != Error::COMMAND_LIMIT && accepted.error() != Error::RESULT_LIMIT) {
                        failed = true;
                        break;
                    }
                }

                std::this_thread::yield();
            }

            if (!ticket.id()) {
                failed = true;
                break;
            }

            while (!ticket.result() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::yield();
            }

            const auto result = ticket.result();

            if (!result || result->status != d::CommandStatus::APPLIED) {
                failed = true;
                break;
            }

            ++successful;
        }

        ++done;
    };
    std::jthread first(producer, f.handle, std::u16string(u"ALPHA"));
    std::jthread second(producer, f.handle, std::u16string(u"BETA"));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(12);

    while (done.load() != 2 && std::chrono::steady_clock::now() < deadline) {
        auto work = turn();
        auto result = f.dispatcher.progress(work);

        if (!result) {
            failed = true;
        }

        std::this_thread::yield();
    }

    first.join();
    second.join();
    CHECK_FALSE(failed.load());
    CHECK(successful.load() == 64);
    CHECK(done.load() == 2);
    CHECK(f.dispatcher.counts().completedCommands == 64);
    CHECK(f.dispatcher.desired()->entries().empty());
    CHECK(f.consumer->calls == 0);
}

TEST_CASE("client independent contexts reject command interchange and foreign epoch cannot cross a drained queue") {
    auto one = fixture();
    auto two = fixture();
    const c::SubscriptionRequest wrong{{two.local.recordContext(0), u"ALPHA"}, {}};
    auto submit = submitTurn();
    auto rejected = one.handle.add(std::span(&wrong, 1), submit);
    REQUIRE(rejected);
    CHECK(complete(one, *rejected).error == c::SubscriptionError::INVALID_CONTEXT);
    CHECK(one.dispatcher.desired()->entries().empty());
    (void)add(one, u"ALPHA");
    (void)add(two, u"ALPHA");
    d::OwnedBatch first;
    d::OwnedBatch second;
    one.consumer->action = [&](const d::BorrowedBatch &batch) {
        first = batch.retain();
    };
    two.consumer->action = [&](const d::BorrowedBatch &batch) {
        second = batch.retain();
    };
    const Input inputs[]{{0, u"ALPHA", 1, 10}};
    stage(one, inputs);
    stage(two, inputs);
    pump(one);
    pump(two);
    CHECK_FALSE(first.runIdentity().matches(second.runIdentity()));
    CHECK_FALSE(first.epochIdentity().matches(second.epochIdentity()));
    CHECK_FALSE(first.records()[0].context.matches(second.records()[0].context));
    auto foreign = admitted(two.live, data(inputs));
    const auto bytes = foreign.residentBytes();
    CHECK(one.dispatcher.admit(std::move(foreign)).error() == Error::INVALID_EPOCH);
    CHECK(foreign.residentBytes() == bytes);
    CHECK(one.dispatcher.state() == d::DispatchState::OPEN);
    auto invalidContext = fixture();
    auto incompatible = admitted(two.live, data(inputs));
    REQUIRE(invalidContext.dispatcher.admit(std::move(incompatible)));
    pump(invalidContext);
    REQUIRE(invalidContext.handle.terminal());
    CHECK(invalidContext.handle.terminal()->primary == Error::INVALID_CONTEXT);
    CHECK(invalidContext.handle.terminal()->counts.startedBatches == 0);
    CHECK(invalidContext.handle.terminal()->counts.discardedPendingRecords == 1);
}

TEST_CASE("client port exact limits and UTF16 ownership preserve empty NUL and unpaired units") {
    auto bounds = limits();
    bounds.commandEntries = 2;
    bounds.commandSymbolUnits = 3;
    auto f = fixture(bounds);
    char16_t borrowed[]{u'X', 0, static_cast<char16_t>(0xd800)};
    const char16_t exact[]{u'X', 0, static_cast<char16_t>(0xd800)};
    const c::SubscriptionRequest requests[]{{{f.local.recordContext(0), u""}, {}},
                                            {{f.local.recordContext(0), std::u16string_view(borrowed, 3)}, {}}};
    auto submit = submitTurn();
    auto ticket = f.handle.add(requests, submit);
    REQUIRE(ticket);
    borrowed[0] = u'Z';
    CHECK(complete(f, *ticket).status == d::CommandStatus::APPLIED);
    CHECK(f.dispatcher.desired()->entries().size() == 2);
    const c::SubscriptionRequest excessive{{f.local.recordContext(0), u"FOUR"}, {}};
    CHECK(f.handle.add(std::span(&excessive, 1), submit).error() == Error::RESOURCE_LIMIT);
    const c::SubscriptionRequest tooMany[]{requests[0], requests[0], requests[0]};
    CHECK(f.handle.add(tooMany, submit).error() == Error::RESOURCE_LIMIT);
    const Input inputs[]{{0, u"", 1, 10}, {0, std::u16string_view(exact, 3), 1, 11}, {0, u"Z", 1, 99}};
    d::OwnedBatch retained;
    f.consumer->action = [&](const d::BorrowedBatch &batch) {
        retained = batch.retain();
    };
    stage(f, inputs);
    pump(f);
    REQUIRE(retained.records().size() == 2);
    CHECK(retained.records()[0].symbol.empty());
    CHECK(retained.records()[0].symbol.data() != nullptr);
    CHECK(retained.records()[1].symbol == std::u16string_view(exact, 3));
    CHECK(f.dispatcher.counts().ignoredRecords == 1);
}

TEST_CASE("client factory allocation failures preserve caller callback and never create an executing consumer") {
    auto local = schema();
    auto consumer = std::make_shared<Recorder>();
    bool completed = false;
    std::size_t failures = 0;

    for (std::size_t fault = 0; fault < 64; ++fault) {
        auto work = turn();
        auto result = [&] {
            AllocationFault injection(fault);

            return d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), limits(), consumer, work);
        }();

        if (result) {
            completed = true;
            break;
        }

        CHECK(result.error() == Error::ALLOCATION_FAILURE);
        CHECK(local.recordContext(0).valid());
        CHECK(consumer->calls == 0);
        ++failures;
    }

    CHECK(completed);
    CHECK(failures > 8);
    auto zero = limits();
    zero.residentBytes = 0;
    auto work = turn();
    CHECK(d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), zero, consumer, work).error() ==
          Error::RESOURCE_LIMIT);
    zero = limits();
    zero.batchRecords = 0;
    CHECK(d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), zero, consumer, work).error() ==
          Error::INVALID_CONFIGURATION);
    auto impossible = limits();
    impossible.commandEntries = std::numeric_limits<std::size_t>::max();
    CHECK(d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), impossible, consumer, work).error() ==
          Error::INVALID_CONFIGURATION);
}

TEST_CASE("client full standard Quote Java fixture reaches callback and prepared mapping with explicit external output "
          "ownership") {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};
    auto defaults = q::defaultQuoteSchema(schemaBounds);
    REQUIRE(defaults);
    REQUIRE(defaults->records()[0].fields.size() == 8);
    auto local = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    // The Quote plan charges all 72 full-type alternatives, including absent optional fields.
    auto mapping = q::PreparedQuote::prepare(*local, 0, {128, 256, 32, 64, 128, 128, 16384, 1000000});
    REQUIRE(mapping);
    p::OutgoingRecord declaration{17, 0, u"Quote", {}};
    const auto &record = local->records()[0];

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
            return entry.fieldId == field;
        });
        REQUIRE(lookup != record.lookups.end());
        declaration.fields.push_back(
            {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
    }

    auto live = session(*local, std::span(&declaration, 1));
    auto consumer = std::make_shared<Recorder>();
    auto work = turn();
    auto dispatcher = d::StreamDispatcher::create(*local, desiredLimits(), collectorLimits(), limits(), consumer, work);
    REQUIRE(dispatcher);
    auto handle = dispatcher->handle();
    REQUIRE(handle);
    Fixture f{std::move(*local), std::move(live), consumer, std::move(*dispatcher), std::move(*handle)};
    CHECK(add(f, u"IBM").revision == 1);
    const c::SubscriptionRequest interest{{f.local.recordContext(0), u"IBM"}, {}};
    auto protocolWork = liveTurn();
    auto output = f.live.subscribe(p::SubscriptionMessage::STREAM_ADD, std::span(&interest, 1), protocolWork);
    REQUIRE(output);
    REQUIRE_FALSE(output->pendingBytes().empty());
    std::optional<q::Quote> quote;
    d::OwnedBatch retained;
    bool mapped = false;
    bool exactTypes = false;
    f.consumer->action = [&](const d::BorrowedBatch &batch) {
        retained = batch.retain();

        if (batch.records().size() != 1 || batch.records()[0].typedValues.size() != 8) {
            return;
        }

        const auto &generic = batch.records()[0];
        exactTypes = generic.context.matches(mapping->context());
        std::vector<c::FieldValue> values;

        for (std::size_t i = 0; i < generic.typedValues.size(); ++i) {
            exactTypes = exactTypes && generic.typedValues[i].type == q::QUOTE_FIELD_TYPES[i];
            values.push_back({generic.typedValues[i].type, generic.typedValues[i].raw});
        }

        auto result = mapping->map(generic.symbol, {generic.context, values}, 128);
        mapped = result.has_value();

        if (result) {
            quote.emplace(std::move(*result));
        }

        (void)f.handle.requestStop(d::StopMode::GRACEFUL);
    };
    const auto path = std::filesystem::path(ORG_TTLDTOR_QDCXX_CLIENT_SOURCE_ROOT) /
                      "tests/fixtures/protocol/quote/expected/time-1.qtp";
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> characters((std::istreambuf_iterator<char>(input)), {});
    const auto capture = std::as_bytes(std::span(characters));
    p::FrameReader framing(liveLimits().frame);
    std::size_t offset = 0;

    while (offset < capture.size()) {
        auto framed = framing.feed(capture.subspan(offset));
        REQUIRE(framed.frame);
        offset += framed.accepted;

        if (framed.frame->type != 2 && framed.frame->type != 15) {
            continue;
        }

        const auto begin = static_cast<std::size_t>(framed.frame->startOffset);
        const auto size = static_cast<std::size_t>(framed.frame->endOffset - framed.frame->startOffset);
        auto event = admitted(f.live, capture.subspan(begin, size));
        REQUIRE(f.dispatcher.admit(std::move(event)));
        auto ownerWork = turn();
        REQUIRE(f.dispatcher.progress(ownerWork));
    }

    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::DRAINED);
    CHECK(f.handle.terminal()->counts.completedRecords == 1);
    CHECK(mapped);
    CHECK(exactTypes);
    REQUIRE(quote);
    CHECK(quote->symbol == u"IBM");
    CHECK(quote->bidTimeMs == 1000);
    CHECK(quote->askTimeMs == 1000);
    CHECK(quote->timeMs == 1000);
    REQUIRE(retained.records().size() == 1);
    CHECK(retained.records()[0].typedValues.size() == 8);
    // Dispatcher completion cannot consume or certify externally owned wire output.
    CHECK_FALSE(output->pendingBytes().empty());
    REQUIRE(output->consume(output->pendingBytes().size()));
    CHECK(output->pendingBytes().empty());
    CHECK(f.live.finish() == p::LiveStatus::CLOSED);
}

TEST_CASE("client detached nullable text and opaque payloads survive all serialized protocol owners") {
    d::OwnedBatch retained;
    std::u16string exact(40, u'A');
    exact[0] = 0;
    exact[1] = static_cast<char16_t>(0xd800);
    exact[2] = static_cast<char16_t>(0xdc00);
    exact[3] = static_cast<char16_t>(0xdc00);
    {
        const c::LocalRecord record{0,
                                    u"Text",
                                    false,
                                    {{10}, {137}, {9}, {233}, {249}},
                                    {{u"array", 0}, {u"string", 1}, {u"bytes", 2}, {u"custom", 3}, {u"serial", 4}}};
        auto local =
            c::LocalSchema::create(std::span(&record, 1), {1, 5, 5, 16, 128}, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        const p::OutgoingRecord layout{
            14,
            0,
            u"Text",
            {{u"array", 10, 0}, {u"string", 137, 1}, {u"bytes", 9, 2}, {u"custom", 233, 3}, {u"serial", 249, 4}}};
        auto live = session(*local, std::span(&layout, 1));
        std::vector<std::byte> payload;
        number(payload, 32);
        text(payload, u"Text");
        number(payload, 5);

        for (const auto &field : layout.fields) {
            text(payload, field.wireName);
            number(payload, field.serialTypeId);
        }

        (void)admitted(live, frame(2, payload));
        auto consumer = std::make_shared<Recorder>();
        auto work = turn();
        auto dispatcher =
            d::StreamDispatcher::create(*local, desiredLimits(), collectorLimits(), limits(), consumer, work);
        REQUIRE(dispatcher);
        auto handle = dispatcher->handle();
        REQUIRE(handle);
        Fixture f{std::move(*local), std::move(live), consumer, std::move(*dispatcher), std::move(*handle)};
        (void)add(f, u"NULL");
        (void)add(f, u"TEXT");
        f.consumer->action = [&](const d::BorrowedBatch &batch) {
            retained = batch.retain();
        };
        payload.clear();
        const auto symbol = [&](std::u16string_view value) {
            payload.push_back(std::byte{0xfd});
            number(payload, static_cast<std::int64_t>(value.size()));

            for (const auto unit : value) {
                std::array<std::byte, 4> bytes{};
                const auto result = e::writeUtfChar(unit, bytes);
                payload.insert(payload.end(), bytes.begin(),
                               bytes.begin() + static_cast<std::ptrdiff_t>(result.written));
            }

            number(payload, 32);
        };
        symbol(u"NULL");
        number(payload, -1);
        text(payload, u"");
        number(payload, 0);
        number(payload, -1);
        number(payload, 3);
        payload.insert(payload.end(), {std::byte{0}, std::byte{0xff}, std::byte{0}});
        symbol(u"TEXT");
        std::array<std::byte, 512> encoded{};
        const auto array = e::writeCharArray(exact, encoded, {512, 128});
        REQUIRE(array.status == e::Status::OK);
        payload.insert(payload.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(array.written));
        text(payload, exact);
        number(payload, 2);
        payload.insert(payload.end(), {std::byte{0}, std::byte{0xff}});
        number(payload, 0);
        number(payload, -1);
        auto event = admitted(f.live, frame(15, payload));
        REQUIRE(f.dispatcher.admit(std::move(event)));
        REQUIRE(f.dispatcher.progress(work));
        pump(f);
        REQUIRE(retained.records().size() == 2);
        REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
        pump(f);
        REQUIRE(f.handle.terminal());
    }
    std::atomic<bool> correct{false};
    std::jthread producer([batch = std::move(retained), &correct, exact]() mutable {
        const auto nulls = batch.records()[0].typedValues;
        const auto values = batch.records()[1].typedValues;
        correct = nulls.size() == 5 && values.size() == 5 && !nulls[0].units && nulls[1].units &&
                  nulls[1].units->empty() && nulls[2].bytes && nulls[2].bytes->empty() && !nulls[3].bytes &&
                  nulls[4].bytes && nulls[4].bytes->size() == 3 && values[0].units && values[1].units &&
                  *values[0].units == exact && *values[1].units == exact && values[2].bytes &&
                  values[2].bytes->size() == 2 && values[3].bytes && values[3].bytes->empty() && !values[4].bytes;
        batch = {};
    });
    producer.join();
    CHECK(correct.load());
}

TEST_CASE("client callback remove-readd cannot revive an older queued activation and replacement remains atomic") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    d::CommandTicket removed;
    d::CommandTicket readded;
    d::OwnedBatch old;
    bool accepted = true;
    f.consumer->action = [&](const d::BorrowedBatch &batch) {
        if (f.consumer->calls != 1) {
            return;
        }

        old = batch.retain();
        auto submit = submitTurn();
        const c::SubscriptionKey key{f.local.recordContext(0), u"ALPHA"};
        const c::SubscriptionRequest request{key, {}};
        auto removal = f.handle.remove(std::span(&key, 1), submit);
        auto addition = f.handle.add(std::span(&request, 1), submit);
        accepted = removal.has_value() && addition.has_value();

        if (removal) {
            removed = std::move(*removal);
        }

        if (addition) {
            readded = std::move(*addition);
        }
    };
    const Input first[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}};
    stage(f, first);
    pump(f);
    REQUIRE(accepted);
    REQUIRE(removed.result());
    REQUIRE(readded.result());
    CHECK(removed.id() < readded.id());
    CHECK(removed.result()->revision == 2);
    CHECK(readded.result()->revision == 3);
    CHECK(f.consumer->calls == 1);
    CHECK(f.dispatcher.counts().withdrawnRecords == 1);
    CHECK_FALSE(f.dispatcher.desired()->isCurrent(old.records()[0].membership));
    auto independent = schema();
    const c::SubscriptionRequest invalid[]{{{f.local.recordContext(0), u"BETA"}, {}},
                                           {{independent.recordContext(0), u"WRONG"}, {}}};
    auto submit = submitTurn();
    auto rejected = f.handle.replace(invalid, submit);
    REQUIRE(rejected);
    CHECK(complete(f, *rejected).error == c::SubscriptionError::INVALID_CONTEXT);
    REQUIRE(f.dispatcher.desired()->entries().size() == 1);
    CHECK(f.dispatcher.desired()->entries()[0].key.symbol == u"ALPHA");
    CHECK(f.dispatcher.desired()->revision() == 3);
    const c::SubscriptionRequest replacement{{f.local.recordContext(0), u"BETA"}, {}};
    auto replaced = f.handle.replace(std::span(&replacement, 1), submit);
    REQUIRE(replaced);
    CHECK(complete(f, *replaced).revision == 4);
    const Input later[]{{0, u"ALPHA", 2, 30}, {0, u"BETA", 2, 40}};
    stage(f, later);
    pump(f);
    CHECK(f.consumer->calls == 2);
    CHECK(f.consumer->values[1][0] == 40);
    auto cleared = f.handle.clear(submit);
    REQUIRE(cleared);
    CHECK(complete(f, *cleared).revision == 5);
    stage(f, later);
    pump(f);
    CHECK(f.consumer->calls == 2);
}

TEST_CASE("client final callable destruction can reenter the closed port without a mutation lock") {
    struct ClosingConsumer final : d::Consumer {
        d::ClientHandle handle;
        bool &destroyed;
        bool &closedSubmission;

        /** Capture external destruction observations.
         * @param finished External lifetime flag.
         * @param rejected External port observation.
         */
        ClosingConsumer(bool &finished, bool &rejected) : destroyed(finished), closedSubmission(rejected) {
        }

        /** No application work is needed; this case targets callable retirement.
         * @param batch Borrowed immutable callback batch.
         */
        void onBatch(const d::BorrowedBatch &batch) override {
            static_cast<void>(batch);
        }

        /** Exercise a real port-locking method during final user-state destruction. */
        ~ClosingConsumer() override {
            auto work = submitTurn();
            auto result = handle.clear(work);
            closedSubmission = !result && result.error() == Error::CLOSING;
            destroyed = true;
        }
    };

    bool destroyed = false;
    bool closedSubmission = false;
    auto local = schema();
    auto consumer = std::make_shared<ClosingConsumer>(destroyed, closedSubmission);
    auto work = turn();
    auto dispatcher = d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), limits(), consumer, work);
    REQUIRE(dispatcher);
    auto handle = dispatcher->handle();
    REQUIRE(handle);
    consumer->handle = *handle;
    consumer.reset();
    REQUIRE(handle->requestStop(d::StopMode::GRACEFUL));

    for (unsigned attempt = 0; attempt < 16 && !handle->terminal(); ++attempt) {
        REQUIRE(dispatcher->progress(work));
    }

    REQUIRE(handle->terminal());
    CHECK(handle->terminal()->outcome == d::DispatchOutcome::DRAINED);
    CHECK(destroyed);
    CHECK(closedSubmission);
}

TEST_CASE("client fresh handle acquisition and last-public retirement have one linearized winner") {
    auto local = schema();
    auto consumer = std::make_shared<Recorder>();

    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        auto work = turn();
        auto dispatcher =
            d::StreamDispatcher::create(local, desiredLimits(), collectorLimits(), limits(), consumer, work);
        REQUIRE(dispatcher);
        auto original = dispatcher->handle();
        REQUIRE(original);
        std::atomic<bool> releaseNow{false};
        std::jthread producer([handle = std::move(*original), &releaseNow]() mutable {
            while (!releaseNow.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            handle = {};
        });
        releaseNow.store(true, std::memory_order_release);
        auto fresh = dispatcher->handle();
        producer.join();

        if (fresh) {
            // Acquisition won the shared CAS: the old reference was no longer the last public reference.
            auto submit = submitTurn();
            auto accepted = fresh->clear(submit);
            REQUIRE(accepted);
            REQUIRE(dispatcher->progress(work));
            REQUIRE(accepted->result());
            CHECK(accepted->result()->status == d::CommandStatus::APPLIED);
            REQUIRE(fresh->requestStop(d::StopMode::GRACEFUL));
            auto closingCopy = *fresh;
            CHECK_FALSE(closingCopy.terminal());
            CHECK(dispatcher->handle().error() == Error::CLOSING);
        } else {
            // Retirement won: no fresh public reference can resurrect the endpoint.
            CHECK(fresh.error() == Error::CLOSING);
        }
    }
}

TEST_CASE("client exact detached field symbol text and opaque capacities reject whole candidates when exceeded") {
    for (unsigned dimension = 0; dimension < 5; ++dimension) {
        auto bounds = limits();
        bounds.batchFields = 4;
        bounds.batchSymbolUnits = 10;
        bounds.batchFieldUnits = 6;
        bounds.batchByteUnits = 8;

        if (dimension == 1) {
            --bounds.batchFields;
        }

        if (dimension == 2) {
            --bounds.batchSymbolUnits;
        }

        if (dimension == 3) {
            --bounds.batchFieldUnits;
        }

        if (dimension == 4) {
            --bounds.batchByteUnits;
        }

        const c::LocalRecord record{0, u"Payload", false, {{137}, {9}}, {{u"text", 0}, {u"bytes", 1}}};
        auto local =
            c::LocalSchema::create(std::span(&record, 1), {1, 2, 2, 16, 64}, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        const p::OutgoingRecord layout{41, 0, u"Payload", {{u"text", 137, 0}, {u"bytes", 9, 1}}};
        auto live = session(*local, std::span(&layout, 1));
        auto liveWork = liveTurn();
        const std::int32_t sender = 41;
        auto declared = live.describe(std::span(&sender, 1), liveWork);
        REQUIRE(declared);
        (void)admitted(live, declared->bytes());
        auto consumer = std::make_shared<Recorder>();
        auto work = turn();
        auto dispatcher =
            d::StreamDispatcher::create(*local, desiredLimits(), collectorLimits(), bounds, consumer, work);
        REQUIRE(dispatcher);
        auto handle = dispatcher->handle();
        REQUIRE(handle);
        Fixture f{std::move(*local), std::move(live), consumer, std::move(*dispatcher), std::move(*handle)};
        (void)add(f, u"ALPHA");
        std::vector<std::byte> payload;

        for (unsigned index = 0; index < 2; ++index) {
            payload.push_back(std::byte{0xfd});
            number(payload, 5);

            for (const auto unit : std::u16string_view(u"ALPHA")) {
                std::array<std::byte, 4> encoded{};
                const auto result = e::writeUtfChar(unit, encoded);
                payload.insert(payload.end(), encoded.begin(),
                               encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
            }

            number(payload, 41);
            text(payload, u"abc");
            number(payload, 4);
            payload.insert(payload.end(), {std::byte{0}, std::byte{1}, std::byte{0xff}, std::byte{2}});
        }

        auto event = admitted(f.live, frame(15, payload));
        REQUIRE(f.dispatcher.admit(std::move(event)));
        REQUIRE(f.dispatcher.progress(work));
        REQUIRE(f.dispatcher.progress(work));

        if (dimension == 0) {
            CHECK(f.consumer->calls == 1);
            CHECK(f.consumer->sizes[0] == 2);
            CHECK(f.dispatcher.counts().completedRecords == 2);
            CHECK(f.dispatcher.state() == d::DispatchState::OPEN);
        } else {
            CHECK(f.consumer->calls == 0);
            CHECK(f.dispatcher.counts().acknowledgedRecords == 0);
            CHECK(f.dispatcher.resources().queuedRecords == 2);
            pump(f);
            REQUIRE(f.handle.terminal());
            CHECK(f.handle.terminal()->primary == Error::RESOURCE_LIMIT);
            CHECK(f.handle.terminal()->counts.discardedRecords == 2);
        }
    }
}

TEST_CASE("client batch byte and total reservation caps include old retained plus next candidate owners") {
    std::size_t unit = 0;
    std::size_t fixed = 0;
    {
        auto probe = fixture();
        fixed = probe.dispatcher.resources().fixedReservationBytes;
        (void)add(probe, u"ALPHA");
        d::OwnedBatch retained;
        probe.consumer->action = [&](const d::BorrowedBatch &batch) {
            retained = batch.retain();
        };
        const Input input[]{{0, u"ALPHA", 1, 10}};
        stage(probe, input);
        pump(probe);
        unit = probe.dispatcher.resources().batchBytes;
        REQUIRE(unit != 0);
    }

    for (unsigned boundary = 0; boundary < 2; ++boundary) {
        auto bounds = limits();
        bounds.batchOwners = 2;

        if (boundary == 0) {
            bounds.batchResidentBytes = unit;
        } else {
            bounds.residentBytes = fixed + unit;
        }

        auto f = fixture(bounds);
        (void)add(f, u"ALPHA");
        d::OwnedBatch retained;
        f.consumer->action = [&](const d::BorrowedBatch &batch) {
            retained = batch.retain();
        };
        const Input input[]{{0, u"ALPHA", 1, 10}};
        stage(f, input);
        pump(f);
        CHECK(f.dispatcher.resources().batchOwners == 1);
        CHECK(f.dispatcher.resources().batchBytes == unit);
        CHECK(f.dispatcher.resources().fixedReservationBytes + f.dispatcher.resources().batchBytes <=
              bounds.residentBytes);
        auto copiedLease = retained;
        CHECK(f.dispatcher.resources().batchOwners == 1);
        stage(f, input);
        pump(f);
        REQUIRE(f.handle.terminal());
        CHECK(f.handle.terminal()->primary == Error::RESOURCE_LIMIT);
        CHECK(f.handle.terminal()->counts.discardedRecords == 1);
        CHECK(f.dispatcher.resources().batchOwners == 1);
        retained = {};
        CHECK(f.dispatcher.resources().batchOwners == 1);
        copiedLease = {};
        CHECK(f.dispatcher.resources().batchOwners == 0);
    }
}

TEST_CASE("client zero ordinary capacities never become unlimited or consume reserved stop controls") {
    for (unsigned dimension = 0; dimension < 3; ++dimension) {
        auto bounds = limits();

        if (dimension == 0) {
            bounds.commandSlots = 0;
        }

        if (dimension == 1) {
            bounds.resultSlots = 0;
        }

        if (dimension == 2) {
            bounds.commandEntries = 0;
        }

        auto f = fixture(bounds);
        auto submit = submitTurn();
        const c::SubscriptionRequest request{{f.local.recordContext(0), u"ALPHA"}, {}};
        auto admission = f.handle.add(std::span(&request, 1), submit);
        REQUIRE_FALSE(admission);
        CHECK(admission.error() == (dimension == 0   ? Error::COMMAND_LIMIT
                                    : dimension == 1 ? Error::RESULT_LIMIT
                                                     : Error::RESOURCE_LIMIT));
        REQUIRE(f.handle.requestStop(d::StopMode::GRACEFUL));
        CHECK_FALSE(f.handle.terminal());
        auto limited = turn();
        limited.operations = 0;
        CHECK(f.dispatcher.progress(limited).error() == Error::WORK_LIMIT);
        CHECK_FALSE(f.handle.terminal());
        pump(f);
        REQUIRE(f.handle.terminal());
        CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::DRAINED);
    }
}

TEST_CASE("client composition never dispatches the completed decoder prefix of a failed live frame") {
    auto f = fixture();
    (void)add(f, u"ALPHA");
    const Input first[]{{0, u"ALPHA", 1, 10}};
    stage(f, first);
    pump(f);
    CHECK(f.consumer->calls == 1);
    const Input failed[]{{0, u"ALPHA", 2, 20}, {0, u"ALPHA", 3, 30}};
    auto malformed = data(failed);
    malformed.pop_back();
    malformed[0] = static_cast<std::byte>(std::to_integer<unsigned>(malformed[0]) - 1);
    auto inputWork = liveTurn();
    auto decoded = f.live.feed(malformed, inputWork);
    CHECK(decoded.status == p::LiveStatus::FAILED);
    CHECK_FALSE(decoded.event);
    CHECK(f.live.diagnostic().error == p::LiveError::MALFORMED);
    CHECK(f.dispatcher.counts().acceptedRecords == 1);
    CHECK(f.dispatcher.counts().completedRecords == 1);
    REQUIRE(f.handle.requestStop(d::StopMode::ABORT));
    pump(f);
    REQUIRE(f.handle.terminal());
    CHECK(f.handle.terminal()->outcome == d::DispatchOutcome::ABORTED);
    CHECK(f.handle.terminal()->counts.completedRecords == 1);
    CHECK(f.consumer->calls == 1);
    // The composing caller retains the actual driver failure; local terminal is no socket/parser certificate.
    CHECK(f.live.diagnostic().error == p::LiveError::MALFORMED);
}
