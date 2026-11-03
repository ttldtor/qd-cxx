// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/mux/queue.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::collectors;
namespace e = org::ttldtor::qdcxx::encoding;
namespace m = org::ttldtor::qdcxx::mux;
namespace q = org::ttldtor::qdcxx::events;

/** Explicit protocol turn.
 * @return Finite admission credits.
 */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
} /** Configure complete native live admission, independent of tape lifetime quotas.
   * @return Explicit supported binary profile limits.
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
    result.eventBytes = 262144;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;
    result.data.processedRecords = 256;
    result.data.emittedRecords = 256;
    result.data.emittedValues = 2048;

    return result;
}

/** Build the selected production-observation record shape with fresh native provenance.
 * @return Validated immutable schema with explicit high/low raw time words.
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

/** Append exact UTFString bytes.
 * @param bytes Test-owned payload.
 * @param value Borrowed non-null UTF-16 text.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 8192> buffer{};
    const auto result = e::writeUtfString(value, buffer, {8192, 1024});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap a complete canonical envelope.
 * @param type Exact message identity.
 * @param payload Complete message payload.
 * @return Owned wire frame.
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

/** Prepare modern peer capability metadata, without clocks or sockets.
 * @return Canonical DESCRIBE_PROTOCOL frame advertising STREAM_DATA.
 */
std::vector<std::byte> ready() {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    number(payload, 0);
    number(payload, 3);
    number(payload, 15);
    text(payload, u"STREAM_DATA");
    number(payload, 0);
    number(payload, 10);
    text(payload, u"TICKER_DATA");
    number(payload, 0);
    number(payload, 20);
    text(payload, u"HISTORY_DATA");
    number(payload, 0);
    number(payload, 0);

    return frame(1, payload);
}

/** Build one exact peer description with an explicit sparse sender identity.
 * @param id Directional peer sender ID.
 * @param local Local record identity selecting the exact name.
 * @return Complete canonical DESCRIBE_RECORDS frame.
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

/** One synthetic source tuple shared with production collector observations. */
struct Input {
    std::int32_t record;        ///< Local record identity, mapped explicitly to a peer sender ID.
    std::u16string_view symbol; ///< Exact requested or unsolicited symbol.
    std::int64_t time;          ///< Raw signed64 record time, never milliseconds.
    std::int32_t value;         ///< Compact field payload.
    std::int32_t flags = 0;     ///< Exact raw source flags.
};

/** Compose selected source tuples through actual symbol/compact codecs.
 * @param inputs Complete ordered records.
 * @param type Exact data family, STREAM_DATA by default.
 * @return Whole native admission frame.
 */
std::vector<std::byte> data(std::span<const Input> inputs, std::int32_t type = 20) {
    std::vector<std::byte> payload;

    for (const auto &input : inputs) {
        payload.push_back(std::byte{0xf9});
        number(payload, input.flags);
        payload.push_back(std::byte{0xfd});
        number(payload, static_cast<std::int64_t>(input.symbol.size()));

        for (const auto unit : input.symbol) {
            std::array<std::byte, 4> bytes{};
            const auto written = e::writeUtfChar(unit, bytes).written;
            payload.insert(payload.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(written));
        }

        number(payload, input.record == 0 ? 41 : 7);

        if ((input.flags & 2) != 0) {
            REQUIRE(input.value == 0); // QTP REMOVE materializes ordinary defaults.
            number(payload, input.time);
            continue;
        }

        const auto bits = std::bit_cast<std::uint64_t>(input.time);
        number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
        number(payload, std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
        number(payload, input.value);
    }

    return frame(type, payload);
}

/** Admit a whole expected-success frame using the real live protocol driver.
 * @param session Native complete-frame owner.
 * @param bytes Complete input envelope.
 * @return Admitted immutable owner, never a manually fabricated view.
 */
p::LiveEvent admit(p::LiveSession &session, std::span<const std::byte> bytes) {
    auto work = liveTurn();
    auto result = session.feed(bytes, work);
    INFO("live error=", static_cast<int>(session.diagnostic().error), " stage=", session.diagnostic().stage);
    REQUIRE(result.status == p::LiveStatus::EVENT);
    REQUIRE(result.accepted == bytes.size());
    REQUIRE(result.event);

    return std::move(*result.event);
}

/** Prepare a real epoch using the selected local schema identity.
 * @param source Immutable schema, copied with preserved provenance for transfer.
 * @param bounds Explicit actual admission profile, including the large quantum case.
 * @return Ready native live session with both complete sender descriptions.
 */
p::LiveSession session(const c::LocalSchema &source, p::LiveLimits bounds = liveLimits()) {
    auto copied = source;
    const p::OutgoingRecord declarations[]{
        {41, 0, u"Oracle#ONE", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}},
        {7, 1, u"Oracle#TWO", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}}};
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(source, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copied), std::move(*writer), bounds, work);
    REQUIRE(result);
    (void)admit(*result, ready());
    (void)admit(*result, description(41, 0));
    (void)admit(*result, description(7, 1));

    return std::move(*result);
}

/** Explicit independent owner turn; no production defaults.
 * @return Work for one deterministic test turn.
 */
m::QueueWork turn() {
    return {100000,
            1000000000,
            1000000000,
            1000000000,
            1000000000,
            10000,
            10000,
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000},
            {100000, 1000000000, 1000000000, 1000000000}};
}

/** Explicit producer turn.
 * @return Finite copy credits.
 */
m::QueueSubmitWork submitTurn() {
    return {10000, 1000000, 1000000, 1000000};
}

/** Actual desired capacities.
 * @return Bounded old+candidate scope.
 */
c::SubscriptionLimits desiredLimits() {
    return {16, 32, 128, 4096, 64, 8192, 64};
}

/** Actual MUX limits.
 * @return Four fixed clients and finite snapshots.
 */
m::MuxLimits muxLimits() {
    return {16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
}

/** Explicit aggregate queue profile.
 * @return Reserved capacity including retired generations.
 */
m::QueueLimits queueLimits() {
    return {4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
}

/** Explicit individual queue profile.
 * @return Credits for immutable records/ports/composites.
 */
m::ClientLimits clientLimits() {
    return {32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096};
}

/** Actual callback observes scalar values without retaining borrowed spans. */
struct Recorder : m::MuxConsumer {
    std::vector<std::int64_t> values;               ///< Deep copied observation only.
    std::vector<std::int32_t> flags;                ///< Produced flags.
    std::vector<std::optional<std::int64_t>> times; ///< Actual optional raw index.
    bool pause = false, throws = false;             ///< Controlled finite user behavior.
    std::function<void()> action;                   ///< Optional caller-created callback action.
    std::vector<m::OwnedMuxBatch> retained;         ///< Explicit owning values.
    bool retain = false;                            ///< Hold copies after return.

    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        for (const auto &r : batch.records()) {
            REQUIRE(r.typedValues.size() == 3);
            values.push_back(r.typedValues[2].raw);
            flags.push_back(r.eventFlags);
            times.push_back(r.rawTime);
        }

        if (retain)
            retained.push_back(batch.retain());

        if (action)
            action();

        if (throws)
            throw std::runtime_error("controlled");

        return pause ? m::BatchAction::PAUSE : m::BatchAction::CONTINUE;
    }
};

/** Independent source plus one family owner. */
struct Fixture {
    c::LocalSchema schema;
    p::LiveSession live;
    m::MuxQueue queue;
    c::SubscriptionFamily family;
};

/** Create an actual complete graph.
 * @param family Immutable contract.
 * @param bounds Aggregate queue maxima.
 * @return Real source/actual collector/queue composition.
 */
Fixture fixture(c::SubscriptionFamily family, m::QueueLimits bounds = queueLimits()) {
    auto local = schema();
    auto live = session(local);
    auto work = turn();
    std::expected<m::MuxQueue, m::QueueFailure> q = std::unexpected(m::QueueFailure{m::QueueError::INVALID_STATE, {}});

    if (family == c::SubscriptionFamily::STREAM)
        q = m::MuxQueue::createStream(local, muxLimits(), desiredLimits(), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                      bounds, work);
    else if (family == c::SubscriptionFamily::TICKER)
        q = m::MuxQueue::createTicker(local, muxLimits(),
                                      {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desiredLimits()}, {false, 100},
                                      bounds, work);
    else
        q = m::MuxQueue::createHistory(
            local, muxLimits(),
            {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desiredLimits()},
            {false, 100, 10000}, bounds, work);

    REQUIRE(q);

    return {std::move(local), std::move(live), std::move(*q), family};
}

/** Activate empty desired callback registration.
 * @param f Actual graph.
 * @param consumer Caller callback.
 * @param limits Per-client maxima.
 * @return Public producer-safe handle.
 */
m::MuxHandle open(Fixture &f, const std::shared_ptr<Recorder> &consumer, m::ClientLimits limits = clientLimits()) {
    auto work = turn();
    auto result = f.queue.open(limits, consumer, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Pump finite deterministic turns until an observable condition.
 * @param f Actual graph.
 * @param ready Public condition.
 */
void until(Fixture &f, const std::function<bool()> &ready) {
    for (unsigned i = 0; i < 20000; ++i) {
        if (ready())
            return;

        auto work = turn();
        auto result = f.queue.progress(work);
        INFO("progress error=", result ? -1 : static_cast<int>(result.error().error));
        REQUIRE(result);
    }

    FAIL("bounded deterministic owner progress did not reach condition");
}

/** Apply desired interest and synchronize its actual view.
 * @param f Actual graph.
 * @param h Public registration.
 * @param bound Inclusive raw HISTORY bound.
 */
void add(Fixture &f, m::MuxHandle &h, std::int64_t bound = 0) {
    const c::SubscriptionRequest request{{f.schema.recordContext(0), u"ALPHA"},
                                         f.family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{bound}
                                                                                    : std::nullopt};
    auto sw = submitTurn();
    auto ticket = h.add(std::span(&request, 1), sw);
    REQUIRE(ticket);
    until(f, [&] {
        return ticket->result().has_value();
    });
    REQUIRE(ticket->result()->status == m::CommandStatus::APPLIED);
    until(f, [&] {
        return h.state() == m::QueueClientState::ACTIVE;
    });
}

/** Admit one complete actual source frame after fair owner readiness.
 * @param f Actual graph.
 * @param records Ordered source tuples.
 */
void send(Fixture &f, std::initializer_list<Input> records) {
    const auto type =
        f.family == c::SubscriptionFamily::STREAM ? 15 : (f.family == c::SubscriptionFamily::TICKER ? 10 : 20);
    auto event = admit(f.live, data(std::span<const Input>(records.begin(), records.size()), type));

    for (unsigned i = 0; i < 20000; ++i) {
        auto w = turn();
        auto result = f.queue.admit(std::move(event), w);

        if (result)
            return;

        REQUIRE(result.error().error == m::QueueError::BUSY);
        auto next = f.queue.progress(w);
        REQUIRE(next);
    }

    FAIL("source admission never became ready");
}

/** Observe a scoped actual HISTORY range.
 * @param f Graph.
 * @param h Current registration.
 * @return Detached actual diagnostics plus downstream guards.
 */
m::QueueRangeStatus range(Fixture &f, const m::MuxHandle &h) {
    auto w = turn();
    auto r = f.queue.rangeStatus(h, {f.schema.recordContext(0), u"ALPHA"}, w);
    REQUIRE(r);

    return std::move(*r);
}
} // namespace

TEST_CASE("MUX queue STREAM duplicates reach both clients and public generation reservations are explicit") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    CHECK(f.queue.resources().generations == 0);
    CHECK(f.queue.aggregateEntries().empty());
    CHECK(f.queue.revision() == 4);
    auto a = std::make_shared<Recorder>();
    auto b = std::make_shared<Recorder>();
    auto first = open(f, a);
    auto second = open(f, b);
    add(f, first);
    add(f, second);
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 1, 10, 0}, {0, u"BETA", 1, 99, 0}, {0, u"ALPHA", 2, 11, 0}});
    until(f, [&] {
        return a->values.size() == 3 && b->values.size() == 3;
    });
    CHECK(a->values == std::vector<std::int64_t>{10, 10, 11});
    CHECK(b->values == a->values);
    CHECK(f.queue.resources().generations == 2);
}

TEST_CASE("MUX queue HISTORY complete cannot use old source bound before synchronization or unstaged TX") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto r = std::make_shared<Recorder>();
    auto h = open(f, r);
    add(f, h, 2);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return r->values.size() == 2;
    });
    CHECK(range(f, h).completeDeliveredRange);
    const c::SubscriptionRequest expanded{{f.schema.recordContext(0), u"ALPHA"}, 0};
    auto sw = submitTurn();
    auto ticket = h.replace(std::span(&expanded, 1), sw);
    REQUIRE(ticket);
    until(f, [&] {
        return ticket->result().has_value();
    });
    auto pending = range(f, h);
    REQUIRE(pending.source);
    CHECK(pending.source->requestedFrom == 2);
    CHECK(pending.requestedFrom == 0);
    CHECK_FALSE(pending.mappingCurrent);
    CHECK_FALSE(pending.completeDeliveredRange);
    until(f, [&] {
        return h.state() == m::QueueClientState::ACTIVE;
    });
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    until(f, [&] {
        return range(f, h).completeDeliveredRange;
    });
    send(f, {{0, u"ALPHA", 3, 31, 1}});
    auto unstage = range(f, h);
    CHECK(unstage.acceptedUnstagedData);
    CHECK_FALSE(unstage.completeDeliveredRange);
    REQUIRE(unstage.source);
    CHECK(unstage.source->completeRequestedRange);
}

TEST_CASE("MUX queue reset discards old unstaged data and retries actual TICKER global ACK") {
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto r = std::make_shared<Recorder>();
    auto h = open(f, r);
    add(f, h);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return r->values.size() == 1;
    });
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    auto work = turn();
    auto request = f.queue.resetSource(work);
    REQUIRE(request);
    until(f, [&] {
        return f.queue.resetStatus().phase == m::ResetPhase::GLOBAL_ACK_PENDING;
    });
    auto before = f.queue.resetStatus();
    REQUIRE(before.sourceCommitted);
    CHECK(before.discardedPendingFrames == 1);
    CHECK(before.discardedPendingRecords == 1);
    REQUIRE(h.resetNotice());
    work = turn();
    work.ticker.operations = 0;
    auto blocked = f.queue.progress(work);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().error == m::QueueError::WORK_LIMIT);
    auto after = f.queue.resetStatus();
    CHECK(after.phase == m::ResetPhase::GLOBAL_ACK_PENDING);
    CHECK(after.actualSourceSequence == before.actualSourceSequence);
    CHECK(r->values == std::vector<std::int64_t>{10});
    auto notice = h.resetNotice();
    REQUIRE(notice);
    CHECK(h.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return f.queue.resetStatus().phase == m::ResetPhase::NONE && !h.resetNotice() &&
               h.state() == m::QueueClientState::ACTIVE;
    });
    send(f, {{0, u"ALPHA", 3, 30, 0}});
    until(f, [&] {
        return r->values.size() == 2;
    });
    CHECK(r->values == std::vector<std::int64_t>{10, 30});
}

TEST_CASE("MUX queue empty-view retirement charges fixed mapping visits before source closure") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    CHECK(h.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
    bool sourceClosed = false;

    for (unsigned n = 0; n < 40; ++n) {
        auto work = turn();
        work.visits = 10;
        auto step = f.queue.progress(work);

        if (!step)
            CHECK(step.error().error == m::QueueError::WORK_LIMIT);

        work = turn();
        auto status = f.queue.rangeStatus(h, {f.schema.recordContext(0), u"ALPHA"}, work);
        REQUIRE_FALSE(status);

        if (status.error().error == m::QueueError::INVALID_CLIENT)
            sourceClosed = true;
    }

    // The configured32 mapping slots must be reset even though the desired map is empty.
    CHECK_FALSE(sourceClosed);
    CHECK_FALSE(h.terminal());
}

TEST_CASE("MUX queue paused STREAM overflow retires only the slow client and reserved stop stays visible") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto fast = std::make_shared<Recorder>();
    auto slow = std::make_shared<Recorder>();
    slow->pause = true;
    auto a = open(f, fast);
    auto small = clientLimits();
    small.dataItems = 4;
    auto b = open(f, slow, small);
    add(f, a);
    add(f, b);
    send(f, {{0, u"ALPHA", 1, 1, 0}});
    until(f, [&] {
        return fast->values.size() == 1 && b.state() == m::QueueClientState::PAUSED;
    });
    send(f, {{0, u"ALPHA", 2, 2, 0}, {0, u"ALPHA", 3, 3, 0}, {0, u"ALPHA", 4, 4, 0}, {0, u"ALPHA", 5, 5, 0}});
    until(f, [&] {
        return fast->values.size() == 5;
    });
    auto charged = f.queue.resources(b);
    REQUIRE(charged);
    CHECK(charged->queuedRecords == 4);
    CHECK(charged->recordOwners == 4);
    send(f, {{0, u"ALPHA", 6, 6, 0}});
    until(f, [&] {
        return fast->values.size() == 6 && b.terminal().has_value();
    });
    REQUIRE(b.terminal()->primary);
    CHECK(b.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
    CHECK(b.terminal()->outcome == m::TerminalOutcome::FAILED);
    CHECK(fast->values == std::vector<std::int64_t>{1, 2, 3, 4, 5, 6});
    CHECK(slow->values == std::vector<std::int64_t>{1});
    CHECK(a.state() == m::QueueClientState::ACTIVE);
    CHECK(b.requestStop(m::StopMode::ABORT) == m::ControlAdmission::CLOSED);
}

TEST_CASE("MUX queue reserved resume restarts a paused TICKER registration") {
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto consumer = std::make_shared<Recorder>();
    consumer->pause = true;
    auto h = open(f, consumer);
    add(f, h);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return h.state() == m::QueueClientState::PAUSED;
    });
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        auto r = f.queue.resources(h);

        return r && r->queuedRecords == 1;
    });
    consumer->pause = false;
    CHECK(h.requestResume() == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return consumer->values.size() == 2;
    });
    CHECK(consumer->values == std::vector<std::int64_t>{10, 20});
    CHECK(h.state() == m::QueueClientState::ACTIVE);
}

TEST_CASE("MUX queue retained composite quota and retired generation keep their reservations") {
    auto qlimits = queueLimits();
    qlimits.clients = 1;
    auto f = fixture(c::SubscriptionFamily::STREAM, qlimits);
    auto consumer = std::make_shared<Recorder>();
    consumer->retain = true;
    auto small = clientLimits();
    small.batchOwners = 1;
    auto h = open(f, consumer, small);
    add(f, h);
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return consumer->retained.size() == 1;
    });
    auto before = f.queue.resources(h);
    REQUIRE(before);
    CHECK(before->recordOwners == 2);
    CHECK(before->batchOwners == 1);
    send(f, {{0, u"ALPHA", 3, 30, 0}});
    until(f, [&] {
        return h.terminal().has_value();
    });
    CHECK(h.terminal()->outcome == m::TerminalOutcome::FAILED);
    auto saved = consumer->retained.front();
    consumer->retained.clear();
    h = {};
    auto work = turn();
    auto blocked = f.queue.open(small, consumer, work);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().error == m::QueueError::RESOURCE_LIMIT);
    std::atomic<bool> valid{false};
    {
        std::optional<m::MuxQueue> owner(std::move(f.queue));
        owner.reset();
    }
    std::thread release([batch = std::move(saved), &valid]() mutable {
        auto copy = batch;
        valid = copy.records().size() == 2 && copy.records()[1].typedValues[2].raw == 20;
        batch = {};
        copy = {};
    });
    release.join();
    CHECK(valid.load());
}

TEST_CASE("MUX queue held old ports and results fence reuse until actual clear and closure") {
    auto qlimits = queueLimits();
    qlimits.clients = 1;
    auto f = fixture(c::SubscriptionFamily::HISTORY, qlimits);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    const c::SubscriptionRequest request{{f.schema.recordContext(0), u"ALPHA"}, 2};
    auto sw = submitTurn();
    auto ticket = h.replace(std::span(&request, 1), sw);
    REQUIRE(ticket);
    CHECK(h.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return h.terminal().has_value();
    });
    REQUIRE(ticket->result());
    CHECK(ticket->result()->status == m::CommandStatus::CANCELLED_CLOSING);
    sw = submitTurn();
    CHECK_FALSE(h.add(std::span(&request, 1), sw));
    auto work = turn();
    CHECK_FALSE(f.queue.open(clientLimits(), consumer, work));
    h = {};

    for (unsigned i = 0; i < 40; ++i) {
        work = turn();
        REQUIRE(f.queue.progress(work));
    }

    CHECK(f.queue.resources().generations == 1);
    ticket = {};
    until(f, [&] {
        return f.queue.resources().generations == 0;
    });
    auto next = open(f, consumer);
    add(f, next, 2);
    send(f, {{0, u"ALPHA", 3, 33, 4}, {0, u"ALPHA", 2, 22, 0}});
    until(f, [&] {
        return consumer->values.size() == 2;
    });
    CHECK(consumer->values == std::vector<std::int64_t>{33, 22});
}

TEST_CASE("MUX queue throwing callback isolates one client without retry or cause overwrite") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto bad = std::make_shared<Recorder>();
    bad->throws = true;
    auto good = std::make_shared<Recorder>();
    auto a = open(f, bad);
    auto b = open(f, good);
    add(f, a);
    add(f, b);
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return a.terminal().has_value() && good->values.size() == 2;
    });
    REQUIRE(a.terminal()->primary);
    CHECK(a.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
    CHECK(bad->values.size() == 2);
    send(f, {{0, u"ALPHA", 3, 30, 0}});
    until(f, [&] {
        return good->values.size() == 3;
    });
    CHECK(bad->values.size() == 2);
    CHECK(b.state() == m::QueueClientState::ACTIVE);
}

TEST_CASE("MUX queue callback self-remove preserves the complete borrowed batch") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    std::optional<m::MuxCommandTicket> removal;
    consumer->action = [&] {
        if (removal)
            return;

        const c::SubscriptionKey key{f.schema.recordContext(0), u"ALPHA"};
        auto sw = submitTurn();
        auto t = h.remove(std::span(&key, 1), sw);
        REQUIRE(t);
        removal = std::move(*t);
        CHECK_FALSE(removal->result());
    };
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return removal && removal->result().has_value();
    });
    CHECK(consumer->values == std::vector<std::int64_t>{10, 20});
    consumer->action = {};
    send(f, {{0, u"ALPHA", 3, 30, 0}});

    for (unsigned i = 0; i < 100; ++i) {
        auto w = turn();
        REQUIRE(f.queue.progress(w));
    }

    CHECK(consumer->values.size() == 2);
}

TEST_CASE("MUX queue callback stop and recursive progress do not self-wait") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    consumer->action = [&] {
        auto w = turn();
        auto recursive = f.queue.progress(w);
        REQUIRE_FALSE(recursive);
        CHECK(recursive.error().error == m::QueueError::REENTRANT);
        CHECK(h.requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
    };
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return h.terminal().has_value();
    });
    CHECK(h.terminal()->outcome == m::TerminalOutcome::DRAINED);
    CHECK(consumer->values.size() == 2);
}

TEST_CASE("MUX queue HISTORY global reset ACK uses its actual no-work signature") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    until(f, [&] {
        return range(f, h).completeDeliveredRange;
    });
    auto work = turn();
    REQUIRE(f.queue.resetSource(work));
    until(f, [&] {
        return f.queue.resetStatus().phase == m::ResetPhase::GLOBAL_ACK_PENDING;
    });
    work = turn();
    work.history = {0, 0, 0, 0};
    auto ack = f.queue.progress(work);
    REQUIRE(ack);
    CHECK(f.queue.resetStatus().globalAcknowledged);
    CHECK(f.queue.resetStatus().phase == m::ResetPhase::NONE);
    auto notice = h.resetNotice();
    REQUIRE(notice);
    CHECK(h.acknowledgeReset(notice->sequence + 1) == m::ControlAdmission::STALE);
    CHECK(h.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return h.state() == m::QueueClientState::ACTIVE;
    });
    CHECK_FALSE(range(f, h).completeDeliveredRange);
}

TEST_CASE("MUX queue global terminal retains callback failure after its public generation retires") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto bad = std::make_shared<Recorder>();
    bad->throws = true;
    auto good = std::make_shared<Recorder>();
    auto a = open(f, bad);
    auto b = open(f, good);
    add(f, a);
    add(f, b);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return a.terminal().has_value() && good->values.size() == 1;
    });
    a = {};
    until(f, [&] {
        return f.queue.resources().generations == 1 && b.state() == m::QueueClientState::ACTIVE;
    });
    f.queue.requestStop(m::StopMode::GRACEFUL);
    until(f, [&] {
        return f.queue.terminal().has_value();
    });
    CHECK(f.queue.terminal()->outcome == m::TerminalOutcome::FAILED);
    REQUIRE(f.queue.terminal()->primary);
    CHECK(f.queue.terminal()->primary->error == m::QueueError::CALLBACK_FAILURE);
    CHECK(f.queue.terminal()->failedRegistrationCount == 1);
}

TEST_CASE("MUX queue repeated stop preserves published terminal state for every family") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        for (const auto mode : {m::StopMode::GRACEFUL, m::StopMode::ABORT}) {
            INFO("family=", static_cast<int>(family), " initial mode=", static_cast<int>(mode));
            auto f = fixture(family);
            f.queue.requestStop(mode);
            until(f, [&] {
                return f.queue.terminal().has_value();
            });
            const auto before = *f.queue.terminal();

            for (const auto repeated : {m::StopMode::GRACEFUL, m::StopMode::ABORT, m::StopMode::GRACEFUL}) {
                f.queue.requestStop(repeated);
                CHECK(f.queue.state() == m::QueueState::TERMINAL);
                auto work = turn();
                const auto progress = f.queue.progress(work);
                REQUIRE(progress);
                CHECK(progress->state == m::QueueState::TERMINAL);
                CHECK(f.queue.state() == m::QueueState::TERMINAL);
                REQUIRE(f.queue.terminal());
                const auto after = *f.queue.terminal();
                CHECK(after.outcome == before.outcome);
                CHECK(after.hadGap == before.hadGap);
                CHECK(after.failedRegistrationCount == before.failedRegistrationCount);
                CHECK(after.counts.acceptedRecords == before.counts.acceptedRecords);
                CHECK(after.counts.callbackStartedBatches == before.counts.callbackStartedBatches);
                CHECK(after.primary.has_value() == before.primary.has_value());
                CHECK(after.secondary.has_value() == before.secondary.has_value());
            }
        }
    }
}

TEST_CASE("MUX queue repeated stop retains a callback failure after callback-side abort") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto consumer = std::make_shared<Recorder>();
    auto handle = open(f, consumer);
    add(f, handle);
    consumer->throws = true;
    consumer->action = [&] {
        f.queue.requestStop(m::StopMode::GRACEFUL);
        f.queue.requestStop(m::StopMode::ABORT);
        f.queue.requestStop(m::StopMode::GRACEFUL);
    };
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return f.queue.terminal().has_value();
    });
    const auto before = *f.queue.terminal();
    REQUIRE(before.primary);
    CHECK(before.primary->error == m::QueueError::CALLBACK_FAILURE);
    CHECK(before.outcome == m::TerminalOutcome::FAILED);
    const auto delivered = consumer->values.size();

    for (const auto mode : {m::StopMode::GRACEFUL, m::StopMode::ABORT}) {
        f.queue.requestStop(mode);
        CHECK(f.queue.state() == m::QueueState::TERMINAL);
        auto work = turn();
        REQUIRE(f.queue.progress(work));
        REQUIRE(f.queue.terminal());
        const auto after = *f.queue.terminal();
        REQUIRE(after.primary);
        CHECK(after.primary->error == before.primary->error);
        CHECK(after.outcome == before.outcome);
        CHECK(after.failedRegistrationCount == before.failedRegistrationCount);
        CHECK(after.counts.callbackFailedRecords == before.counts.callbackFailedRecords);
        CHECK(after.counts.discarded == before.counts.discarded);
        CHECK(after.secondary.has_value() == before.secondary.has_value());
        CHECK(consumer->values.size() == delivered);
    }
}

TEST_CASE("MUX queue global graceful result cannot hide a paused-client forced abort") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto paused = std::make_shared<Recorder>();
    paused->pause = true;
    auto h = open(f, paused);
    add(f, h);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return h.state() == m::QueueClientState::PAUSED;
    });
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        auto r = f.queue.resources(h);

        return r && r->queuedRecords == 1;
    });
    f.queue.requestStop(m::StopMode::GRACEFUL);
    until(f, [&] {
        return f.queue.terminal().has_value();
    });
    REQUIRE(h.terminal());
    CHECK(h.terminal()->outcome == m::TerminalOutcome::ABORTED);
    CHECK(f.queue.terminal()->outcome == m::TerminalOutcome::ABORTED);
    CHECK(f.queue.terminal()->hadGap);
}

TEST_CASE("MUX queue graceful stop drains the admitted STREAM prefix before and during routing") {
    for (const bool partialRouting : {false, true}) {
        INFO("partial routing=", partialRouting);
        auto bounds = queueLimits();
        bounds.routeRecords = 2;
        bounds.batchRecords = 1;
        auto f = fixture(c::SubscriptionFamily::STREAM, bounds);
        auto first = std::make_shared<Recorder>();
        auto keeper = std::make_shared<Recorder>();
        keeper->retain = true;
        auto perClient = clientLimits();
        perClient.batchRecords = 1;
        auto firstHandle = open(f, first, perClient);
        auto keeperHandle = open(f, keeper, perClient);
        add(f, firstHandle);
        add(f, keeperHandle);
        send(f, {{0, u"ALPHA", 1, 10, 0},
                 {0, u"BETA", 1, 99, 0},
                 {0, u"ALPHA", 2, 20, 0},
                 {0, u"ALPHA", 3, 30, 0},
                 {0, u"BETA", 2, 98, 0},
                 {0, u"ALPHA", 4, 40, 0}});
        REQUIRE(f.queue.resources().pendingRecords == 6);

        if (partialRouting) {
            until(f, [&] {
                return f.queue.resources().pendingRecords == 2;
            });
            REQUIRE(f.queue.resources().pendingRecords == 2);
        } else {
            CHECK(first->values.empty());
            CHECK(keeper->values.empty());
        }

        f.queue.requestStop(m::StopMode::GRACEFUL);
        const auto type = 15;
        const std::array<Input, 1> later{{{0, u"ALPHA", 5, 50, 0}}};
        auto unadmitted = admit(f.live, data(later, type));
        auto work = turn();
        auto refused = f.queue.admit(std::move(unadmitted), work);
        REQUIRE_FALSE(refused);
        CHECK(refused.error().error == m::QueueError::CLOSING);
        CHECK(unadmitted.records().size() == 1);
        until(f, [&] {
            return f.queue.terminal().has_value();
        });
        REQUIRE(firstHandle.terminal());
        REQUIRE(keeperHandle.terminal());
        const auto terminal = *f.queue.terminal();
        CHECK(terminal.outcome == m::TerminalOutcome::DRAINED);
        CHECK_FALSE(terminal.hadGap);
        CHECK(firstHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(keeperHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(first->values == std::vector<std::int64_t>{10, 20, 30, 40});
        REQUIRE(first->values.size() == 4);
        CHECK(keeper->values == first->values);
        CHECK(terminal.counts.acceptedRecords == 6);
        CHECK(terminal.counts.collectorAcknowledged == 4);
        CHECK(terminal.counts.queueAdmitted == 8);
        CHECK(terminal.counts.callbackCompletedRecords == 8);
        CHECK(terminal.counts.discarded == 0);
        CHECK(terminal.counts.sourceRecordsDiscarded == 0);
        CHECK(f.queue.resources().pendingRecords == 0);
        REQUIRE(keeper->retained.size() == 4);
        CHECK(f.queue.resources().batchOwners == 4);
        CHECK(f.queue.resources().dataItems == 4);

        for (std::size_t i = 0; i < keeper->retained.size(); ++i) {
            REQUIRE(keeper->retained[i].records().size() == 1);
            CHECK(keeper->retained[i].records()[0].typedValues[2].raw == first->values[i]);
        }

        keeper->retained.clear();
        CHECK(f.queue.resources().batchOwners == 0);
        CHECK(f.queue.resources().dataItems == 0);
        CHECK(f.queue.terminal()->counts.callbackCompletedRecords == terminal.counts.callbackCompletedRecords);
    }
}

TEST_CASE("MUX queue graceful admitted prefix keeps healthy clients independent of a paused or failed peer") {
    for (const bool callbackFailure : {false, true}) {
        INFO("callback failure=", callbackFailure);
        auto bounds = queueLimits();
        bounds.routeRecords = 2;
        bounds.batchRecords = 1;
        auto f = fixture(c::SubscriptionFamily::STREAM, bounds);
        auto first = std::make_shared<Recorder>();
        auto second = std::make_shared<Recorder>();
        auto troubled = std::make_shared<Recorder>();
        troubled->pause = !callbackFailure;
        troubled->throws = callbackFailure;
        auto perClient = clientLimits();
        perClient.batchRecords = 1;
        auto firstHandle = open(f, first, perClient);
        auto secondHandle = open(f, second, perClient);
        auto troubledHandle = open(f, troubled, perClient);
        add(f, firstHandle);
        add(f, secondHandle);
        add(f, troubledHandle);
        send(f, {{0, u"ALPHA", 1, 10, 0},
                 {0, u"ALPHA", 2, 20, 0},
                 {0, u"ALPHA", 3, 30, 0},
                 {0, u"ALPHA", 4, 40, 0},
                 {0, u"ALPHA", 5, 50, 0},
                 {0, u"ALPHA", 6, 60, 0}});
        f.queue.requestStop(m::StopMode::GRACEFUL);
        until(f, [&] {
            return f.queue.terminal().has_value();
        });
        REQUIRE(firstHandle.terminal());
        REQUIRE(secondHandle.terminal());
        REQUIRE(troubledHandle.terminal());
        CHECK(firstHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(secondHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(first->values == std::vector<std::int64_t>{10, 20, 30, 40, 50, 60});
        CHECK(second->values == first->values);
        CHECK(troubled->values == std::vector<std::int64_t>{10});
        CHECK(troubledHandle.terminal()->hadGap);
        const auto terminal = *f.queue.terminal();
        CHECK(terminal.counts.acceptedRecords == 6);
        CHECK(terminal.hadGap);
        CHECK(terminal.counts.discarded > 0);
        CHECK(f.queue.resources().pendingRecords == 0);

        if (callbackFailure) {
            CHECK(troubledHandle.terminal()->outcome == m::TerminalOutcome::FAILED);
            REQUIRE(terminal.primary);
            CHECK(terminal.primary->error == m::QueueError::CALLBACK_FAILURE);
            CHECK(terminal.outcome == m::TerminalOutcome::FAILED);
        } else {
            CHECK(troubledHandle.terminal()->outcome == m::TerminalOutcome::ABORTED);
            CHECK(terminal.outcome == m::TerminalOutcome::ABORTED);
        }
    }
}

TEST_CASE("MUX queue graceful prefix preserves TICKER and HISTORY projections across staging") {
    for (const auto family : {c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        for (const bool staged : {false, true}) {
            INFO("family=", static_cast<int>(family), " staged=", staged);
            auto bounds = queueLimits();
            bounds.batchRecords = 1;
            auto f = fixture(family, bounds);
            auto first = std::make_shared<Recorder>();
            auto keeper = std::make_shared<Recorder>();
            keeper->retain = true;
            auto perClient = clientLimits();
            perClient.batchRecords = 1;
            auto firstHandle = open(f, first, perClient);
            auto keeperHandle = open(f, keeper, perClient);
            add(f, firstHandle, 1);
            add(f, keeperHandle, 1);
            const bool history = family == c::SubscriptionFamily::HISTORY;
            constexpr std::int32_t SNAPSHOT_BEGIN = 4;
            constexpr std::int32_t SNAPSHOT_END = 8;
            send(f, {{0, u"ALPHA", 3, 30, history ? SNAPSHOT_BEGIN : 0},
                     {0, u"ALPHA", 2, 20, 0},
                     {0, u"ALPHA", 1, 10, history ? SNAPSHOT_END : 0}});

            if (staged) {
                until(f, [&] {
                    return f.queue.resources().pendingRecords == 0;
                });
            }

            REQUIRE(first->values.empty());
            REQUIRE(keeper->values.empty());
            f.queue.requestStop(m::StopMode::GRACEFUL);
            until(f, [&] {
                return f.queue.terminal().has_value();
            });
            REQUIRE(firstHandle.terminal());
            REQUIRE(keeperHandle.terminal());
            const auto terminal = *f.queue.terminal();
            const auto expected = history ? std::vector<std::int64_t>{30, 20, 10} : std::vector<std::int64_t>{10};
            CHECK(first->values == expected);
            CHECK(keeper->values == expected);
            CHECK(firstHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
            CHECK(keeperHandle.terminal()->outcome == m::TerminalOutcome::DRAINED);
            CHECK(terminal.outcome == m::TerminalOutcome::DRAINED);
            CHECK_FALSE(terminal.hadGap);
            CHECK(terminal.counts.acceptedRecords == 3);
            CHECK(terminal.counts.queueAdmitted == 2 * expected.size());
            CHECK(terminal.counts.callbackCompletedRecords == 2 * expected.size());
            CHECK(terminal.counts.discarded == 0);
            CHECK(terminal.counts.sourceRecordsDiscarded == 0);
            CHECK(f.queue.resources().pendingRecords == 0);
            REQUIRE(keeper->retained.size() == expected.size());
            CHECK(f.queue.resources().batchOwners == expected.size());

            for (std::size_t i = 0; i < expected.size(); ++i) {
                REQUIRE(keeper->retained[i].records().size() == 1);
                CHECK(keeper->retained[i].records()[0].typedValues[2].raw == expected[i]);
            }

            keeper->retained.clear();
            CHECK(f.queue.resources().batchOwners == 0);
            CHECK(f.queue.resources().dataItems == 0);
        }
    }
}

TEST_CASE("MUX queue graceful prefix escalation accounts unrouted source separately from client copies") {
    for (const bool partialRouting : {false, true}) {
        INFO("partial routing=", partialRouting);
        auto bounds = queueLimits();
        bounds.routeRecords = 2;
        auto f = fixture(c::SubscriptionFamily::STREAM, bounds);
        auto first = std::make_shared<Recorder>();
        auto second = std::make_shared<Recorder>();
        auto firstHandle = open(f, first);
        auto secondHandle = open(f, second);
        add(f, firstHandle);
        add(f, secondHandle);
        send(f, {{0, u"ALPHA", 1, 10, 0},
                 {0, u"ALPHA", 2, 20, 0},
                 {0, u"ALPHA", 3, 30, 0},
                 {0, u"ALPHA", 4, 40, 0},
                 {0, u"ALPHA", 5, 50, 0}});

        if (partialRouting) {
            until(f, [&] {
                return f.queue.resources().pendingRecords == 3;
            });
        }

        REQUIRE(first->values.empty());
        REQUIRE(second->values.empty());
        f.queue.requestStop(m::StopMode::GRACEFUL);
        f.queue.requestStop(m::StopMode::ABORT);
        until(f, [&] {
            return f.queue.terminal().has_value();
        });
        REQUIRE(firstHandle.terminal());
        REQUIRE(secondHandle.terminal());
        const auto terminal = *f.queue.terminal();
        CHECK(terminal.outcome == m::TerminalOutcome::ABORTED);
        CHECK(firstHandle.terminal()->outcome == m::TerminalOutcome::ABORTED);
        CHECK(secondHandle.terminal()->outcome == m::TerminalOutcome::ABORTED);
        CHECK(terminal.hadGap);
        CHECK(terminal.unknownEligibility);
        CHECK(terminal.counts.acceptedRecords == 5);
        CHECK(terminal.counts.sourceRecordsDiscarded == (partialRouting ? 3 : 5));
        CHECK(terminal.counts.discarded == (partialRouting ? 4 : 0));
        CHECK(terminal.counts.callbackStartedRecords == 0);
        CHECK(first->values.empty());
        CHECK(second->values.empty());
        CHECK(f.queue.resources().pendingRecords == 0);
        CHECK(f.queue.resources().dataItems == 0);
    }
}

TEST_CASE("MUX queue graceful prefix cannot report a missing HISTORY range as drained") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto receiver = std::make_shared<Recorder>();
    auto handle = open(f, receiver);
    add(f, handle, 0);
    constexpr std::int32_t SNAPSHOT_BEGIN = 4;
    send(f, {{0, u"ALPHA", 3, 30, SNAPSHOT_BEGIN}, {0, u"ALPHA", 2, 20, 0}});
    f.queue.requestStop(m::StopMode::GRACEFUL);
    until(f, [&] {
        return f.queue.terminal().has_value();
    });
    REQUIRE(handle.terminal());
    const auto terminal = *f.queue.terminal();
    CHECK(receiver->values == std::vector<std::int64_t>{30, 20});
    CHECK(handle.terminal()->outcome == m::TerminalOutcome::ABORTED);
    CHECK(terminal.outcome == m::TerminalOutcome::ABORTED);
    CHECK(terminal.hadGap);
    CHECK(terminal.counts.callbackCompletedRecords == 2);
    CHECK(terminal.counts.discarded == 0);
    CHECK(terminal.counts.sourceRecordsDiscarded == 0);
    CHECK(f.queue.resources().pendingRecords == 0);
}

namespace {
p::LiveSession genericSession(const c::LocalSchema &local) {
    std::vector<p::OutgoingRecord> declarations;

    for (const auto &r : local.records()) {
        p::OutgoingRecord declaration{17 + r.id, r.id, r.name, {}};

        for (std::size_t field = 0; field < r.fields.size(); ++field) {
            const auto lookup = std::ranges::find_if(r.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });
            REQUIRE(lookup != r.lookups.end());
            declaration.fields.push_back(
                {lookup->name, r.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        declarations.push_back(std::move(declaration));
    }

    auto copy = local;
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(local, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto live = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(), work);
    REQUIRE(live);
    (void)admit(*live, ready());

    return std::move(*live);
}

/** Copy actual generic raw values without business conversion.
 * @param fields Actual admitted tagged fields.
 * @return Deep owned exact payload.
 */
std::vector<c::FieldValue> copyFields(std::span<const p::LiveFieldValueView> fields) {
    std::vector<c::FieldValue> result;

    for (const auto &f : fields) {
        c::FieldValue v{f.type, f.raw};

        if (f.units) {
            v.units = std::u16string(*f.units);
        }

        if (f.bytes) {
            v.bytes = std::vector<std::byte>(f.bytes->begin(), f.bytes->end());
        }

        result.push_back(std::move(v));
    }

    return result;
}

/** Detach a projected value for the existing prepared event mapping APIs.
 * @param fields Exact complete projected fields, including extras.
 * @return Deep owned generic values with no protocol lease.
 */
std::vector<c::FieldValue> copyMuxFields(std::span<const m::MuxFieldValueView> fields) {
    std::vector<c::FieldValue> result;

    for (const auto &f : fields) {
        c::FieldValue v{f.type, f.raw};

        if (f.units) {
            v.units = std::u16string(*f.units);
        }

        if (f.bytes) {
            v.bytes = std::vector<std::byte>(f.bytes->begin(), f.bytes->end());
        }

        result.push_back(std::move(v));
    }

    return result;
}

/** Append one exact symbolic source prefix with independent flags and sender identity.
 * @param out Owned payload.
 * @param flags Exact raw input flags.
 * @param symbol Exact UTF16.
 * @param sender Actual directional sender.
 */
void prefix(std::vector<std::byte> &out, std::int32_t flags, std::u16string_view symbol, std::int32_t sender) {
    out.push_back(std::byte{0xf9});
    number(out, flags);
    out.push_back(std::byte{0xfd});
    number(out, static_cast<std::int64_t>(symbol.size()));

    for (auto unit : symbol) {
        std::array<std::byte, 4> encoded{};
        const auto result = e::writeUtfChar(unit, encoded);
        REQUIRE(result.status == e::Status::OK);
        out.insert(out.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(result.written));
    }

    number(out, sender);
}

/** Describe one complete caller-configured record using its explicit lookup names.
 * @param local Complete local schema.
 * @param sender Incoming directional ID.
 * @return Actual DESCRIBE_RECORDS envelope.
 */
std::vector<std::byte> genericDescription(const c::LocalSchema &local, std::int32_t sender = 17) {
    const auto &r = local.records()[0];
    std::vector<std::byte> body;
    number(body, sender);
    text(body, r.name);
    number(body, static_cast<std::int64_t>(r.fields.size()));

    for (std::size_t field = 0; field < r.fields.size(); ++field) {
        const auto found = std::ranges::find_if(r.lookups, [field](const auto &k) {
            return k.fieldId == field;
        });
        REQUIRE(found != r.lookups.end());
        text(body, found->name);
        number(body, r.fields[field].serialTypeId);
    }

    return frame(2, body);
}
} // namespace

namespace {
/** Retain actual arbitrary complete callback records, without a three-field assumption. */
struct Snapshot : m::MuxConsumer {
    std::vector<m::OwnedMuxBatch> batches; ///< Immutable caller-held allocations.

    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        batches.push_back(batch.retain());

        return m::BatchAction::CONTINUE;
    }
};

/** Build the same explicit queue profile for arbitrary validated complete schemas.
 * @param local Actual caller schema.
 * @param family Immutable family.
 * @return Actual composed owner.
 */
m::MuxQueue queueFor(const c::LocalSchema &local, c::SubscriptionFamily family) {
    auto work = turn();
    std::expected<m::MuxQueue, m::QueueFailure> result =
        std::unexpected(m::QueueFailure{m::QueueError::INVALID_STATE, {}});

    if (family == c::SubscriptionFamily::STREAM)
        result = m::MuxQueue::createStream(local, muxLimits(), desiredLimits(),
                                           {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, queueLimits(), work);
    else if (family == c::SubscriptionFamily::TICKER)
        result = m::MuxQueue::createTicker(local, muxLimits(),
                                           {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desiredLimits()},
                                           {false, 100}, queueLimits(), work);
    else
        result = m::MuxQueue::createHistory(
            local, muxLimits(),
            {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desiredLimits()},
            {false, 100, 10000}, queueLimits(), work);

    REQUIRE(result);

    return std::move(*result);
}

/** Apply exact IBM interest in a real graph.
 * @param f Source and queue.
 * @param h Port.
 */
void addIbm(Fixture &f, m::MuxHandle &h) {
    const c::SubscriptionRequest request{{f.schema.recordContext(0), u"IBM"},
                                         f.family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0}
                                                                                    : std::nullopt};
    auto sw = submitTurn();
    auto t = h.add(std::span(&request, 1), sw);
    REQUIRE(t);
    until(f, [&] {
        return h.state() == m::QueueClientState::ACTIVE && t->result().has_value();
    });
    REQUIRE(t->result()->status == m::CommandStatus::APPLIED);
}

/** Transfer an already admitted actual event with bounded deterministic fair pumping.
 * @param f Graph.
 * @param event Complete live owner.
 */
void transfer(Fixture &f, p::LiveEvent &&event) {
    for (unsigned n = 0; n < 20000; ++n) {
        auto w = turn();
        auto a = f.queue.admit(std::move(event), w);

        if (a)
            return;

        REQUIRE(a.error().error == m::QueueError::BUSY);
        REQUIRE(f.queue.progress(w));
    }

    FAIL("bounded transfer did not progress");
}
} // namespace

TEST_CASE("MUX queue preserves full materialized Quote and Profile bodies through all actual collectors") {
    const c::SchemaLimits bounds{16, 128, 256, 128, 16384};

    for (const bool profile : {false, true})
        for (const auto family :
             {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
            auto defaults = profile ? q::defaultProfileSchema(bounds) : q::defaultQuoteSchema(bounds);
            REQUIRE(defaults);
            const std::size_t fields = profile ? 16 : 8;
            REQUIRE(defaults->records()[0].fields.size() == fields);
            auto ordinary = c::LocalSchema::create(defaults->records(), bounds, c::LocalProfile::BINARY_FIELDS_V1);
            REQUIRE(ordinary);
            std::vector<std::byte> descriptor, payload;
            std::vector<c::FieldValue> expected;
            {
                auto source = genericSession(*ordinary);
                const auto relative = profile ? "tests/fixtures/encoding/field-types/expected/profile-stream.qtp"
                                              : "tests/fixtures/protocol/quote/expected/time-1.qtp";
                std::ifstream in(std::filesystem::path(ORG_TTLDTOR_QDCXX_MUX_QUEUE_SOURCE_ROOT) / relative,
                                 std::ios::binary);
                REQUIRE(in);
                const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
                const auto input = std::as_bytes(std::span(bytes));
                p::FrameReader reader(liveLimits().frame);
                std::size_t offset = 0;

                while (offset < input.size()) {
                    auto r = reader.feed(input.subspan(offset));
                    REQUIRE(r.frame);
                    offset += r.accepted;

                    if (r.frame->type == 2) {
                        descriptor = r.frame->payload;
                        (void)admit(source, frame(2, descriptor));
                    } else if (r.frame->type == 15) {
                        payload = r.frame->payload;
                        auto event = admit(source, frame(15, payload));
                        REQUIRE(event.records().size() == 1);
                        expected = copyFields(event.records()[0].typedValues);
                    }
                }
            }
            REQUIRE(expected.size() == fields);
            std::vector<c::LocalRecord> records(defaults->records().begin(), defaults->records().end());

            if (family == c::SubscriptionFamily::HISTORY) {
                records[0].hasTime = true;
                records[0].indexedTimeLayout = c::IndexedTimePairInt32{fields, fields + 1};
                records[0].fields.push_back({8});
                records[0].fields.push_back({8});
                records[0].lookups.push_back({u"History.IndexHigh", fields});
                records[0].lookups.push_back({u"History.IndexLow", fields + 1});
            }

            auto local = c::LocalSchema::create(records, bounds, c::LocalProfile::BINARY_FIELDS_V1);
            REQUIRE(local);
            auto snapshot = std::make_shared<Snapshot>();
            {
                Fixture f{*local, genericSession(*local), queueFor(*local, family), family};
                auto work = turn();
                auto handle = f.queue.open(clientLimits(), snapshot, work);
                REQUIRE(handle);
                addIbm(f, *handle);
                const auto sender = e::readCompactInt(descriptor);
                REQUIRE(sender.status == e::Status::OK);

                if (family == c::SubscriptionFamily::HISTORY) {
                    auto rest = std::span<const std::byte>(descriptor).subspan(sender.consumed);
                    auto name = e::readUtfString(rest, {8192, 1024});
                    REQUIRE(name.status == e::Status::OK);
                    REQUIRE(name.value);
                    rest = rest.subspan(name.consumed);
                    auto n = e::readCompactInt(rest);
                    REQUIRE(n.status == e::Status::OK);
                    rest = rest.subspan(n.consumed);
                    std::vector<std::byte> d;
                    number(d, sender.value);
                    text(d, *name.value);
                    number(d, n.value + 2);
                    d.insert(d.end(), rest.begin(), rest.end());
                    text(d, u"History.IndexHigh");
                    number(d, 8);
                    text(d, u"History.IndexLow");
                    number(d, 8);
                    descriptor = std::move(d);
                }

                (void)admit(f.live, frame(2, descriptor));
                std::vector<std::byte> body;

                if (family == c::SubscriptionFamily::HISTORY) {
                    body.push_back(std::byte{0xf9});
                    number(body, 4);
                }

                body.insert(body.end(), payload.begin(), payload.end());

                if (family == c::SubscriptionFamily::HISTORY) {
                    number(body, 0);
                    number(body, 7);
                }

                const auto type =
                    family == c::SubscriptionFamily::STREAM ? 15 : (family == c::SubscriptionFamily::TICKER ? 10 : 20);
                auto event = admit(f.live, frame(type, body));
                transfer(f, std::move(event));
                until(f, [&] {
                    return !snapshot->batches.empty();
                });
            }
            REQUIRE(snapshot->batches[0].records().size() == 1);
            const auto &record = snapshot->batches[0].records()[0];
            const auto values = copyMuxFields(record.typedValues);
            REQUIRE(values.size() == fields + (family == c::SubscriptionFamily::HISTORY ? 2 : 0));
            CHECK(record.context.matches(local->recordContext(0)));
            CHECK(record.symbol == u"IBM");
            REQUIRE(record.source);

            for (std::size_t n = 0; n < fields; ++n) {
                CHECK(values[n].type == expected[n].type);
                CHECK(values[n].raw == expected[n].raw);
                CHECK(values[n].units == expected[n].units);
                CHECK(values[n].bytes == expected[n].bytes);
            }

            const q::MappingLimits mappingLimits{128, 256, 32, 64, 128, 128, 16384, 1000000};

            if (profile) {
                auto mapping = q::PreparedProfile::prepare(*local, 0, mappingLimits);
                REQUIRE(mapping);
                auto mapped = mapping->map(u"IBM", {record.context, values}, {128, 1024, 8192}, 128);
                REQUIRE(mapped);
                CHECK(mapped->flags == 224);
                CHECK(mapped->statusReason == expected[15].units);
            } else {
                auto mapping = q::PreparedQuote::prepare(*local, 0, mappingLimits);
                REQUIRE(mapping);
                auto mapped = mapping->map(u"IBM", {record.context, values}, 128);
                REQUIRE(mapped);
                CHECK(mapped->timeMs == 1000);
            }
        }
}

TEST_CASE("MUX queue detached generic nullable UTF16 opaque and single64 payload survives owner destruction") {
    const c::LocalRecord definition{0,
                                    u"Generic#History",
                                    true,
                                    {{104}, {10}, {137}, {9}, {233}, {249}, {56}},
                                    {{u"Index", 0},
                                     {u"Units", 1},
                                     {u"String", 2},
                                     {u"Bytes", 3},
                                     {u"Custom", 4},
                                     {u"Marshalled", 5},
                                     {u"Extra", 6}},
                                    c::IndexedTimeSingleInt64{0}};
    auto local = c::LocalSchema::create(std::span(&definition, 1), {16, 128, 256, 128, 16384},
                                        c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto snapshot = std::make_shared<Snapshot>();
    const std::u16string symbol{u'S', char16_t{0}, char16_t{0xd800}};
    const std::u16string units{char16_t{0}, u'A', char16_t{0xd800}};
    {
        Fixture f{*local, genericSession(*local), queueFor(*local, c::SubscriptionFamily::HISTORY),
                  c::SubscriptionFamily::HISTORY};
        auto w = turn();
        auto h = f.queue.open(clientLimits(), snapshot, w);
        REQUIRE(h);
        const c::SubscriptionRequest request{{local->recordContext(0), symbol}, 0};
        auto sw = submitTurn();
        auto ticket = h->add(std::span(&request, 1), sw);
        REQUIRE(ticket);
        until(f, [&] {
            return h->state() == m::QueueClientState::ACTIVE && ticket->result().has_value();
        });
        (void)admit(f.live, genericDescription(*local));
        std::vector<std::byte> body;
        prefix(body, 4, symbol, 17);
        number(body, 7);
        std::array<std::byte, 128> encoded{};
        auto chars = e::writeCharArray(units, encoded, {128, 32});
        REQUIRE(chars.status == e::Status::OK);
        body.insert(body.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(chars.written));
        number(body, -1);
        number(body, 0);
        number(body, 3);
        body.insert(body.end(), {std::byte{0}, std::byte{0xff}, std::byte{1}});
        number(body, -1);
        number(body, -5);
        auto event = admit(f.live, frame(20, body));
        transfer(f, std::move(event));
        until(f, [&] {
            return !snapshot->batches.empty();
        });
    }
    auto saved = snapshot->batches.front();
    snapshot->batches.clear();
    std::atomic<bool> exact{false};
    std::thread reader([batch = std::move(saved), &exact, &symbol, &units]() mutable {
        const auto &r = batch.records()[0];
        const auto &f = r.typedValues;
        exact = r.symbol == symbol && r.rawTime == 7 && f.size() == 7 && f[1].units && *f[1].units == units &&
                !f[2].units && f[3].bytes && f[3].bytes->empty() && f[4].bytes && f[4].bytes->size() == 3 &&
                (*f[4].bytes)[1] == std::byte{0xff} && !f[5].bytes && f[6].raw == -5;
        batch = {};
    });
    reader.join();
    CHECK(exact.load());
}

namespace {
/** Actual pure source reference; no delivery algorithm is reproduced by the queue test. */
struct Reference {
    std::optional<s::StreamCollector> stream;       ///< Actual STREAM staging owner.
    std::optional<s::TickerCollector> ticker;       ///< Actual shared TICKER cache.
    std::optional<s::HistoryCollector> history;     ///< Actual shared HISTORY cache.
    s::TickerViewToken tickerView;                  ///< Actual pure reference interest.
    s::HistoryViewToken historyView;                ///< Actual pure reference interest.
    std::vector<std::int64_t> values;               ///< Copied real pure outputs.
    std::vector<std::int32_t> flags;                ///< Copied real produced flags.
    std::vector<std::optional<std::int64_t>> times; ///< Copied actual history indexes only.

    /** Construct an actual sole reference view.
     * @param local Exact immutable input context.
     * @param family Source family.
     */
    Reference(const c::LocalSchema &local, c::SubscriptionFamily family) {
        auto work = turn();
        const c::SubscriptionRequest request{{local.recordContext(0), u"ALPHA"},
                                             family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0}
                                                                                      : std::nullopt};

        if (family == c::SubscriptionFamily::STREAM) {
            auto desired = c::SubscriptionState::create(local, family, desiredLimits());
            REQUIRE(desired);
            REQUIRE(desired->add(std::span(&request, 1), work.desired));
            auto owner = s::StreamCollector::create(std::move(*desired), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                                    work.stream);
            REQUIRE(owner);
            stream.emplace(std::move(*owner));
        } else if (family == c::SubscriptionFamily::TICKER) {
            auto copy = local;
            auto owner = s::TickerCollector::create(std::move(copy),
                                                    {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desiredLimits()},
                                                    {false, 100}, work.ticker);
            REQUIRE(owner);
            ticker.emplace(std::move(*owner));
            auto view = ticker->openView(work.ticker);
            REQUIRE(view);
            tickerView = *view;
            REQUIRE(ticker->add(tickerView, std::span(&request, 1), work.desired, work.ticker));
        } else {
            auto copy = local;
            auto owner = s::HistoryCollector::create(
                std::move(copy),
                {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desiredLimits()},
                {false, 100, 10000}, work.history);
            REQUIRE(owner);
            history.emplace(std::move(*owner));
            auto view = history->openView({true}, work.history);
            REQUIRE(view);
            historyView = *view;
            REQUIRE(history->add(historyView, std::span(&request, 1), work.desired, work.history));
        }
    }

    /** Stage and fully drain the actual source before the next controlled input.
     * @param event Actual admitted owner, consumed only by the real source.
     */
    void apply(p::LiveEvent &&event) {
        auto w = turn();

        if (stream)
            REQUIRE(stream->stage(std::move(event), w.stream));
        else if (ticker)
            REQUIRE(ticker->stage(std::move(event), w.ticker));
        else
            REQUIRE(history->stage(std::move(event), w.history));

        for (unsigned n = 0; n < 512; ++n) {
            w = turn();

            if (stream) {
                auto o = stream->front(w.stream);
                REQUIRE(o);

                if (!*o)
                    return;

                values.push_back((**o).record->typedValues[2].raw);
                flags.push_back((**o).record->eventFlags);
                times.push_back({});
                REQUIRE(stream->pop(w.stream));
            } else if (ticker) {
                auto o = ticker->front(tickerView, w.ticker);
                REQUIRE(o);

                if (!*o)
                    return;

                values.push_back((**o).value.sourceRecord->typedValues[2].raw);
                flags.push_back((**o).deliveryFlags);
                times.push_back({});
                REQUIRE(ticker->pop(tickerView, w.ticker));
            } else {
                auto o = history->front(historyView, w.history);
                REQUIRE(o);

                if (!*o)
                    return;

                values.push_back((**o).record.fields[2].raw);
                flags.push_back((**o).record.eventFlags);
                times.push_back((**o).record.rawTime);
                REQUIRE(history->pop(historyView, w.history));
            }
        }

        FAIL("bounded real reference drain exceeded its finite input profile");
    }
};
} // namespace

TEST_CASE("MUX queue sustained fast stalled and churning registrations match real source outputs beyond turn quota") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto bounds = queueLimits();
        bounds.sourceRecords = 8;
        bounds.routeRecords = 2;
        auto f = fixture(family, bounds);
        auto referenceSession = session(f.schema);
        Reference reference(f.schema, family);
        auto a = std::make_shared<Recorder>();
        auto b = std::make_shared<Recorder>();
        auto slow = std::make_shared<Recorder>();
        slow->pause = true;
        auto fastA = open(f, a);
        auto fastB = open(f, b);
        auto small = clientLimits();
        small.dataItems = 2;
        auto stalled = open(f, slow, small);
        add(f, fastA);
        add(f, fastB);
        add(f, stalled);
        std::size_t peakRecords = 0, peakBytes = 0, peakResident = 0, churned = 0;
        auto sample = [&] {
            const auto resources = f.queue.resources();
            peakRecords = std::max(peakRecords, resources.dataItems);
            peakBytes = std::max(peakBytes, resources.recordBytes);
            peakResident = std::max(peakResident, resources.residentBytes);
        };
        a->action = sample;
        b->action = sample;
        slow->action = sample;

        for (std::int32_t step = 0; step < 300; ++step) {
            std::vector<Input> inputs;

            if (family == c::SubscriptionFamily::HISTORY && !step)
                inputs = {{0, u"ALPHA", 3, 1, 4}, {0, u"ALPHA", 0, 0, 10}};
            else
                inputs = {{0, u"ALPHA", 3, step * 2 + 1, 0},
                          {0, u"BETA", 3, 999, 0},
                          {0, u"ALPHA", 3, step * 2 + 2, 0},
                          {0, u"ALPHA", 3, step * 2 + 2, 0}};

            const auto type =
                family == c::SubscriptionFamily::STREAM ? 15 : (family == c::SubscriptionFamily::TICKER ? 10 : 20);
            const auto encoded = data(std::span<const Input>(inputs), type);
            auto actual = admit(f.live, encoded);
            auto pure = admit(referenceSession, encoded);
            REQUIRE(actual.records().size() == pure.records().size());

            for (std::size_t n = 0; n < actual.records().size(); ++n) {
                const auto &x = actual.records()[n];
                const auto &y = pure.records()[n];
                CHECK(x.symbol == y.symbol);
                CHECK(x.eventFlags == y.eventFlags);
                REQUIRE(x.typedValues.size() == y.typedValues.size());

                for (std::size_t j = 0; j < x.typedValues.size(); ++j)
                    CHECK(x.typedValues[j].raw == y.typedValues[j].raw);
            }

            reference.apply(std::move(pure));
            transfer(f, std::move(actual));
            until(f, [&] {
                return a->values.size() == reference.values.size() && b->values.size() == reference.values.size();
            });
            CHECK(a->values == reference.values);
            CHECK(a->flags == reference.flags);
            CHECK(a->times == reference.times);
            CHECK(b->values == a->values);
            CHECK(b->flags == a->flags);
            CHECK(b->times == a->times);
            const auto resources = f.queue.resources();
            peakRecords = std::max(peakRecords, resources.dataItems);
            peakBytes = std::max(peakBytes, resources.recordBytes);
            peakResident = std::max(peakResident, resources.residentBytes);
            CHECK(resources.residentBytes <= resources.reservedBytes);
            CHECK(resources.reservedBytes <= bounds.residentBytes);

            if (step && step % 25 == 0) {
                auto observer = std::make_shared<Recorder>();
                auto transient = open(f, observer);
                add(f, transient);
                CHECK(transient.requestStop(m::StopMode::ABORT) != m::ControlAdmission::CLOSED);
                until(f, [&] {
                    return transient.terminal().has_value();
                });
                transient = {};
                until(f, [&] {
                    return f.queue.resources().generations == 3;
                });
                ++churned;
            }
        }

        std::filesystem::create_directories(ORG_TTLDTOR_QDCXX_MUX_QUEUE_OBSERVATIONS);
        const auto familyName = family == c::SubscriptionFamily::STREAM
                                    ? "stream"
                                    : (family == c::SubscriptionFamily::TICKER ? "ticker" : "history");
        std::ofstream trace(std::filesystem::path(ORG_TTLDTOR_QDCXX_MUX_QUEUE_OBSERVATIONS) /
                                (std::string(familyName) + "-source-equivalence.tsv"),
                            std::ios::binary);
        trace << "layer\tordinal\tvalue\tflags\traw-time\n";

        for (std::size_t n = 0; n < reference.values.size(); ++n) {
            for (const bool actual : {false, true}) {
                trace << (actual ? "queue" : "pure-reference") << '\t' << n << '\t'
                      << (actual ? a->values[n] : reference.values[n]) << '\t'
                      << (actual ? a->flags[n] : reference.flags[n]) << '\t';
                const auto time = actual ? a->times[n] : reference.times[n];

                if (time)
                    trace << *time;
                else
                    trace << "none";

                trace << '\n';
            }
        }

        REQUIRE(trace.good());
        std::ofstream metrics(std::filesystem::path(ORG_TTLDTOR_QDCXX_MUX_QUEUE_OBSERVATIONS) /
                                  (std::string(familyName) + "-workload.tsv"),
                              std::ios::binary);
        metrics << "frames\tsource-turn-record-quota\tpure-output-records\thealthy-client-count\tchurned\tobserved-"
                   "record-peak\tobserved-record-byte-peak\tobserved-resident-peak\n300\t"
                << bounds.sourceRecords << '\t' << reference.values.size() << "\t2\t" << churned << '\t' << peakRecords
                << '\t' << peakBytes << '\t' << peakResident << '\n';
        REQUIRE(metrics.good());
        REQUIRE(stalled.terminal());
        CHECK(stalled.terminal()->outcome == m::TerminalOutcome::FAILED);
        CHECK(stalled.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
        CHECK(a->values.size() > bounds.sourceRecords);
        CHECK(churned == 11);
        CHECK(peakRecords <= bounds.dataItems);
        CHECK(peakBytes <= bounds.recordBytes);
        CHECK(peakResident <= bounds.residentBytes);
        CHECK(fastA.requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
        CHECK(fastB.requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
        until(f, [&] {
            return fastA.terminal().has_value() && fastB.terminal().has_value();
        });
        CHECK(fastA.terminal()->outcome == m::TerminalOutcome::DRAINED);
        CHECK(fastB.terminal()->outcome == m::TerminalOutcome::DRAINED);
        fastA = {};
        fastB = {};
        stalled = {};
        until(f, [&] {
            return f.queue.resources().generations == 0;
        });
        CHECK(f.queue.resources().dataItems == 0);
        CHECK(f.queue.resources().recordBytes == 0);
        CHECK(f.queue.resources().batchOwners == 0);
        CHECK(f.queue.resources().batchBytes == 0);
    }
}

TEST_CASE("MUX queue producer submissions controls completions and public retirement use detached thread safe ports") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    std::atomic<bool> done{false};
    std::atomic<std::size_t> accepted{0}, rejected{0};
    std::thread producer([handle = h, &done, &accepted, &rejected, context = f.schema.recordContext(0)]() mutable {
        std::array<m::MuxCommandTicket, 8> retained;
        const c::SubscriptionRequest request{{context, u"ALPHA"}, {}};

        for (unsigned n = 0; n < 1000; ++n) {
            auto copy = handle;
            auto work = submitTurn();
            auto ticket = copy.add(std::span(&request, 1), work);

            if (ticket) {
                ++accepted;
                retained[n % retained.size()] = *ticket;
                (void)retained[n % retained.size()].result();
            } else {
                CHECK(
                    (ticket.error() == m::QueueError::COMMAND_LIMIT || ticket.error() == m::QueueError::RESULT_LIMIT));
                ++rejected;
            }

            copy.requestResume();
        }

        handle.requestStop(m::StopMode::GRACEFUL);
        handle = {};
        done = true;
    });

    for (unsigned n = 0; n < 1000000 && !done.load(); ++n) {
        auto work = turn();
        REQUIRE(f.queue.progress(work));
        std::this_thread::yield();
    }

    producer.join();
    REQUIRE(done.load());
    CHECK(accepted + rejected == 1000);
    CHECK(accepted > 0);
    until(f, [&] {
        return h.terminal().has_value();
    });
    CHECK(h.terminal()->outcome == m::TerminalOutcome::DRAINED);
    auto retained = h;
    h = {};
    std::thread last([port = std::move(retained)]() mutable {
        auto copy = port;
        CHECK(copy.terminal().has_value());
        port = {};
        copy = {};
    });
    last.join();
    until(f, [&] {
        return f.queue.resources().generations == 0;
    });
}

TEST_CASE("MUX queue exact complete record byte reservation and field count bound reject only overflowing clients") {
    auto first = fixture(c::SubscriptionFamily::STREAM);
    auto keeper = std::make_shared<Recorder>();
    keeper->retain = true;
    auto handle = open(first, keeper);
    add(first, handle);
    send(first, {{0, u"ALPHA", 1, 10, 0}});
    until(first, [&] {
        return keeper->values.size() == 1;
    });
    const auto measured = first.queue.resources(handle);
    REQUIRE(measured);
    REQUIRE(measured->recordOwners == 1);
    const auto exact = measured->recordBytes;
    const auto exactFields =
        keeper->retained[0].records()[0].values.size() + keeper->retained[0].records()[0].typedValues.size();

    for (const bool oneShort : {false, true}) {
        auto f = fixture(c::SubscriptionFamily::STREAM);
        auto bounded = std::make_shared<Recorder>();
        auto healthy = std::make_shared<Recorder>();
        auto limits = clientLimits();
        limits.recordBytes = exact - (oneShort ? 1 : 0);
        limits.dataItems = 1;
        limits.recordFields = exactFields;
        auto small = open(f, bounded, limits);
        auto good = open(f, healthy);
        add(f, small);
        add(f, good);
        send(f, {{0, u"ALPHA", 1, 10, 0}});
        until(f, [&] {
            return healthy->values.size() == 1 &&
                   (oneShort ? small.terminal().has_value() : bounded->values.size() == 1);
        });
        CHECK(bounded->values.size() == (oneShort ? 0 : 1));

        if (oneShort) {
            CHECK(small.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
        } else {
            CHECK(!small.terminal());
        }
    }

    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto bounded = std::make_shared<Recorder>();
    auto healthy = std::make_shared<Recorder>();
    auto limits = clientLimits();
    limits.recordFields = exactFields - 1;
    auto small = open(f, bounded, limits);
    auto good = open(f, healthy);
    add(f, small);
    add(f, good);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return small.terminal().has_value() && healthy->values.size() == 1;
    });
    CHECK(bounded->values.empty());
    CHECK(small.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
}

TEST_CASE(
    "MUX queue factory and empty open prepay every independently sized metadata initialization and cleanup axis") {
    auto local = schema();
    auto aggregate = queueLimits();
    aggregate.clients = 128;
    aggregate.sourceRecords = 1;
    aggregate.routeRecords = 1;
    auto mux = muxLimits();
    mux.clients = 128;
    auto work = turn();
    work.visits = 500;
    auto owner = m::MuxQueue::createStream(local, mux, desiredLimits(), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024},
                                           aggregate, work);
    CHECK_FALSE(owner);

    if (!owner)
        CHECK(owner.error().error == m::QueueError::WORK_LIMIT);

    CHECK(local.recordContext(0).matches(local.recordContext(0)));
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto consumer = std::make_shared<Recorder>();
    auto limits = clientLimits();
    limits.commandSymbolUnits = 0;
    work = turn();
    work.visits = 400;
    auto opened = f.queue.open(limits, consumer, work);
    CHECK_FALSE(opened);

    if (!opened)
        CHECK(opened.error().error == m::QueueError::WORK_LIMIT);

    CHECK(f.queue.resources().generations == 0);
    CHECK(f.queue.aggregateEntries().empty());
}

namespace {
thread_local bool allocationFault = false;        ///< Narrow current-thread C++ allocation interception only.
thread_local std::size_t allocationAllowance = 0; ///< Successful requests before injected failure.
thread_local std::size_t allocationRequests = 0;  ///< Observed requests inside the explicitly armed operation.
std::atomic<void *> heldDeletePointer{
    nullptr}; ///< One actual publicly observed arena address under the narrow release probe.
std::atomic<bool> deleteEntered{false}, deleteContinue{true}; ///< Deterministic producer-delete handshake.
} // namespace

/** Inject only explicitly armed current-thread ordinary allocations.
 * @param size Requested bytes.
 * @return Ordinary owned backing, or bad_alloc before publication.
 */
void *operator new(std::size_t size) {
    if (allocationFault) {
        ++allocationRequests;

        if (!allocationAllowance)
            throw std::bad_alloc();

        --allocationAllowance;
    }

    if (auto *p = std::malloc(size ? size : 1))
        return p;

    throw std::bad_alloc();
}

/** Delegate test array allocations.
 * @param size Requested bytes.
 * @return Ordinary owned backing, or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release test ordinary backing.
 * @param value Original allocation, or null.
 */
void operator delete(void *value) noexcept {
    if (value && value == heldDeletePointer.load()) {
        deleteEntered = true;

        while (!deleteContinue.load())
            std::this_thread::yield();
    }

    std::free(value);
}

/** Release test array backing.
 * @param value Original allocation, or null.
 */
void operator delete[](void *value) noexcept {
    if (value && value == heldDeletePointer.load()) {
        deleteEntered = true;

        while (!deleteContinue.load())
            std::this_thread::yield();
    }

    std::free(value);
}

/** Release compiler-sized ordinary backing.
 * @param value Original allocation, or null.
 * @param size Deliberately unused compiler hint.
 */
void operator delete(void *value, std::size_t size) noexcept {
    (void)size;
    ::operator delete(value);
}

/** Release compiler-sized array backing.
 * @param value Original allocation, or null.
 * @param size Deliberately unused compiler hint.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    (void)size;
    ::operator delete[](value);
}

TEST_CASE("MUX queue failed STREAM fanout allocations preserve every client queue and the real source prefix") {
    bool reachedCompleteAllocation = false;

    for (std::size_t failure = 0; failure < 32; ++failure) {
        auto f = fixture(c::SubscriptionFamily::STREAM);
        auto a = std::make_shared<Recorder>();
        auto b = std::make_shared<Recorder>();
        auto ha = open(f, a);
        auto hb = open(f, b);
        add(f, ha);
        add(f, hb);
        send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 1, 10, 0}});
        auto work = turn();
        auto staged = f.queue.progress(work);
        REQUIRE(staged);
        REQUIRE(staged->kind == m::QueueProgressKind::STAGED);
        allocationAllowance = failure;
        allocationRequests = 0;
        allocationFault = true;
        work = turn();
        auto rejected = f.queue.progress(work);
        allocationFault = false;

        for (unsigned n = 0; n < 100 && rejected && rejected->kind != m::QueueProgressKind::ROUTED; ++n) {
            work = turn();
            allocationFault = true;
            rejected = f.queue.progress(work);
            allocationFault = false;
        }

        if (rejected) {
            CHECK(rejected->kind == m::QueueProgressKind::ROUTED);
            CHECK(allocationRequests == failure);
            until(f, [&] {
                return a->values.size() == 2 && b->values.size() == 2;
            });
            reachedCompleteAllocation = true;
            break;
        }

        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == m::QueueError::ALLOCATION_FAILURE);
        CHECK(allocationRequests == failure + 1);
        CHECK(a->values.empty());
        CHECK(b->values.empty());
        CHECK(f.queue.resources().dataItems == 0);
        CHECK(f.queue.resources().recordBytes == 0);
        REQUIRE(f.queue.resources(ha));
        CHECK(f.queue.resources(ha)->queuedRecords == 0);
        CHECK(f.queue.resources(hb)->queuedRecords == 0);
        until(f, [&] {
            return a->values.size() == 2 && b->values.size() == 2;
        });
        CHECK(a->values == std::vector<std::int64_t>{10, 10});
        CHECK(b->values == a->values);
    }

    CHECK(reachedCompleteAllocation);
}

TEST_CASE("MUX queue failed empty open allocation or actual view WORK leaves no registration reservation") {
    bool reachedSuccessfulOpen = false;

    for (std::size_t failure = 0; failure < 64 && !reachedSuccessfulOpen; ++failure) {
        auto f = fixture(c::SubscriptionFamily::TICKER);
        auto consumer = std::make_shared<Recorder>();
        const auto baseline = f.queue.resources();
        auto work = turn();
        allocationAllowance = failure;
        allocationRequests = 0;
        allocationFault = true;
        auto result = f.queue.open(clientLimits(), consumer, work);
        allocationFault = false;

        if (result) {
            reachedSuccessfulOpen = true;
            CHECK(f.queue.resources().generations == 1);
        } else {
            CHECK((result.error().error == m::QueueError::ALLOCATION_FAILURE ||
                   result.error().error == m::QueueError::SOURCE_FAILURE));
            CHECK(f.queue.resources().generations == 0);
            CHECK(f.queue.resources().reservedBytes == baseline.reservedBytes);
            CHECK(f.queue.resources().collectorBytes == baseline.collectorBytes);
            CHECK(f.queue.revision() == 4);
            work = turn();
            auto retry = f.queue.open(clientLimits(), consumer, work);
            REQUIRE(retry);
            CHECK(f.queue.resources().generations == 1);
        }
    }

    CHECK(reachedSuccessfulOpen);
    auto f = fixture(c::SubscriptionFamily::TICKER);
    const auto baseline = f.queue.resources();
    auto consumer = std::make_shared<Recorder>();
    auto work = turn();
    work.ticker.operations = 0;
    auto rejected = f.queue.open(clientLimits(), consumer, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == m::QueueError::WORK_LIMIT);
    CHECK(f.queue.resources().generations == 0);
    CHECK(f.queue.resources().reservedBytes == baseline.reservedBytes);
    CHECK(f.queue.resources().collectorBytes == baseline.collectorBytes);
    auto local = schema();
    auto bounds = queueLimits();
    bounds.residentBytes = 1;
    work = turn();
    allocationAllowance = 0;
    allocationRequests = 0;
    allocationFault = true;
    auto invalid = m::MuxQueue::createStream(local, muxLimits(), desiredLimits(),
                                             {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, bounds, work);
    allocationFault = false;
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error().error == m::QueueError::RESOURCE_LIMIT);
    CHECK(allocationRequests == 0);
}

TEST_CASE("MUX queue last public release inside callback keeps executing callable backing through return") {
    struct Lifetime final : m::MuxConsumer {
        m::MuxHandle *handle = nullptr;    ///< The actual last public handle, serialized with this callback.
        m::OwnedMuxBatch *saved = nullptr; ///< Caller-owned detached output.
        std::atomic<bool> *returned = nullptr, *destroyed = nullptr,
                          *premature = nullptr; ///< Independent test observations.
        bool inside = false;                    ///< Callable body is active.

        ~Lifetime() override {
            if (inside)
                premature->store(true);

            destroyed->store(true);
        }

        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            inside = true;
            *saved = batch.retain();
            *handle = {};
            CHECK(batch.records()[0].typedValues[2].raw == 10);
            CHECK_FALSE(destroyed->load());
            inside = false;
            returned->store(true);

            return m::BatchAction::CONTINUE;
        }
    };

    auto f = fixture(c::SubscriptionFamily::STREAM);
    m::MuxHandle h;
    m::OwnedMuxBatch retained;
    std::atomic<bool> returned = false, destroyed = false, premature = false;
    auto consumer = std::make_shared<Lifetime>();
    consumer->handle = &h;
    consumer->saved = &retained;
    consumer->returned = &returned;
    consumer->destroyed = &destroyed;
    consumer->premature = &premature;
    auto work = turn();
    auto opened = f.queue.open(clientLimits(), consumer, work);
    REQUIRE(opened);
    h = std::move(*opened);
    add(f, h);
    consumer.reset();
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return returned.load() && destroyed.load();
    });
    CHECK_FALSE(premature.load());
    CHECK(h.requestStop(m::StopMode::ABORT) == m::ControlAdmission::CLOSED);
    CHECK(retained.records()[0].typedValues[2].raw == 10);
    CHECK(f.queue.resources().generations == 1);
    std::thread release([batch = std::move(retained)]() mutable {
        batch = {};
    });
    release.join();
    until(f, [&] {
        return f.queue.resources().generations == 0;
    });
}

TEST_CASE("MUX queue abort reports materialized actual source projections discarded before detached publication") {
    for (const auto family : {c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto f = fixture(family);
        auto consumer = std::make_shared<Recorder>();
        auto h = open(f, consumer);
        add(f, h);

        if (family == c::SubscriptionFamily::HISTORY) {
            send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
            until(f, [&] {
                return consumer->values.size() == 2;
            });
            consumer->values.clear();
            send(f, {{0, u"ALPHA", 3, 31, 1}});
        } else
            send(f, {{0, u"ALPHA", 1, 10, 0}});

        auto work = turn();
        auto staged = f.queue.progress(work);
        REQUIRE(staged);
        REQUIRE(staged->kind == m::QueueProgressKind::STAGED);
        CHECK(h.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
        until(f, [&] {
            return h.terminal().has_value();
        });
        const auto terminal = *h.terminal();
        CHECK(consumer->values.empty());
        CHECK(terminal.counts.queueAdmitted == (family == c::SubscriptionFamily::HISTORY ? 2 : 0));
        CHECK(terminal.counts.collectorAcknowledged == (family == c::SubscriptionFamily::HISTORY ? 2 : 0));
        CHECK(terminal.counts.discarded == 1);
        CHECK(terminal.hadGap);
        CHECK(terminal.outcome == m::TerminalOutcome::ABORTED);
    }
}

TEST_CASE("MUX queue HISTORY unmaterialized initial loss remains unknown rather than invented delivery counts") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    auto work = turn();
    REQUIRE(f.queue.progress(work));
    CHECK(h.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return h.terminal().has_value();
    });
    CHECK(h.terminal()->counts.discarded == 0);
    CHECK(h.terminal()->unknownEligibility);
    CHECK(h.terminal()->hadGap);
    CHECK(h.terminal()->unknownUpstreamLoss);
    CHECK(consumer->values.empty());
}

TEST_CASE("MUX queue abort separates accepted unstaged source records from delivery projection counts") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    until(f, [&] {
        return consumer->values.size() == 2;
    });
    CHECK(range(f, h).completeDeliveredRange);
    send(f, {{0, u"ALPHA", 3, 31, 1}});
    f.queue.requestStop(m::StopMode::ABORT);
    CHECK_FALSE(range(f, h).completeDeliveredRange);
    auto work = turn();
    auto dropped = f.queue.progress(work);
    REQUIRE(dropped);
    CHECK(dropped->kind == m::QueueProgressKind::RELEASED);
    CHECK_FALSE(range(f, h).completeDeliveredRange);
    until(f, [&] {
        return f.queue.terminal().has_value();
    });
    const auto terminal = *f.queue.terminal();
    CHECK(terminal.counts.sourceRecordsDiscarded == 1);
    CHECK(terminal.counts.discarded == 0);
    CHECK(terminal.unknownEligibility);
    CHECK(terminal.hadGap);
    CHECK(terminal.outcome == m::TerminalOutcome::ABORTED);
    REQUIRE(h.terminal());
    CHECK(h.terminal()->unknownEligibility);
    CHECK(h.terminal()->hadGap);
}

TEST_CASE("MUX queue reset source interval counts are assigned once through coalescing and global ACK retries") {
    for (const auto family : {c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto f = fixture(family);
        auto consumer = std::make_shared<Recorder>();
        auto h = open(f, consumer);
        add(f, h);

        if (family == c::SubscriptionFamily::HISTORY) {
            send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
            until(f, [&] {
                return consumer->values.size() == 2;
            });
            send(f, {{0, u"ALPHA", 3, 31, 1}});
        } else
            send(f, {{0, u"ALPHA", 1, 10, 0}});

        auto work = turn();
        auto staged = f.queue.progress(work);
        REQUIRE(staged);
        REQUIRE(staged->kind == m::QueueProgressKind::STAGED);
        work = turn();
        REQUIRE(f.queue.resetSource(work));
        until(f, [&] {
            return f.queue.resetStatus().phase == m::ResetPhase::GLOBAL_ACK_PENDING;
        });
        const auto initial = f.queue.resetStatus();
        CHECK(initial.discardedSourceProjections == 1);
        CHECK_FALSE(initial.sourceCountsSaturated);
        CHECK(initial.unknownSourceLoss);
        CHECK(initial.unknownEligibility);
        REQUIRE(initial.actualSourceSequence);

        for (unsigned n = 0; n < 3; ++n) {
            work = turn();
            auto coalesced = f.queue.resetSource(work);
            REQUIRE(coalesced);
            CHECK(coalesced->kind == m::ResetAdmissionKind::COALESCED);

            if (family == c::SubscriptionFamily::TICKER) {
                work = turn();
                work.ticker.operations = 0;
                auto yielded = f.queue.progress(work);
                REQUIRE_FALSE(yielded);
                CHECK(yielded.error().error == m::QueueError::WORK_LIMIT);
            }

            CHECK(f.queue.resetStatus().actualSourceSequence == initial.actualSourceSequence);
            CHECK(f.queue.resetStatus().discardedSourceProjections == 1);
        }

        until(f, [&] {
            return f.queue.resetStatus().globalAcknowledged;
        });
        REQUIRE(h.resetNotice());
        CHECK(h.resetNotice()->knownDiscardedQueueRecords == 0);
        CHECK_FALSE(h.resetNotice()->knownDiscardedSourceRecords);
        CHECK(h.resetNotice()->unknownEligibility);
        CHECK(h.resetNotice()->unknownUpstreamLoss);
        f.queue.requestStop(m::StopMode::ABORT);
        until(f, [&] {
            return f.queue.terminal().has_value();
        });
        CHECK(f.queue.terminal()->counts.discarded == 1);
        CHECK(f.queue.terminal()->counts.sourceRecordsDiscarded == 0);
        CHECK(f.queue.terminal()->reset.discardedSourceProjections == 1);
    }
}

TEST_CASE("MUX queue actual HISTORY pending overflow isolates the low view and retains explicit incomplete evidence") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto work = turn();
    auto queued = m::MuxQueue::createHistory(
        f.schema, muxLimits(),
        {32, 256, 128, 64, 4 * 1024 * 1024, 4, 4, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desiredLimits()},
        {false, 100, 10000}, queueLimits(), work);
    REQUIRE(queued);
    f.queue = std::move(*queued);
    auto lowConsumer = std::make_shared<Recorder>();
    auto highConsumer = std::make_shared<Recorder>();
    auto low = open(f, lowConsumer);
    auto high = open(f, highConsumer);
    add(f, low, 0);
    add(f, high, 5);
    send(f, {{0, u"ALPHA", 0, 0, 14}});

    for (unsigned n = 0; n < 100; ++n) {
        work = turn();
        REQUIRE(f.queue.progress(work));
    }

    REQUIRE_FALSE(low.terminal());
    REQUIRE_FALSE(high.terminal());
    REQUIRE(lowConsumer->values.size() > 0);
    REQUIRE(highConsumer->values.size() > 0);
    const auto highBaseline = highConsumer->values.size();
    const auto before = range(f, high);
    REQUIRE(before.source);
    send(f, {{0, u"ALPHA", 5, 51, 1},
             {0, u"ALPHA", 4, 41, 1},
             {0, u"ALPHA", 3, 31, 1},
             {0, u"ALPHA", 2, 21, 1},
             {0, u"ALPHA", 1, 11, 0}});
    until(f, [&] {
        return low.terminal().has_value() &&
               (high.terminal().has_value() || highConsumer->values.size() > highBaseline);
    });
    CHECK(low.terminal()->outcome == m::TerminalOutcome::FAILED);
    REQUIRE(low.terminal()->primary);
    CHECK(low.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
    CHECK(low.terminal()->primary->source.has_value());
    CHECK(low.terminal()->unknownEligibility);
    CHECK(low.terminal()->hadGap);
    CHECK_FALSE(high.terminal());
    const auto after = range(f, high);
    REQUIRE(after.source);
    CHECK(after.epoch.matches(before.epoch));
    CHECK(after.source->epoch.matches(before.source->epoch));
    CHECK(after.mappingCurrent);
    CHECK(after.completeDeliveredRange);
}

TEST_CASE("MUX queue source ACK work and late-client detached copy work preserve all-client STREAM transaction") {
    bool observedLateCopyFailure = false;

    for (const auto fields : {6u, 12u, 18u, 24u, 30u, 36u, 1000000u}) {
        auto f = fixture(c::SubscriptionFamily::STREAM);
        auto a = std::make_shared<Recorder>();
        auto b = std::make_shared<Recorder>();
        auto ha = open(f, a);
        auto hb = open(f, b);
        add(f, ha);
        add(f, hb);
        send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 1, 10, 0}});
        auto work = turn();
        auto staged = f.queue.progress(work);
        REQUIRE(staged);
        REQUIRE(staged->kind == m::QueueProgressKind::STAGED);
        allocationAllowance = std::numeric_limits<std::size_t>::max();
        allocationRequests = 0;
        std::expected<m::QueueProgress, m::QueueFailure> result =
            std::unexpected(m::QueueFailure{m::QueueError::INVALID_STATE, {}});

        for (unsigned n = 0; n < 100; ++n) {
            work = turn();
            work.fields = fields;

            if (fields == 1000000)
                work.stream.operations = 1;

            allocationFault = true;
            result = f.queue.progress(work);
            allocationFault = false;

            if (!result || result->kind == m::QueueProgressKind::ROUTED)
                break;
        }

        REQUIRE_FALSE(result);
        CHECK(result.error().error == m::QueueError::WORK_LIMIT);

        if (fields != 1000000 && allocationRequests >= 6)
            observedLateCopyFailure = true;

        CHECK(a->values.empty());
        CHECK(b->values.empty());
        CHECK(f.queue.resources().dataItems == 0);
        CHECK(f.queue.resources(ha)->queuedRecords == 0);
        CHECK(f.queue.resources(hb)->queuedRecords == 0);
        until(f, [&] {
            return a->values.size() == 2 && b->values.size() == 2;
        });
        CHECK(a->values == b->values);
    }

    CHECK(observedLateCopyFailure);
}

TEST_CASE("MUX queue final callable retirement rejects recursive owner mutation before terminal publication") {
    struct Reentrant final : m::MuxConsumer {
        m::MuxQueue *owner = nullptr;         ///< Still-live execution owner during final callable retirement.
        std::atomic<bool> *refused = nullptr; ///< Observed reentrancy diagnostic.

        ~Reentrant() override {
            auto work = turn();
            auto result = owner->resetSource(work);
            refused->store(!result && result.error().error == m::QueueError::REENTRANT);
        }

        m::BatchAction onBatch(const m::BorrowedMuxBatch &) override {
            return m::BatchAction::CONTINUE;
        }
    };

    std::atomic<bool> refused = false;
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto consumer = std::make_shared<Reentrant>();
    consumer->owner = &f.queue;
    consumer->refused = &refused;
    auto work = turn();
    auto result = f.queue.open(clientLimits(), consumer, work);
    REQUIRE(result);
    auto handle = std::move(*result);
    consumer.reset();
    CHECK(handle.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return handle.terminal().has_value();
    });
    CHECK(refused.load());
    CHECK(f.queue.resetStatus().phase == m::ResetPhase::NONE);
}

TEST_CASE("MUX queue fallback final callable retirement rejects owner mutation after source owners dispose") {
    struct Retiring final : m::MuxConsumer {
        m::MuxQueue *owner = nullptr;         ///< Still-live wrapper during serialized state destruction.
        std::atomic<bool> *refused = nullptr; ///< Recursive mutation outcome.

        ~Retiring() override {
            auto work = turn();
            auto result = owner->resetSource(work);
            refused->store(!result && result.error().error == m::QueueError::REENTRANT);
        }

        m::BatchAction onBatch(const m::BorrowedMuxBatch &) override {
            FAIL("fallback must not invoke delivery callbacks");

            return m::BatchAction::CONTINUE;
        }
    };

    std::atomic<bool> refused = false;
    std::optional<Fixture> f = fixture(c::SubscriptionFamily::TICKER);
    auto consumer = std::make_shared<Retiring>();
    consumer->owner = &f->queue;
    consumer->refused = &refused;
    std::weak_ptr<Retiring> weak = consumer;
    auto work = turn();
    auto opened = f->queue.open(clientLimits(), consumer, work);
    REQUIRE(opened);
    auto handle = std::move(*opened);
    consumer.reset();
    f.reset();
    CHECK(refused.load());
    CHECK(weak.expired());
    REQUIRE(handle.terminal());
    CHECK(handle.terminal()->outcome == m::TerminalOutcome::ABORTED);
}

TEST_CASE("MUX queue detached backing remains charged until actual producer-thread allocation release completes") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto consumer = std::make_shared<Recorder>();
    consumer->retain = true;
    auto handle = open(f, consumer);
    add(f, handle);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return consumer->retained.size() == 1;
    });
    auto batch = std::move(consumer->retained[0]);
    consumer->retained.clear();
    const auto before = f.queue.resources(handle);
    REQUIRE(before);
    REQUIRE(before->recordOwners == 1);
    REQUIRE(before->batchOwners == 1);
    heldDeletePointer = const_cast<char16_t *>(batch.records()[0].symbol.data());
    deleteEntered = false;
    deleteContinue = false;
    std::atomic<bool> finished = false;
    std::thread release([value = std::move(batch), &finished]() mutable {
        value = {};
        finished = true;
    });

    for (unsigned n = 0; n < 1000000 && !deleteEntered.load() && !finished.load(); ++n)
        std::this_thread::yield();

    const bool observed = deleteEntered.load();
    const auto during = f.queue.resources(handle);
    deleteContinue = true;
    release.join();
    heldDeletePointer = nullptr;
    REQUIRE(observed);
    REQUIRE(during);
    CHECK(during->recordOwners == 1);
    CHECK(during->recordBytes == before->recordBytes);
    CHECK(during->batchOwners == 1);
    CHECK(during->batchBytes > 0);
    CHECK(during->batchBytes <= before->batchBytes);
    CHECK(f.queue.resources(handle)->recordOwners == 0);
    CHECK(f.queue.resources(handle)->batchOwners == 0);
}

TEST_CASE("MUX queue fixed and generation exact resident boundaries include measured STL control requests") {
    auto first = fixture(c::SubscriptionFamily::STREAM);
    const auto fixed = first.queue.resources().fixedBytes;
    REQUIRE(fixed > 0);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(first, consumer);
    const auto individual = first.queue.resources(h);
    REQUIRE(individual);
    const auto clientFixed = individual->fixedReservationBytes;

    for (const bool oneShort : {false, true}) {
        auto local = schema();
        auto bounds = queueLimits();
        bounds.residentBytes = fixed - (oneShort ? 1 : 0);
        auto work = turn();
        allocationRequests = 0;
        allocationAllowance = 0;
        allocationFault = oneShort;
        auto owner = m::MuxQueue::createStream(local, muxLimits(), desiredLimits(),
                                               {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, bounds, work);
        allocationFault = false;

        if (oneShort) {
            REQUIRE_FALSE(owner);
            CHECK(owner.error().error == m::QueueError::RESOURCE_LIMIT);
            CHECK(allocationRequests == 0);
        } else {
            REQUIRE(owner);
            CHECK(owner->resources().fixedBytes == fixed);
            CHECK(owner->resources().reservedBytes == fixed);
        }
    }

    for (const bool oneShort : {false, true}) {
        auto bounds = queueLimits();
        const auto limit = clientLimits();
        bounds.residentBytes = fixed + clientFixed + limit.recordBytes + limit.batchBytes - (oneShort ? 1 : 0);
        auto f = fixture(c::SubscriptionFamily::STREAM, bounds);
        auto receiver = std::make_shared<Recorder>();
        auto work = turn();
        allocationRequests = 0;
        allocationAllowance = 0;
        allocationFault = oneShort;
        auto result = f.queue.open(limit, receiver, work);
        allocationFault = false;

        if (oneShort) {
            REQUIRE_FALSE(result);
            CHECK(result.error().error == m::QueueError::RESOURCE_LIMIT);
            CHECK(allocationRequests == 0);
            CHECK(f.queue.resources().generations == 0);
            CHECK(f.queue.resources().reservedBytes == fixed);
        } else {
            REQUIRE(result);
            CHECK(f.queue.resources().reservedBytes == bounds.residentBytes);
            CHECK(f.queue.resources(*result)->fixedReservationBytes == clientFixed);
        }
    }
}

TEST_CASE("MUX queue exact and one-short batch metadata bytes include actual control roots and field views") {
    auto first = fixture(c::SubscriptionFamily::STREAM);
    auto receiver = std::make_shared<Recorder>();
    receiver->retain = true;
    auto h = open(first, receiver);
    add(first, h);
    send(first, {{0, u"ALPHA", 1, 10, 0}});
    until(first, [&] {
        return receiver->retained.size() == 1;
    });
    const auto sampled = first.queue.resources(h);
    REQUIRE(sampled);
    const auto exact = sampled->batchBytes;
    REQUIRE(exact > 0);

    for (const bool oneShort : {false, true}) {
        auto f = fixture(c::SubscriptionFamily::STREAM);
        auto limited = std::make_shared<Recorder>();
        auto healthy = std::make_shared<Recorder>();
        auto limits = clientLimits();
        limits.batchBytes = exact - (oneShort ? 1 : 0);
        auto a = open(f, limited, limits);
        auto b = open(f, healthy);
        add(f, a);
        add(f, b);
        send(f, {{0, u"ALPHA", 1, 10, 0}});
        until(f, [&] {
            return healthy->values.size() == 1 && (a.terminal().has_value() || limited->values.size() == 1);
        });
        CHECK(limited->values.size() == (oneShort ? 0 : 1));

        if (oneShort) {
            REQUIRE(a.terminal());
            CHECK(a.terminal()->primary->error == m::QueueError::RESOURCE_LIMIT);
        } else
            CHECK_FALSE(a.terminal());
    }
}

TEST_CASE("MUX queue caller schema can die while actual components and the pure context table retain provenance") {
    std::optional<c::LocalSchema> caller = schema();
    auto source = session(*caller);
    const auto context = caller->recordContext(0);
    auto queue = queueFor(*caller, c::SubscriptionFamily::STREAM);
    caller.reset();
    auto consumer = std::make_shared<Recorder>();
    auto work = turn();
    auto handle = queue.open(clientLimits(), consumer, work);
    REQUIRE(handle);
    const c::SubscriptionRequest request{{context, u"ALPHA"}, {}};
    auto sw = submitTurn();
    auto ticket = handle->add(std::span(&request, 1), sw);
    REQUIRE(ticket);

    for (unsigned n = 0; n < 1000 && (!ticket->result() || handle->state() != m::QueueClientState::ACTIVE); ++n) {
        work = turn();
        REQUIRE(queue.progress(work));
    }

    REQUIRE(ticket->result());
    REQUIRE(handle->state() == m::QueueClientState::ACTIVE);
    const std::array<Input, 1> input{{{0, u"ALPHA", 1, 10, 0}}};
    auto event = admit(source, data(std::span<const Input>(input), 15));
    bool accepted = false;

    for (unsigned n = 0; n < 1000 && !accepted; ++n) {
        work = turn();
        auto result = queue.admit(std::move(event), work);

        if (result)
            accepted = true;
        else {
            REQUIRE(result.error().error == m::QueueError::BUSY);
            REQUIRE(queue.progress(work));
        }
    }

    REQUIRE(accepted);

    for (unsigned n = 0; n < 1000 && consumer->values.empty(); ++n) {
        work = turn();
        REQUIRE(queue.progress(work));
    }

    CHECK(consumer->values == std::vector<std::int64_t>{10});
}

TEST_CASE("MUX queue empty fair slots prepay cursor phase and inline metadata writes") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto work = turn();
    work.visits = 0;
    work.bytes = 0;
    auto result = f.queue.progress(work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == m::QueueError::WORK_LIMIT);
    work = turn();
    auto retry = f.queue.progress(work);
    REQUIRE(retry);
    CHECK(retry->kind == m::QueueProgressKind::IDLE);
}

TEST_CASE("MUX queue late STREAM membership cannot inherit an already accepted old frame prefix") {
    auto bounds = queueLimits();
    bounds.routeRecords = 1;
    auto f = fixture(c::SubscriptionFamily::STREAM, bounds);
    auto a = std::make_shared<Recorder>();
    auto b = std::make_shared<Recorder>();
    auto first = open(f, a);
    add(f, first);
    send(f, {{0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 2, 11, 0}, {0, u"ALPHA", 3, 12, 0}});
    until(f, [&] {
        return a->values.size() == 1;
    });
    auto late = open(f, b);
    add(f, late);
    until(f, [&] {
        return a->values.size() == 3;
    });
    CHECK(b->values.empty());
    send(f, {{0, u"ALPHA", 4, 13, 0}});
    until(f, [&] {
        return a->values.size() == 4 && b->values.size() == 1;
    });
    CHECK(a->values == std::vector<std::int64_t>{10, 11, 12, 13});
    CHECK(b->values == std::vector<std::int64_t>{13});
}

TEST_CASE(
    "MUX queue shared TICKER late view receives the actual retained latest without replaying intermediate values") {
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto a = std::make_shared<Recorder>();
    auto b = std::make_shared<Recorder>();
    auto first = open(f, a);
    add(f, first);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return a->values.size() == 1;
    });
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return a->values.size() == 2;
    });
    auto late = open(f, b);
    add(f, late);
    until(f, [&] {
        return b->values.size() == 1;
    });
    CHECK(b->values == std::vector<std::int64_t>{20});
    CHECK(a->values == std::vector<std::int64_t>{10, 20});
}

TEST_CASE("MUX queue independent contexts foreign input and fresh source epochs preserve precise refusal scope") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto independent = schema();
    auto other = session(independent);
    const std::array<Input, 1> input{{{0, u"ALPHA", 1, 10, 0}}};
    auto foreign = admit(other, data(std::span<const Input>(input), 15));
    auto work = turn();
    auto rejected = f.queue.admit(std::move(foreign), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == m::QueueError::INVALID_CONTEXT);
    CHECK(foreign.residentBytes() > 0);
    CHECK(f.queue.state() == m::QueueState::OPEN);
    CHECK(f.queue.resources().pendingRecords == 0);
    auto consumer = std::make_shared<Recorder>();
    auto h = open(f, consumer);
    add(f, h);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return consumer->values.size() == 1;
    });
    auto fresh = session(f.schema);
    auto event = admit(fresh, data(std::span<const Input>(input), 15));
    bool observed = false;

    for (unsigned n = 0; n < 1000 && !observed; ++n) {
        work = turn();
        auto result = f.queue.admit(std::move(event), work);
        REQUIRE_FALSE(result);

        if (result.error().error == m::QueueError::BUSY) {
            REQUIRE(f.queue.progress(work));
        } else {
            CHECK(result.error().error == m::QueueError::INVALID_EPOCH);
            observed = true;
        }
    }

    CHECK(observed);
    CHECK(event.residentBytes() > 0);
    CHECK(consumer->values == std::vector<std::int64_t>{10});
}

TEST_CASE("MUX queue HISTORY range query prepays every queued exact-symbol comparison and preserves paused delivery on "
          "WORK") {
    auto f = fixture(c::SubscriptionFamily::HISTORY);
    auto consumer = std::make_shared<Recorder>();
    consumer->pause = true;
    auto handle = open(f, consumer);
    add(f, handle);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    until(f, [&] {
        return handle.state() == m::QueueClientState::PAUSED;
    });

    for (std::int32_t value = 31; value <= 44; ++value) {
        send(f, {{0, u"ALPHA", 3, value, 0}});

        for (unsigned n = 0; n < 100; ++n) {
            auto work = turn();
            REQUIRE(f.queue.progress(work));
        }
    }

    const auto before = f.queue.resources(handle);
    REQUIRE(before);
    REQUIRE(before->queuedRecords == 15);
    CHECK_FALSE(handle.terminal());
    const c::SubscriptionKey key{f.schema.recordContext(0), u"ALPHA"};
    auto shortWork = turn();
    shortWork.textUnits = 3 * key.symbol.size();
    auto rejected = f.queue.rangeStatus(handle, key, shortWork);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == m::QueueError::WORK_LIMIT);
    CHECK(f.queue.resources(handle)->queuedRecords == before->queuedRecords);
    CHECK(consumer->values.size() == 1);
    auto full = turn();
    const auto available = full.textUnits;
    auto inspected = f.queue.rangeStatus(handle, key, full);
    REQUIRE(inspected);
    const auto measured = available - full.textUnits;
    CHECK(measured >= before->queuedRecords * key.symbol.size());
    CHECK(inspected->queuedForKey == before->queuedRecords);
    CHECK_FALSE(inspected->completeDeliveredRange);
    auto exact = turn();
    exact.textUnits = measured;
    REQUIRE(f.queue.rangeStatus(handle, key, exact));
    CHECK(exact.textUnits == 0);
    auto oneShort = turn();
    oneShort.textUnits = measured - 1;
    auto boundary = f.queue.rangeStatus(handle, key, oneShort);
    REQUIRE_FALSE(boundary);
    CHECK(boundary.error().error == m::QueueError::WORK_LIMIT);
    CHECK(f.queue.resources(handle)->queuedRecords == before->queuedRecords);
    CHECK(handle.requestResume() == m::ControlAdmission::RECORDED);
    consumer->pause = false;
    until(f, [&] {
        return consumer->values.size() == 16;
    });
    CHECK(f.queue.resources(handle)->queuedRecords == 0);
}

TEST_CASE(
    "MUX queue aggregate reserved controls reject zero and one then recover exact two after retired result release") {
    for (const std::size_t capacity : {0u, 1u, 2u}) {
        auto limits = queueLimits();
        limits.controlSlots = capacity;
        auto f = fixture(c::SubscriptionFamily::STREAM, limits);
        auto consumer = std::make_shared<Recorder>();
        const auto baseline = f.queue.resources();
        auto work = turn();
        auto result = f.queue.open(clientLimits(), consumer, work);

        if (capacity < 2) {
            REQUIRE_FALSE(result);
            CHECK(result.error().error == m::QueueError::RESOURCE_LIMIT);
            CHECK(f.queue.resources().generations == 0);
            CHECK(f.queue.resources().reservedControlSlots == 0);
            CHECK(f.queue.resources().reservedBytes == baseline.reservedBytes);
            continue;
        }

        REQUIRE(result);
        auto handle = std::move(*result);
        CHECK(f.queue.resources().reservedControlSlots == 2);
        const auto reserved = f.queue.resources().reservedBytes;
        work = turn();
        allocationAllowance = 0;
        allocationRequests = 0;
        allocationFault = true;
        auto exceeded = f.queue.open(clientLimits(), consumer, work);
        allocationFault = false;
        REQUIRE_FALSE(exceeded);
        CHECK(exceeded.error().error == m::QueueError::RESOURCE_LIMIT);
        CHECK(allocationRequests == 0);
        CHECK(f.queue.resources().reservedBytes == reserved);
        CHECK(handle.state() == m::QueueClientState::ACTIVE);
        auto submit = submitTurn();
        auto accepted = handle.clear(submit);
        REQUIRE(accepted);
        m::MuxCommandTicket ticket = std::move(*accepted);
        until(f, [&] {
            return ticket.result().has_value();
        });
        CHECK(ticket.result()->status == m::CommandStatus::APPLIED);
        CHECK(handle.requestStop(m::StopMode::ABORT) == m::ControlAdmission::RECORDED);
        until(f, [&] {
            return handle.terminal().has_value();
        });
        handle = {};

        for (unsigned n = 0; n < 40; ++n) {
            work = turn();
            REQUIRE(f.queue.progress(work));
        }

        CHECK(f.queue.resources().generations == 1);
        CHECK(f.queue.resources().reservedControlSlots == 2);
        CHECK(f.queue.resources().reservedBytes == reserved);
        const auto completed = *ticket.result();
        ticket = {};
        until(f, [&] {
            return f.queue.resources().generations == 0;
        });
        CHECK(completed.status == m::CommandStatus::APPLIED);
        CHECK(f.queue.resources().reservedControlSlots == 0);
        CHECK(f.queue.resources().reservedBytes == baseline.reservedBytes);
        work = turn();
        auto reopened = f.queue.open(clientLimits(), consumer, work);
        REQUIRE(reopened);
        CHECK(f.queue.resources().reservedControlSlots == 2);
    }
}

TEST_CASE("MUX queue reserved resume and graceful stop work with an actually full one-slot ordinary ring") {
    auto f = fixture(c::SubscriptionFamily::TICKER);
    auto consumer = std::make_shared<Recorder>();
    consumer->pause = true;
    auto limits = clientLimits();
    limits.commandSlots = 1;
    limits.resultSlots = 2;
    auto handle = open(f, consumer, limits);
    add(f, handle);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return handle.state() == m::QueueClientState::PAUSED;
    });
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    until(f, [&] {
        return f.queue.resources(handle)->queuedRecords == 1;
    });
    const c::SubscriptionRequest request{{f.schema.recordContext(0), u"ALPHA"}, {}};
    auto submit = submitTurn();
    auto first = handle.add(std::span(&request, 1), submit);
    REQUIRE(first);
    submit = submitTurn();
    auto full = handle.add(std::span(&request, 1), submit);
    REQUIRE_FALSE(full);
    CHECK(full.error() == m::QueueError::COMMAND_LIMIT);
    CHECK(f.queue.resources(handle)->queuedCommands == 1);
    consumer->pause = false;
    CHECK(handle.requestResume() == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return consumer->values.size() == 2 && first->result().has_value();
    });
    CHECK(first->result()->status == m::CommandStatus::APPLIED);
    CHECK_FALSE(first->result()->changed);
    CHECK(consumer->values == std::vector<std::int64_t>{10, 20});
    submit = submitTurn();
    auto pending = handle.add(std::span(&request, 1), submit);
    REQUIRE(pending);
    CHECK(f.queue.resources(handle)->queuedCommands == 1);
    CHECK(handle.requestStop(m::StopMode::GRACEFUL) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return handle.terminal().has_value();
    });
    REQUIRE(pending->result());
    CHECK(pending->result()->status == m::CommandStatus::CANCELLED_CLOSING);
    CHECK(handle.terminal()->outcome == m::TerminalOutcome::DRAINED);
    CHECK(handle.terminal()->counts.commandsCancelled == 1);
    CHECK(consumer->values == std::vector<std::int64_t>{10, 20});
}

TEST_CASE("MUX source binding validates exact schema family policy and future count capacities") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto f = fixture(family);
        auto work = turn();
        REQUIRE(f.queue.validateBinding(f.schema, family, c::SubscriptionSelectorPolicy::EXACT_ONLY, work));
        const auto caps = f.queue.capacity();
        CHECK(caps.queueBytes == queueLimits().residentBytes);
        CHECK(caps.muxBytes == muxLimits().residentBytes);
        CHECK(caps.desiredOwners == (family == c::SubscriptionFamily::STREAM ? 1 : 4));
        CHECK(caps.desiredPerOwner.peakEntries == desiredLimits().peakEntries);
        CHECK(caps.desiredPerOwner.peakSymbolUnits == desiredLimits().peakSymbolUnits);
        auto foreign = schema();
        CHECK(f.queue.validateBinding(foreign, family, c::SubscriptionSelectorPolicy::EXACT_ONLY, work).error().error ==
              m::QueueError::INVALID_CONTEXT);
        const auto wrongFamily =
            family == c::SubscriptionFamily::STREAM ? c::SubscriptionFamily::TICKER : c::SubscriptionFamily::STREAM;
        CHECK(f.queue.validateBinding(f.schema, wrongFamily, c::SubscriptionSelectorPolicy::EXACT_ONLY, work)
                  .error()
                  .error == m::QueueError::INVALID_CONFIGURATION);
        auto shortWork = turn();
        shortWork.bytes = sizeof(c::SchemaIdentity) - 1;
        CHECK(f.queue.validateBinding(f.schema, family, c::SubscriptionSelectorPolicy::EXACT_ONLY, shortWork)
                  .error()
                  .error == m::QueueError::WORK_LIMIT);
        auto receiver = std::make_shared<Recorder>();
        auto handle = open(f, receiver);
        add(f, handle);
        const auto revision = f.queue.revision();
        REQUIRE(f.queue.validateBinding(f.schema, family, c::SubscriptionSelectorPolicy::EXACT_ONLY, work));
        CHECK(f.queue.revision() == revision);
        send(f, {{0, u"ALPHA", 1, 10, 0}});
        CHECK(
            f.queue.validateBinding(f.schema, family, c::SubscriptionSelectorPolicy::EXACT_ONLY, work).error().error ==
            m::QueueError::INVALID_STATE);
    }

    auto empty = c::LocalSchema::create({}, {16, 128, 256, 128, 16384});
    auto independent = c::LocalSchema::create({}, {16, 128, 256, 128, 16384});
    REQUIRE(empty);
    REQUIRE(independent);
    auto work = turn();
    auto queue = m::MuxQueue::createStream(*empty, muxLimits(), desiredLimits(),
                                           {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, queueLimits(), work);
    REQUIRE(queue);
    REQUIRE(
        queue->validateBinding(*empty, c::SubscriptionFamily::STREAM, c::SubscriptionSelectorPolicy::EXACT_ONLY, work));
    CHECK(queue
              ->validateBinding(*independent, c::SubscriptionFamily::STREAM, c::SubscriptionSelectorPolicy::EXACT_ONLY,
                                work)
              .error()
              .error == m::QueueError::INVALID_CONTEXT);
    auto moved = std::move(*queue);
    CHECK(queue->capacity().queueBytes == 0);
    CHECK(queue->validateBinding(*empty, c::SubscriptionFamily::STREAM, c::SubscriptionSelectorPolicy::EXACT_ONLY, work)
              .error()
              .error == m::QueueError::INVALID_STATE);
    REQUIRE(
        moved.validateBinding(*empty, c::SubscriptionFamily::STREAM, c::SubscriptionSelectorPolicy::EXACT_ONLY, work));
}

TEST_CASE("MUX STREAM reset preserves handles old detached values and exact client ACK gates") {
    auto f = fixture(c::SubscriptionFamily::STREAM);
    auto receiver = std::make_shared<Recorder>();
    receiver->retain = true;
    auto handle = open(f, receiver);
    add(f, handle);
    send(f, {{0, u"ALPHA", 1, 10, 0}});
    until(f, [&] {
        return receiver->retained.size() == 1;
    });
    const auto oldEpoch = receiver->retained[0].epoch();
    const auto oldBytes = f.queue.resources().batchBytes;
    REQUIRE(oldBytes > 0);
    send(f, {{0, u"ALPHA", 2, 20, 0}});
    auto work = turn();
    REQUIRE(f.queue.resetSource(work));
    until(f, [&] {
        return f.queue.resetStatus().phase == m::ResetPhase::NOTICE_PUBLICATION_PENDING;
    });
    const auto committed = f.queue.resetStatus();
    REQUIRE(committed.sourceCommitted);
    REQUIRE(committed.actualSourceSequence);
    CHECK(committed.globalAcknowledged);
    CHECK(committed.discardedPendingFrames == 1);
    work = turn();
    work.bytes = 0;
    const auto blocked = f.queue.progress(work);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error().error == m::QueueError::WORK_LIMIT);
    CHECK(f.queue.resetStatus().actualSourceSequence == committed.actualSourceSequence);
    until(f, [&] {
        return f.queue.resetStatus().phase == m::ResetPhase::NONE;
    });
    auto notice = handle.resetNotice();
    REQUIRE(notice);
    REQUIRE(notice->toEpoch);
    CHECK_FALSE(notice->toEpoch->matches(oldEpoch));
    CHECK_FALSE(notice->globalAckPending);
    CHECK(handle.acknowledgeReset(notice->sequence + 1) == m::ControlAdmission::STALE);
    CHECK(receiver->retained[0].records()[0].typedValues[2].raw == 10);
    CHECK(f.queue.resources().batchBytes >= oldBytes);

    f.live = session(f.schema);
    send(f, {{0, u"ALPHA", 3, 30, 0}});
    constexpr unsigned PRE_ACK_TURNS = 100; ///< Finite fair cycles with an intentionally unacknowledged reset.

    for (unsigned i = 0; i < PRE_ACK_TURNS; ++i) {
        work = turn();
        REQUIRE(f.queue.progress(work));
    }

    CHECK(receiver->values == std::vector<std::int64_t>{10});
    CHECK(handle.acknowledgeReset(notice->sequence) == m::ControlAdmission::RECORDED);
    until(f, [&] {
        return receiver->values.size() == 2;
    });
    CHECK(receiver->values == std::vector<std::int64_t>{10, 30});
    CHECK_FALSE(handle.resetNotice());
    CHECK(receiver->retained[1].epoch().matches(*notice->toEpoch));
    const auto retainedCurrentBytes = f.queue.resources().batchBytes;
    receiver->retained.erase(receiver->retained.begin());
    CHECK(f.queue.resources().batchBytes < retainedCurrentBytes);
    CHECK(receiver->retained[0].records()[0].typedValues[2].raw == 30);
}
