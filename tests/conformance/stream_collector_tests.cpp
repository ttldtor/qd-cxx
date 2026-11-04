// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/collectors/stream.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace e = org::ttldtor::qdcxx::encoding;
namespace s = org::ttldtor::qdcxx::collectors;
namespace q = org::ttldtor::qdcxx::events;
using Error = s::StreamError;
bool failAllocations = false;         ///< Enabled only around an explicit production operation.
std::size_t allocationsRemaining = 0; ///< Successful ordinary allocation requests allowed before failure.

/** Narrow allocation failure injection, disabled before any assertion executes. */
struct AllocationFault {
    /** Arm one bounded production operation.
     * @param count Successful requests permitted before throwing.
     */
    explicit AllocationFault(std::size_t count) {
        allocationsRemaining = count;
        failAllocations = true;
    }

    /** Restore normal test-framework allocation. */
    ~AllocationFault() {
        failAllocations = false;
    }
};

/** Supply independent finite turn credits, never production defaults.
 * @return Credits for one explicitly selected test turn.
 */
s::StreamWork turn() {
    return {10000, 1000000, 1000000, 100000000};
}

/** Supply independent desired-state command credits.
 * @return Finite command/matching/copy credits.
 */
c::SubscriptionWork command() {
    return {10000, 1000000, 1000000};
}

/** Supply independent live admission credits.
 * @return Finite protocol work allowance.
 */
p::LiveWork liveTurn() {
    return {10000, 10000000, 10000000, 10000000};
}

/** Build explicit generous collector bounds for the test profile.
 * @return Finite frame/item/backing maxima.
 */
s::StreamLimits limits() {
    return {8, 64, 1024 * 1024, 2 * 1024 * 1024};
}

/** Configure complete native live admission, independent of tape lifetime quotas.
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
    result.eventBytes = 65536;
    result.outputBytes = 8192;
    result.credentials = p::LiveCredentialPolicy::CREDENTIAL_FREE_PLAIN;
    result.fieldTime = p::FieldTimePolicy::DISABLED;
    result.events = p::DataEventPolicy::ZERO_FLAGS;
    result.historySnapshot = false;

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

/** Build the real desired-state owner used by collector admission.
 * @param source Immutable schema provenance.
 * @param family Selected contract for positive and rejection tests.
 * @return Fresh usable desired state.
 */
c::SubscriptionState state(const c::LocalSchema &source, c::SubscriptionFamily family = c::SubscriptionFamily::STREAM) {
    auto result = c::SubscriptionState::create(source, family, {2, 16, 128, 1024, 32, 2048, 32});
    REQUIRE(result);

    return std::move(*result);
}

/** Construct fixed collector backing before transferring desired ownership.
 * @param desired Usable desired state.
 * @param bounds Explicit finite collector capacities.
 * @return Fresh collector.
 */
s::StreamCollector collector(c::SubscriptionState &&desired, s::StreamLimits bounds = limits()) {
    auto work = turn();
    auto result = s::StreamCollector::create(std::move(desired), bounds, work);
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
    number(payload, 2);
    number(payload, 15);
    text(payload, u"STREAM_DATA");
    number(payload, 0);
    number(payload, 10);
    text(payload, u"TICKER_DATA");
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
};

/** Compose selected source tuples through actual symbol/compact codecs.
 * @param inputs Complete ordered records.
 * @param type Exact data family, STREAM_DATA by default.
 * @return Whole native admission frame.
 */
std::vector<std::byte> data(std::span<const Input> inputs, std::int32_t type = 15) {
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
 * @return Ready native live session with both complete sender descriptions.
 */
p::LiveSession session(const c::LocalSchema &source) {
    auto copied = source;
    const p::OutgoingRecord declarations[]{
        {41, 0, u"Oracle#ONE", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}},
        {7, 1, u"Oracle#TWO", {{u"TimeHigh", 8, 0}, {u"TimeLow", 8, 1}, {u"Value", 8, 2}}}};
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(source, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copied), std::move(*writer), liveLimits(), work);
    REQUIRE(result);
    (void)admit(*result, ready());
    (void)admit(*result, description(41, 0));
    (void)admit(*result, description(7, 1));

    return std::move(*result);
}

/** Complete independent test owner graph. */
struct Fixture {
    c::LocalSchema schema;        ///< Source provenance for desired keys and further independent sessions.
    p::LiveSession session;       ///< Whole-frame admission owner.
    s::StreamCollector collector; ///< Desired state and bounded delivery staging.
};

/** Build a fresh ready profile with no desired entries.
 * @param bounds Collector capacities.
 * @return Independent source/session/collector owner graph.
 */
Fixture fixture(s::StreamLimits bounds = limits()) {
    auto local = schema();
    auto live = session(local);
    auto stream = collector(state(local), bounds);

    return {std::move(local), std::move(live), std::move(stream)};
}

/** Add one exact desired key.
 * @param f Complete test owner graph.
 * @param symbol Exact non-null units.
 * @param record Local record identity.
 */
void add(Fixture &f, std::u16string_view symbol, std::int32_t record = 0) {
    const c::SubscriptionRequest request{{f.schema.recordContext(record), symbol}, {}};
    auto work = command();
    REQUIRE(f.collector.add(std::span(&request, 1), work));
}

/** Remove one exact desired key.
 * @param f Complete test owner graph.
 * @param symbol Exact units.
 * @param record Local record identity.
 */
void remove(Fixture &f, std::u16string_view symbol, std::int32_t record = 0) {
    const c::SubscriptionKey key{f.schema.recordContext(record), symbol};
    auto work = command();
    REQUIRE(f.collector.remove(std::span(&key, 1), work));
}

/** Admit and stage actual native wire input.
 * @param f Complete test owner graph.
 * @param inputs Complete ordered source tuples.
 * @return Actual collector admission result.
 */
s::StreamStage stage(Fixture &f, std::span<const Input> inputs) {
    auto event = admit(f.session, data(inputs));
    auto work = turn();
    auto result = f.collector.stage(std::move(event), work);
    REQUIRE(result);

    return *result;
}

/** Drain actual offered values with explicit acknowledgement.
 * @param stream Bounded collector.
 * @return Source-ordered compact value field observations.
 */
std::vector<std::int32_t> drain(s::StreamCollector &stream) {
    std::vector<std::int32_t> values;
    auto work = turn();

    for (;;) {
        auto next = stream.front(work);
        REQUIRE(next);

        if (!*next) {
            break;
        }

        const auto *record = (**next).record;
        REQUIRE(record->typedValues.size() == 3);
        values.push_back(static_cast<std::int32_t>(record->typedValues[2].raw));
        REQUIRE(stream.pop(work));
    }

    return values;
}
} // namespace

/** Replace ordinary allocation only to exercise failure boundaries in explicit production calls.
 * @param bytes Requested backing.
 * @return Ordinary allocated storage or std::bad_alloc.
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
 * @param pointer Previously allocated pointer.
 */
void operator delete(void *pointer) noexcept {
    std::free(pointer);
}

/** Release ordinary sized replacement allocation.
 * @param pointer Previously allocated pointer.
 * @param bytes Original requested size, not required by free.
 */
void operator delete(void *pointer, std::size_t bytes) noexcept {
    static_cast<void>(bytes);
    std::free(pointer);
}

/** Keep array requests in the same narrow ordinary-allocation fault domain.
 * @param bytes Requested array backing.
 * @return Ordinary allocated storage or std::bad_alloc.
 */
void *operator new[](std::size_t bytes) {
    return ::operator new(bytes);
}

/** Release replacement array allocation symmetrically.
 * @param pointer Previously allocated array backing.
 */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** Release sized replacement array allocation symmetrically.
 * @param pointer Previously allocated array backing.
 * @param bytes Original requested size.
 */
void operator delete[](void *pointer, std::size_t bytes) noexcept {
    ::operator delete(pointer, bytes);
}

TEST_CASE("STREAM preserves identical consecutive records and interleaved source order without a cache") {
    auto f = fixture();
    const Input before[]{{0, u"ALPHA", 1, 5}};
    CHECK(stage(f, before).selected == 0);
    add(f, u"ALPHA");
    add(f, u"BETA");
    CHECK(drain(f.collector).empty());
    const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20},
                        {1, u"ALPHA", 1, 99}, {0, u"OTHER", 1, 88}, {0, u"ALPHA", 1, 11}};
    const auto result = stage(f, batch);
    CHECK(result.selected == 4);
    CHECK(result.ignored == 2);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10, 10, 20, 11});
    CHECK(f.collector.counts().ignored == 3);
    CHECK(f.collector.counts().delivered == 4);
    CHECK(f.collector.stagedBytes() == 0);
}

TEST_CASE("STREAM revision barriers withdraw retired activations without resurrecting remove-readd data") {
    auto f = fixture();
    add(f, u"ALPHA");
    add(f, u"BETA");
    const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {0, u"ALPHA", 1, 11}};
    (void)stage(f, batch);
    auto work = turn();
    auto offered = f.collector.front(work);
    REQUIRE((offered && *offered));
    CHECK((**offered).revision == 2);
    remove(f, u"ALPHA");
    add(f, u"ALPHA");
    CHECK(f.collector.pop(work).error() == Error::NO_OFFER);
    const Input later[]{{0, u"ALPHA", 2, 12}};
    CHECK(stage(f, later).revision == 4);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{20, 12});
    CHECK(f.collector.counts().withdrawn == 2);
    CHECK(f.collector.counts().delivered == 2);
}

TEST_CASE("STREAM no-op rejected and effective desired commands preserve exactly their promised offers") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
    (void)stage(f, batch);
    auto work = turn();
    auto first = f.collector.front(work);
    REQUIRE((first && *first));
    add(f, u"ALPHA");
    remove(f, u"ABSENT");
    const c::SubscriptionRequest invalid{{f.schema.recordContext(0), u"*"}, {}};
    auto desiredWork = command();
    CHECK_FALSE(f.collector.add(std::span(&invalid, 1), desiredWork));
    CHECK(f.collector.pop(work));
    auto next = f.collector.front(work);
    REQUIRE((next && *next));
    const c::SubscriptionRequest replacement{{f.schema.recordContext(0), u"BETA"}, {}};
    REQUIRE(f.collector.replace(std::span(&replacement, 1), desiredWork));
    CHECK(f.collector.pop(work).error() == Error::NO_OFFER);
    CHECK(drain(f.collector).empty());
    CHECK(f.collector.counts().withdrawn == 1);
    add(f, u"ALPHA");
    (void)stage(f, batch);
    REQUIRE(f.collector.clear(desiredWork));
    CHECK(drain(f.collector).empty());
    CHECK(f.collector.counts().withdrawn == 3);
}

TEST_CASE("STREAM exact item frame and byte limits have visible terminal whole-frame overflow") {
    for (int boundary = 0; boundary < 4; ++boundary) {
        auto bounds = limits();
        auto probe = fixture();
        const Input one[]{{0, u"ALPHA", 1, 10}};
        auto probeEvent = admit(probe.session, data(one));
        const auto eventBytes = probeEvent.residentBytes();
        bounds.frames = 1;
        bounds.records = 2;
        bounds.stagedBytes = 2 * eventBytes;

        if (boundary == 1) {
            bounds.frames = 2;
            bounds.records = 1;
        } else if (boundary == 2) {
            bounds.frames = 2;
            bounds.stagedBytes = eventBytes;
        } else if (boundary == 3) {
            bounds.frames = 2;
            // Obtain the exact fixed metadata scope for these capacities separately.
            auto fixed = fixture(bounds);
            bounds.residentBytes = fixed.collector.residentBytes() + eventBytes;
        }

        auto f = fixture(bounds);
        add(f, u"ALPHA");
        auto event = admit(f.session, data(one));
        auto work = turn();
        REQUIRE(f.collector.stage(std::move(event), work));
        CHECK(event.residentBytes() == 0);
        CHECK(f.collector.stagedBytes() == eventBytes);
        auto overflow = admit(f.session, data(one));
        const auto retained = overflow.residentBytes();
        auto result = f.collector.stage(std::move(overflow), work);
        REQUIRE_FALSE(result);
        CHECK(result.error() == Error::RESOURCE_LIMIT);
        CHECK(overflow.residentBytes() == retained);
        CHECK(f.collector.queuedRecords() == 1);
        CHECK(f.collector.diagnostic().pending == 1);
        CHECK(f.collector.diagnostic().incoming == 1);
        CHECK(f.collector.front(work).error() == Error::INVALID_STATE);
        CHECK(f.collector.pop(work).error() == Error::INVALID_STATE);
        auto desiredWork = command();
        REQUIRE(f.collector.clear(desiredWork));
        CHECK(f.collector.discard(work) == 1);
        CHECK(f.collector.stagedBytes() == 0);
        CHECK(f.collector.diagnostic().error == Error::RESOURCE_LIMIT);
        CHECK(f.collector.counts().discarded == 1);
        CHECK(f.collector.counts().delivered == 0);
        CHECK(f.collector.residentBytes() <= bounds.residentBytes);
    }

    auto bounds = limits();
    bounds.records = 1;
    auto f = fixture(bounds);
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
    auto event = admit(f.session, data(batch));
    auto work = turn();
    CHECK(f.collector.stage(std::move(event), work).error() == Error::RESOURCE_LIMIT);
    CHECK(f.collector.queuedRecords() == 0);
    CHECK(f.collector.counts().staged == 0);
    CHECK(f.collector.diagnostic().incoming == 2);
    CHECK(event.records().size() == 2);
}

TEST_CASE("STREAM stage work exhaustion preserves the input queue and existing offer") {
    for (int category = 0; category < 3; ++category) {
        auto f = fixture();
        add(f, u"ALPHA");
        const Input batch[]{{0, u"ALPHA", 1, 10}};
        (void)stage(f, batch);
        auto healthy = turn();
        REQUIRE(f.collector.front(healthy));
        auto event = admit(f.session, data(batch));
        const auto *address = event.records().data();
        auto work = turn();

        if (category == 0) {
            work.operations = 0;
        } else if (category == 1) {
            work.visits = 2;
        } else {
            work.textUnits = 3;
        }

        CHECK(f.collector.stage(std::move(event), work).error() == Error::WORK_LIMIT);
        CHECK(event.records().data() == address);
        CHECK(f.collector.queuedRecords() == 1);
        CHECK_FALSE(f.collector.diagnostic().error);
        REQUIRE(f.collector.pop(healthy));
        REQUIRE(f.collector.stage(std::move(event), healthy));
        CHECK(drain(f.collector) == std::vector<std::int32_t>{10});
    }
}

TEST_CASE("STREAM release work covers retained old snapshots and is preflighted before withdrawal or pop") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}};
    auto event = admit(f.session, data(batch));
    const auto release = event.releaseBytes();
    CHECK(release > event.residentBytes());
    auto work = turn();
    REQUIRE(f.collector.stage(std::move(event), work));
    REQUIRE(f.collector.front(work));
    auto insufficient = turn();
    insufficient.bytes = 0;
    CHECK(f.collector.pop(insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.collector.queuedRecords() == 1);
    REQUIRE(f.collector.pop(work));
    CHECK(f.collector.queuedRecords() == 0);
    (void)stage(f, batch);
    remove(f, u"ALPHA");
    // The queued frame is now the sole owner of its old peer generation.
    f.session = session(f.schema);
    insufficient = turn();
    insufficient.bytes = 0;
    CHECK(f.collector.front(insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.collector.queuedRecords() == 1);
    CHECK(f.collector.counts().withdrawn == 0);
    insufficient = turn();
    insufficient.visits = 0;
    CHECK(f.collector.discard(insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.collector.queuedRecords() == 1);
    CHECK(drain(f.collector).empty());
    CHECK(f.collector.counts().withdrawn == 1);
}

TEST_CASE("STREAM allows new binding generations but keeps the pinned epoch after drain") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}};
    auto event = admit(f.session, data(batch));
    auto identity = event.snapshot().identity();
    auto work = turn();
    REQUIRE(f.collector.stage(std::move(event), work));
    (void)admit(f.session, description(41, 0));
    auto newer = admit(f.session, data(batch));
    CHECK_FALSE(identity.matches(newer.snapshot().identity()));
    CHECK(identity.sameEpoch(newer.snapshot().identity()));
    REQUIRE(f.collector.stage(std::move(newer), work));
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10, 10});
    auto other = session(f.schema);
    auto foreign = admit(other, data(batch));
    const auto bytes = foreign.residentBytes();
    CHECK(f.collector.stage(std::move(foreign), work).error() == Error::INVALID_EPOCH);
    CHECK(foreign.residentBytes() == bytes);
    CHECK(f.collector.queuedRecords() == 0);
    CHECK_FALSE(f.collector.diagnostic().error);
}

TEST_CASE("STREAM input admission owns its values after session destruction and rejects control or moved events") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}};
    auto work = turn();
    auto control = admit(f.session, ready());
    CHECK(f.collector.stage(std::move(control), work).error() == Error::INVALID_EVENT);
    CHECK(control.kind() == p::LiveEventKind::PROTOCOL);
    auto event = admit(f.session, data(batch));
    const auto *pointer = event.records().data();
    REQUIRE(f.collector.stage(std::move(event), work));
    CHECK(event.residentBytes() == 0);
    CHECK(event.releaseBytes() == 0);
    CHECK(f.collector.stage(std::move(event), work).error() == Error::INVALID_EVENT);
    auto moved = std::move(f.collector);
    CHECK(f.collector.residentBytes() == 0);
    CHECK(f.collector.desired() == nullptr);
    CHECK(f.collector.front(work).error() == Error::INVALID_STATE);
    // Replacing the source session releases its old epoch; the staged event remains its owner.
    f.session = session(f.schema);
    auto result = moved.front(work);
    REQUIRE((result && *result));
    CHECK((**result).record == pointer);
    CHECK((**result).record->typedValues[2].raw == 10);
    REQUIRE(moved.pop(work));
    CHECK(moved.stagedBytes() == 0);
}

TEST_CASE("STREAM never observes a completed decoder prefix from a failed whole live frame") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input good[]{{0, u"ALPHA", 1, 10}};
    (void)stage(f, good);
    const Input partial[]{{0, u"ALPHA", 2, 20}, {0, u"ALPHA", 3, 30}};
    auto malformed = data(partial);
    malformed.pop_back();
    // Keep framing complete while removing the last required field byte.
    malformed[0] = static_cast<std::byte>(std::to_integer<unsigned>(malformed[0]) - 1);
    auto work = liveTurn();
    auto result = f.session.feed(malformed, work);
    CHECK(result.status == p::LiveStatus::FAILED);
    CHECK_FALSE(result.event);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10});
    CHECK(f.collector.counts().staged == 1);
}

TEST_CASE("STREAM fixed rings wrap and a continuing feed has no finite lifetime quota") {
    auto bounds = limits();
    bounds.frames = 2;
    bounds.records = 3;
    auto f = fixture(bounds);
    add(f, u"ALPHA");
    const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}};
    const auto fixed = f.collector.residentBytes();

    for (int i = 0; i < 256; ++i) {
        CHECK(stage(f, batch).selected == 2);
        CHECK(drain(f.collector) == std::vector<std::int32_t>{10, 10});
        CHECK(f.collector.residentBytes() == fixed);
    }

    CHECK(f.collector.counts().staged == 512);
    CHECK(f.collector.counts().delivered == 512);
    CHECK_FALSE(f.collector.counts().saturated);
}

TEST_CASE("STREAM factory failures preserve desired identity and stage front pop allocate no storage") {
    auto local = schema();
    auto desired = state(local);
    const c::SubscriptionRequest request{{local.recordContext(0), u"ALPHA"}, {}};
    auto desiredWork = command();
    REQUIRE(desired.add(std::span(&request, 1), desiredWork));
    const auto membership = desired.entries()[0].membership;

    for (std::size_t successful = 0; successful < 3; ++successful) {
        auto work = turn();
        auto result = [&] {
            AllocationFault fault(successful);

            return s::StreamCollector::create(std::move(desired), limits(), work);
        }();
        REQUIRE_FALSE(result);
        CHECK(result.error() == Error::ALLOCATION_FAILURE);
        CHECK(desired.isCurrent(membership));
        CHECK(desired.revision() == 1);
    }

    auto work = turn();
    auto stream = collector(std::move(desired));
    auto live = session(local);
    const Input inputs[]{{0, u"ALPHA", 1, 10}};
    auto event = admit(live, data(inputs));
    auto staged = [&] {
        AllocationFault fault(0);

        return stream.stage(std::move(event), work);
    }();
    REQUIRE(staged);
    auto offered = [&] {
        AllocationFault fault(0);

        return stream.front(work);
    }();
    REQUIRE((offered && *offered));
    auto popped = [&] {
        AllocationFault fault(0);

        return stream.pop(work);
    }();
    CHECK(popped);
    auto invalidState = s::StreamCollector::create(std::move(desired), limits(), work);
    CHECK(invalidState.error() == Error::INVALID_FAMILY);
    auto ticker = state(local, c::SubscriptionFamily::TICKER);
    CHECK(s::StreamCollector::create(std::move(ticker), limits(), work).error() == Error::INVALID_FAMILY);
    auto impossible = limits();
    impossible.records = std::numeric_limits<std::size_t>::max();
    auto fresh = state(local);
    CHECK(s::StreamCollector::create(std::move(fresh), impossible, work).error() == Error::RESOURCE_LIMIT);
    auto untouched = command();
    CHECK(fresh.add(std::span(&request, 1), untouched));
}

namespace {
/** Exact selected semantic coordinates emitted from real native input and delivery. */
struct Trace {
    using Coordinate = std::tuple<std::string, std::string, std::string, std::string>;
    std::map<Coordinate, std::string> values;                   ///< Unique case/phase/index/key coordinates.
    std::map<std::pair<std::string, std::string>, bool> phases; ///< Required observed phases, including empty output.

    /** Append one actual observation.
     * @param name Selected production case.
     * @param phase Input/output operation boundary.
     * @param index Exact tuple ordinal, or minus one for counts.
     * @param key Semantic field.
     * @param value Canonical typed value.
     */
    void row(std::string_view name, std::string_view phase, std::int64_t index, std::string_view key,
             std::string value) {
        const Coordinate coordinate{std::string(name), std::string(phase), std::to_string(index), std::string(key)};
        REQUIRE(values.emplace(coordinate, std::move(value)).second);
        phases[{std::string(name), std::string(phase)}] = true;
    }
};

/** Encode exact UTF-16 units without normalization.
 * @param text Borrowed native value.
 * @return Production ledger's canonical unit notation.
 */
std::string units(std::u16string_view text) {
    std::ostringstream output;
    output << "u:" << std::hex << std::setfill('0');

    for (const auto unit : text) {
        output << std::setw(4) << static_cast<unsigned>(unit);
    }

    return output.str();
}

/** Emit semantic fields from an actual native admitted/delivered record.
 * @param trace Native trace sink.
 * @param name Selected case.
 * @param phase Exact production phase.
 * @param index Tuple position.
 * @param record Real immutable live record.
 */
void observe(Trace &trace, std::string_view name, std::string_view phase, std::int64_t index,
             const p::LiveRecordView &record) {
    REQUIRE(record.typedValues.size() == 3);
    const auto high = std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(record.typedValues[0].raw));
    const auto low = std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(record.typedValues[1].raw));
    const auto raw = std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(high) << 32) | low);
    trace.row(name, phase, index, "record", units(record.localId == 0 ? u"Oracle#ONE" : u"Oracle#TWO"));
    trace.row(name, phase, index, "symbol", units(record.symbol));
    trace.row(name, phase, index, "raw-time", "i:" + std::to_string(raw));
    trace.row(name, phase, index, "value", "i:" + std::to_string(record.typedValues[2].raw));
    trace.row(name, phase, index, "flags", "i:" + std::to_string(record.eventFlags));
}

/** Admit exact named production inputs and trace the actual native decoder result before staging.
 * @param trace Native trace sink.
 * @param name Production case.
 * @param phase Exact data-input batch phase.
 * @param f Native owner graph.
 * @param inputs Fixed source tuples mirrored from the production capture driver.
 */
void input(Trace &trace, std::string_view name, std::string_view phase, Fixture &f, std::span<const Input> inputs) {
    auto event = admit(f.session, data(inputs));
    std::int64_t index = 0;

    for (const auto &record : event.records()) {
        observe(trace, name, phase, index++, record);
    }

    auto work = turn();
    REQUIRE(f.collector.stage(std::move(event), work));
}

/** Drain an exact selected production phase from actual collector offers.
 * @param trace Native trace sink.
 * @param name Production case.
 * @param phase Exact retrieval phase.
 * @param stream Native staging owner.
 */
void output(Trace &trace, std::string_view name, std::string_view phase, s::StreamCollector &stream) {
    auto work = turn();
    std::int64_t index = 0;

    for (;;) {
        auto next = stream.front(work);
        REQUIRE(next);

        if (!*next) {
            break;
        }

        observe(trace, name, phase, index++, *(**next).record);
        REQUIRE(stream.pop(work));
    }

    trace.row(name, phase, -1, "record-count", "i:" + std::to_string(index));
}

/** Emit actual native desired operation inputs and apply them synchronously.
 * @param trace Native trace sink.
 * @param name Production case.
 * @param ordinal Exact subscription-input command ordinal.
 * @param agent Selected independent client ordinal, without claiming shared MUX behavior.
 * @param f Native owner graph.
 * @param symbol Exact key.
 * @param removeKey True for exact removal.
 */
void subscription(Trace &trace, std::string_view name, int ordinal, int agent, Fixture &f, std::u16string_view symbol,
                  bool removeKey = false) {
    trace.row(name, "subscription-input", ordinal, "agent", "i:" + std::to_string(agent));
    trace.row(name, "subscription-input", ordinal, "operation", removeKey ? "t:remove" : "t:add");
    trace.row(name, "subscription-input", ordinal, "record", units(u"Oracle#ONE"));
    trace.row(name, "subscription-input", ordinal, "symbol", units(symbol));

    if (removeKey) {
        remove(f, symbol);
    } else {
        add(f, symbol);
    }
}

/** Parse one canonical tab-separated observation row.
 * @param line Complete row without trailing newline.
 * @return Five exact columns; malformed input fails the test.
 */
std::array<std::string, 5> columns(const std::string &line) {
    std::array<std::string, 5> result;
    std::istringstream input(line);

    for (auto &column : result) {
        REQUIRE(static_cast<bool>(std::getline(input, column, '\t')));
    }

    std::string extra;
    REQUIRE_FALSE(std::getline(input, extra, '\t'));

    return result;
}

/** Compare every selected native input/output coordinate with real captured production results.
 * @param trace Actual native semantic tuples, including explicit output counts.
 */
void compare(const Trace &trace) {
    const auto source = std::filesystem::path(ORG_TTLDTOR_QDCXX_STREAM_SOURCE_ROOT) /
                        "tests/fixtures/subscriptions/expected/collectors/observations.tsv";
    std::ifstream input(source, std::ios::binary);
    REQUIRE(input.good());
    std::map<Trace::Coordinate, std::string> expected;
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(input, line)));
    CHECK(line == "case\tphase\tindex\tkey\tvalue");

    while (std::getline(input, line)) {
        const auto row = columns(line);

        if (!trace.phases.contains({row[0], row[1]}) || row[3] == "cipher" || row[3] == "raw-symbol" ||
            row[3] == "more") {
            continue;
        }

        REQUIRE(expected.emplace(Trace::Coordinate{row[0], row[1], row[2], row[3]}, row[4]).second);
    }

    const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_STREAM_CAPTURE_DIR);
    std::filesystem::create_directories(directory);
    std::ofstream native(directory / "observations.tsv", std::ios::binary | std::ios::trunc);
    REQUIRE(native.good());
    native << "case\tphase\tindex\tkey\tvalue\n";

    for (const auto &[coordinate, value] : trace.values) {
        const auto &[name, phase, index, key] = coordinate;
        native << name << '\t' << phase << '\t' << index << '\t' << key << '\t' << value << '\n';
    }

    REQUIRE(native.good());
    native.close();

    for (const auto &[coordinate, value] : trace.values) {
        const auto &[name, phase, index, key] = coordinate;
        const auto found = expected.find(coordinate);
        INFO(name, "/", phase, "/", index, "/", key);
        REQUIRE(found != expected.end());
        CHECK(found->second == value);
    }

    CHECK(expected.size() == trace.values.size());
    CHECK(expected == trace.values);
}
} // namespace

TEST_CASE("STREAM actual native semantic inputs and outputs match selected production collector observations") {
    Trace trace;
    {
        constexpr auto name = "stream-every-event-order";
        auto f = fixture();
        subscription(trace, name, 0, 0, f, u"ALPHA");
        subscription(trace, name, 1, 0, f, u"BETA");
        const Input batch[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 12}};
        input(trace, name, "data-input-batch-0", f, batch);
        output(trace, name, "single-batch", f.collector);
        const Input beta[]{{0, u"BETA", 2, 21}};
        const Input alpha[]{{0, u"ALPHA", 2, 13}};
        input(trace, name, "data-input-batch-1", f, beta);
        input(trace, name, "data-input-batch-2", f, alpha);
        output(trace, name, "separate-batches", f.collector);
    }
    {
        constexpr auto name = "stream-identical-filter";
        auto f = fixture();
        const Input before[]{{0, u"ALPHA", 1, 9}};
        input(trace, name, "data-input-batch-0", f, before);
        subscription(trace, name, 0, 0, f, u"ALPHA");
        output(trace, name, "after-add", f.collector);
        const Input batch[]{{0, u"ALPHA", 1, 10},
                            {0, u"ALPHA", 1, 10},
                            {0, u"BETA", 1, 20},
                            {1, u"ALPHA", 1, 30},
                            {0, u"ALPHA", 1, 10}};
        input(trace, name, "data-input-batch-1", f, batch);
        output(trace, name, "filtered-batch", f.collector);
        const Input next[]{{0, u"ALPHA", 2, 11}};
        input(trace, name, "data-input-batch-2", f, next);
        output(trace, name, "next-batch", f.collector);
    }
    {
        constexpr auto name = "stream-duplicate-two-clients";
        auto one = fixture();
        auto local = one.schema;
        Fixture two{local, session(local), collector(state(local))};
        subscription(trace, name, 0, 0, one, u"ALPHA");
        subscription(trace, name, 1, 0, one, u"ALPHA");
        subscription(trace, name, 2, 1, two, u"ALPHA");
        const Input first[]{{0, u"ALPHA", 3, 11}};
        input(trace, name, "data-input-batch-0", one, first);
        (void)stage(two, first);
        output(trace, name, "first-client-data", one.collector);
        output(trace, name, "second-client-data", two.collector);
        subscription(trace, name, 3, 0, one, u"ALPHA", true);
        subscription(trace, name, 4, 0, one, u"ALPHA", true);
        const Input remaining[]{{0, u"ALPHA", 4, 12}};
        input(trace, name, "data-input-batch-1", one, remaining);
        (void)stage(two, remaining);
        output(trace, name, "removed-client-data", one.collector);
        output(trace, name, "remaining-client-data", two.collector);
        subscription(trace, name, 5, 1, two, u"ALPHA", true);
    }

    compare(trace);
}

TEST_CASE("STREAM zero capacities empty frames wrong families and exact unusual symbols remain explicit") {
    auto bounds = limits();
    bounds.frames = 0;
    bounds.records = 0;
    bounds.stagedBytes = 0;
    auto empty = fixture(bounds);
    auto event = admit(empty.session, data({}));
    const auto ownerBytes = event.residentBytes();
    auto work = turn();
    auto ignored = empty.collector.stage(std::move(event), work);
    REQUIRE(ignored);
    CHECK(ignored->selected == 0);
    CHECK(event.residentBytes() == ownerBytes);
    CHECK(empty.collector.queuedRecords() == 0);
    CHECK(empty.collector.front(work)->has_value() == false);
    add(empty, u"ALPHA");
    const Input one[]{{0, u"ALPHA", 1, 10}};
    auto overflow = admit(empty.session, data(one));
    CHECK(empty.collector.stage(std::move(overflow), work).error() == Error::RESOURCE_LIMIT);
    CHECK(empty.collector.diagnostic().incoming == 1);
    auto f = fixture();
    add(f, u"ALPHA");
    auto ticker = admit(f.session, data(one, 10));
    CHECK(f.collector.stage(std::move(ticker), work).error() == Error::INVALID_EVENT);
    CHECK(ticker.messageType() == 10);
    const char16_t unusual[]{u'X', 0, static_cast<char16_t>(0xd800)};
    const std::u16string_view exact(unusual, 3);
    add(f, u"");
    add(f, exact);
    const Input symbols[]{{0, u"", -1, 1}, {0, exact, std::numeric_limits<std::int64_t>::min(), 2}, {0, u"X", 0, 3}};
    CHECK(stage(f, symbols).selected == 2);
    auto next = f.collector.front(work);
    REQUIRE((next && *next));
    CHECK((**next).record->symbol.empty());
    REQUIRE(f.collector.pop(work));
    next = f.collector.front(work);
    REQUIRE((next && *next));
    CHECK((**next).record->symbol == exact);
    REQUIRE(f.collector.pop(work));
    CHECK(f.collector.stagedBytes() == 0);
}

TEST_CASE("STREAM exact record capacity accepts the entire frame and isolated schema provenance never matches") {
    auto bounds = limits();
    bounds.records = 2;
    auto f = fixture(bounds);
    add(f, u"ALPHA");
    const Input pair[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}};
    CHECK(stage(f, pair).selected == 2);
    CHECK(f.collector.queuedRecords() == 2);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10, 10});
    auto independent = schema();
    auto wrong = session(independent);
    auto foreign = admit(wrong, data(pair));
    auto fresh = collector(state(f.schema));
    const c::SubscriptionRequest request{{f.schema.recordContext(0), u"ALPHA"}, {}};
    auto desiredWork = command();
    REQUIRE(fresh.add(std::span(&request, 1), desiredWork));
    auto work = turn();
    auto result = fresh.stage(std::move(foreign), work);
    REQUIRE(result);
    CHECK(result->selected == 0);
    CHECK(result->ignored == 2);
    CHECK(foreign.records().size() == 2);
    CHECK(fresh.queuedRecords() == 0);
}

TEST_CASE("STREAM full standard Quote capture retains all fields and provenance through prepared mapping") {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};
    auto defaults = q::defaultQuoteSchema(schemaBounds);
    REQUIRE(defaults);
    REQUIRE(defaults->records()[0].fields.size() == 8);
    auto local = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto mapping = q::PreparedQuote::prepare(*local, 0, {128, 256, 32, 64, 128, 128, 16384, 1000000});
    REQUIRE(mapping);
    auto stream = collector(state(*local));
    const c::SubscriptionRequest request{{local->recordContext(0), u"IBM"}, {}};
    auto desiredWork = command();
    REQUIRE(stream.add(std::span(&request, 1), desiredWork));
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

    const auto path = std::filesystem::path(ORG_TTLDTOR_QDCXX_STREAM_SOURCE_ROOT) /
                      "tests/fixtures/protocol/quote/expected/time-1.qtp";
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> characters((std::istreambuf_iterator<char>(input)), {});
    const auto capture = std::as_bytes(std::span(characters));
    std::vector<c::FieldValue> expected;
    const p::LiveRecordView *admittedAddress = nullptr;
    {
        p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
        auto writer = p::SubscriptionWriter::create(
            *local, std::span(&declaration, 1), {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
            {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
        REQUIRE(writer);
        auto work = liveTurn();
        auto live = p::LiveSession::create(std::move(*local), std::move(*writer), liveLimits(), work);
        REQUIRE(live);
        (void)admit(*live, ready());
        p::FrameReader framing(liveLimits().frame);
        std::size_t offset = 0;

        while (offset < capture.size()) {
            auto framed = framing.feed(capture.subspan(offset));
            REQUIRE(framed.frame);
            offset += framed.accepted;

            if (framed.frame->type != 2 && framed.frame->type != 15) {
                continue;
            }

            // Preserve the actual Java-produced whole envelopes; exclude only the offline tape descriptor.
            const auto begin = static_cast<std::size_t>(framed.frame->startOffset);
            const auto size = static_cast<std::size_t>(framed.frame->endOffset - framed.frame->startOffset);
            auto event = admit(*live, capture.subspan(begin, size));

            if (event.kind() != p::LiveEventKind::DATA) {
                continue;
            }

            REQUIRE(event.records().size() == 1);
            admittedAddress = event.records().data();
            REQUIRE(admittedAddress->typedValues.size() == q::QUOTE_FIELD_TYPES.size());

            for (const auto &field : admittedAddress->typedValues) {
                expected.push_back({field.type, field.raw});
            }

            auto collectorWork = turn();
            const auto selected = stream.stage(std::move(event), collectorWork);
            REQUIRE(selected);
            CHECK(selected->selected == 1);
            CHECK(selected->ignored == 0);
        }

        CHECK(live->finish() == p::LiveStatus::CLOSED);
    }

    // The session and moved local schema are gone; collector and prepared mapping own their provenance.
    auto work = turn();
    auto next = stream.front(work);
    REQUIRE((next && *next));
    const auto &generic = *(**next).record;
    CHECK(&generic == admittedAddress);
    CHECK(generic.context.matches(mapping->context()));
    REQUIRE(generic.typedValues.size() == 8);
    std::vector<c::FieldValue> actual;

    for (std::size_t field = 0; field < generic.typedValues.size(); ++field) {
        CHECK(generic.typedValues[field].type == q::QUOTE_FIELD_TYPES[field]);
        CHECK(generic.typedValues[field].type == expected[field].type);
        CHECK(generic.typedValues[field].raw == expected[field].raw);
        actual.push_back({generic.typedValues[field].type, generic.typedValues[field].raw});
    }

    auto quote = mapping->map(generic.symbol, {generic.context, actual}, 128);
    REQUIRE(quote);
    REQUIRE(stream.pop(work));
    CHECK(stream.stagedBytes() == 0);
    CHECK(stream.counts().delivered == 1);
    CHECK(quote->symbol == u"IBM");
    CHECK(quote->bidTimeMs == 1000);
    CHECK(quote->askTimeMs == 1000);
    CHECK(quote->timeMs == 1000);
    CHECK(quote->eventTime == 0);
}

TEST_CASE("STREAM prepared batches preserve eligible queue until one atomic acknowledgement") {
    auto f = fixture();
    add(f, u"ALPHA");
    add(f, u"BETA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {0, u"ALPHA", 1, 11}, {0, u"BETA", 1, 21}};
    (void)stage(f, inputs);
    remove(f, u"ALPHA");
    std::array<s::StreamDelivery, 4> storage{};
    auto work = turn();
    auto prepared = f.collector.prepareBatch(storage, work);
    REQUIRE(prepared);
    REQUIRE(prepared->size() == 2);
    CHECK((*prepared)[0].record->typedValues[2].raw == 20);
    CHECK((*prepared)[1].record->typedValues[2].raw == 21);
    CHECK((*prepared)[0].revision == 2);
    CHECK(f.collector.desired()->isCurrent((*prepared)[0].membership));
    CHECK(f.collector.queuedRecords() == 4);
    CHECK(f.collector.counts().delivered == 0);
    CHECK(f.collector.counts().withdrawn == 0);
    CHECK(f.collector.pop(work).error() == Error::NO_OFFER);
    auto acknowledged = f.collector.acknowledgeBatch(work);
    REQUIRE(acknowledged);
    CHECK(acknowledged->records == 2);
    CHECK(acknowledged->withdrawn == 2);
    CHECK(f.collector.queuedRecords() == 0);
    CHECK(f.collector.counts().delivered == 2);
    CHECK(f.collector.counts().withdrawn == 2);
    CHECK(f.collector.acknowledgeBatch(work).error() == Error::NO_OFFER);
}

TEST_CASE("STREAM single and batch offer replacement rejects stale acknowledgement interleavings") {
    auto f = fixture();
    add(f, u"ALPHA");
    add(f, u"BETA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}};
    (void)stage(f, inputs);
    std::array<s::StreamDelivery, 4> storage{};
    auto work = turn();
    REQUIRE(f.collector.prepareBatch(storage, work));
    REQUIRE(f.collector.front(work));
    CHECK(f.collector.acknowledgeBatch(work).error() == Error::NO_OFFER);
    REQUIRE(f.collector.pop(work));
    REQUIRE(f.collector.front(work));
    auto insufficient = turn();
    insufficient.visits = 0;
    CHECK(f.collector.prepareBatch(storage, insufficient).error() == Error::WORK_LIMIT);
    REQUIRE(f.collector.pop(work));
    (void)stage(f, inputs);
    REQUIRE(f.collector.prepareBatch(storage, work));
    insufficient = turn();
    insufficient.visits = 0;
    CHECK(f.collector.front(insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.collector.acknowledgeBatch(work)->records == 2);
    (void)stage(f, inputs);
    REQUIRE(f.collector.prepareBatch(storage, work));
    remove(f, u"ALPHA");
    REQUIRE(f.collector.front(work));
    CHECK(f.collector.acknowledgeBatch(work).error() == Error::NO_OFFER);
    REQUIRE(f.collector.pop(work));
    CHECK(f.collector.queuedRecords() == 0);
}

TEST_CASE("STREAM batch acknowledgement preflights shared release work without partial consumption or allocation") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
    (void)stage(f, inputs);
    std::array<s::StreamDelivery, 4> storage{};
    auto work = turn();
    auto prepared = [&] {
        AllocationFault fault(0);

        return f.collector.prepareBatch(storage, work);
    }();
    REQUIRE(prepared);
    CHECK(prepared->size() == 2);
    auto insufficient = turn();
    insufficient.bytes = 0;
    CHECK(f.collector.acknowledgeBatch(insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.collector.queuedRecords() == 2);
    CHECK(f.collector.counts().delivered == 0);
    auto acknowledged = [&] {
        AllocationFault fault(0);

        return f.collector.acknowledgeBatch(work);
    }();
    REQUIRE(acknowledged);
    CHECK(acknowledged->records == 2);
    CHECK(f.collector.queuedRecords() == 0);
}

TEST_CASE("STREAM zero-capacity and wholly withdrawn prepared prefixes have distinct exact counts") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 11}};
    (void)stage(f, inputs);
    remove(f, u"ALPHA");
    auto work = turn();
    CHECK(f.collector.prepareBatch({}, work)->empty());
    auto empty = f.collector.acknowledgeBatch(work);
    REQUIRE(empty);
    CHECK(empty->records == 0);
    CHECK(empty->withdrawn == 0);
    CHECK(f.collector.queuedRecords() == 2);
    std::array<s::StreamDelivery, 1> storage{};
    CHECK(f.collector.prepareBatch(storage, work)->empty());
    auto withdrawn = f.collector.acknowledgeBatch(work);
    REQUIRE(withdrawn);
    CHECK(withdrawn->records == 0);
    CHECK(withdrawn->withdrawn == 2);
    CHECK(f.collector.queuedRecords() == 0);
    CHECK(f.collector.counts().delivered == 0);
}

TEST_CASE("STREAM prepared provenance preserves original generations and no-op versus effective invalidation") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input first[]{{0, u"ALPHA", 1, 10}};
    auto old = admit(f.session, data(first));
    const auto oldGeneration = old.snapshot().identity().generation();
    auto work = turn();
    REQUIRE(f.collector.stage(std::move(old), work));
    (void)admit(f.session, description(41, 0));
    auto newer = admit(f.session, data(first));
    const auto newGeneration = newer.snapshot().identity().generation();
    REQUIRE(newGeneration != oldGeneration);
    REQUIRE(f.collector.stage(std::move(newer), work));
    std::array<s::StreamDelivery, 2> storage{};
    auto prepared = f.collector.prepareBatch(storage, work);
    REQUIRE(prepared);
    REQUIRE(prepared->size() == 2);
    CHECK((*prepared)[0].bindingGeneration == oldGeneration);
    CHECK((*prepared)[1].bindingGeneration == newGeneration);
    add(f, u"ALPHA");
    CHECK(f.collector.acknowledgeBatch(work)->records == 2);
    (void)stage(f, first);
    REQUIRE(f.collector.prepareBatch(storage, work));
    add(f, u"BETA");
    CHECK(f.collector.acknowledgeBatch(work).error() == Error::NO_OFFER);
    REQUIRE(f.collector.prepareBatch(storage, work));
    const Input ignored[]{{0, u"OTHER", 1, 99}};
    CHECK(stage(f, ignored).selected == 0);
    CHECK(f.collector.acknowledgeBatch(work)->records == 1);
    (void)stage(f, first);
    REQUIRE(f.collector.prepareBatch(storage, work));
    (void)stage(f, first);
    CHECK(f.collector.acknowledgeBatch(work).error() == Error::NO_OFFER);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10, 10});
}

TEST_CASE("STREAM batch offers are invalidated by move construction and assignment while single offers survive") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    (void)stage(f, input);
    std::array<s::StreamDelivery, 1> metadata{};
    auto work = turn();
    REQUIRE(f.collector.prepareBatch(metadata, work));
    auto moved = std::move(f.collector);
    auto stale = moved.acknowledgeBatch(work);
    CHECK_FALSE(stale);

    if (!stale) {
        CHECK(stale.error() == Error::NO_OFFER);
    }

    CHECK(moved.queuedRecords() == 1);

    if (moved.queuedRecords() != 0) {
        REQUIRE(moved.prepareBatch(metadata, work));
        auto destination = collector(state(f.schema));
        destination = std::move(moved);
        auto assigned = destination.acknowledgeBatch(work);
        CHECK_FALSE(assigned);

        if (!assigned) {
            CHECK(assigned.error() == Error::NO_OFFER);
        }

        CHECK(destination.queuedRecords() == 1);
        REQUIRE(destination.front(work));
        auto singleMoved = std::move(destination);
        CHECK(singleMoved.pop(work));
        CHECK(singleMoved.queuedRecords() == 0);
    }
}

TEST_CASE("STREAM explicit source reset preserves desired activations and accepts a fresh real epoch") {
    auto f = fixture();
    add(f, u"ALPHA");
    const auto membership = f.collector.desired()->entries()[0].membership;
    const auto revision = f.collector.desired()->revision();
    const auto backing = f.collector.residentBytes();
    const Input first[]{{0, u"ALPHA", 1, 10}};
    auto event = admit(f.session, data(first));
    const auto oldEpoch = event.snapshot().identity();
    auto work = turn();
    REQUIRE(f.collector.stage(std::move(event), work));
    REQUIRE(f.collector.front(work));
    const auto reset = [&] {
        AllocationFault noAllocation(0);

        return f.collector.resetSource(work);
    }();
    REQUIRE(reset);
    CHECK(reset->sequence == 1);
    CHECK(reset->discarded == 1);
    CHECK(reset->unknownLoss);
    CHECK(f.collector.residentBytes() == backing);
    CHECK(f.collector.queuedRecords() == 0);
    CHECK(f.collector.desired()->revision() == revision);
    CHECK(f.collector.desired()->isCurrent(membership));
    CHECK(f.collector.pop(work).error() == Error::NO_OFFER);

    auto next = session(f.schema);
    const Input second[]{{0, u"ALPHA", 2, 20}};
    auto fresh = admit(next, data(second));
    CHECK_FALSE(oldEpoch.sameEpoch(fresh.snapshot().identity()));
    REQUIRE(f.collector.stage(std::move(fresh), work));
    CHECK(drain(f.collector) == std::vector<std::int32_t>{20});
    auto stale = admit(f.session, data(first));
    CHECK(f.collector.stage(std::move(stale), work).error() == Error::INVALID_EPOCH);
}

TEST_CASE("STREAM source reset prepays every old frame and epoch release before any mutation") {
    for (unsigned boundary = 0; boundary < 3; ++boundary) {
        auto f = fixture();
        add(f, u"ALPHA");
        const Input first[]{{0, u"ALPHA", 1, 10}};
        auto event = admit(f.session, data(first));
        const auto release = event.releaseBytes() + event.snapshot().identity().releaseBytes();
        auto work = turn();
        REQUIRE(f.collector.stage(std::move(event), work));
        auto offered = f.collector.front(work);
        REQUIRE(offered);
        REQUIRE(*offered);
        const auto *record = (*offered)->record;
        s::StreamWork exact{1, 2, 0, release};
        auto shortWork = exact;

        if (boundary == 0)
            shortWork.operations = 0;
        else if (boundary == 1)
            --shortWork.visits;
        else
            --shortWork.bytes;

        const auto rejected = f.collector.resetSource(shortWork);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::WORK_LIMIT);
        CHECK(f.collector.queuedRecords() == 1);
        REQUIRE(record->typedValues.size() == 3);
        CHECK(record->typedValues[2].raw == 10);
        CHECK(f.collector.counts().discarded == 0);
        const auto reset = f.collector.resetSource(exact);
        REQUIRE(reset);
        CHECK(reset->sequence == 1);
        CHECK(reset->discarded == 1);
        CHECK(exact.operations == 0);
        CHECK(exact.visits == 0);
        CHECK(exact.bytes == 0);
        CHECK(f.collector.queuedRecords() == 0);
    }
}

TEST_CASE("STREAM empty resets release a last old epoch pin and never revive terminal overflow") {
    auto f = fixture();
    add(f, u"ALPHA");
    const Input first[]{{0, u"ALPHA", 1, 10}};
    (void)stage(f, first);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10});
    f.session = session(f.schema);
    auto work = turn();
    REQUIRE(f.collector.resetSource(work));
    (void)stage(f, first);
    CHECK(drain(f.collector) == std::vector<std::int32_t>{10});

    auto bounds = limits();
    bounds.frames = 1;
    bounds.records = 1;
    auto overflow = fixture(bounds);
    add(overflow, u"ALPHA");
    (void)stage(overflow, first);
    auto extra = admit(overflow.session, data(first));
    REQUIRE(overflow.collector.stage(std::move(extra), work).error() == Error::RESOURCE_LIMIT);
    CHECK(overflow.collector.resetSource(work).error() == Error::INVALID_STATE);
    CHECK(overflow.collector.queuedRecords() == 1);
    CHECK(overflow.collector.diagnostic().error == Error::RESOURCE_LIMIT);

    auto empty = fixture();
    s::StreamWork exact{1, 0, 0, 0};
    auto firstReset = empty.collector.resetSource(exact);
    REQUIRE(firstReset);
    CHECK(firstReset->sequence == 1);
    CHECK(firstReset->discarded == 0);
    auto moved = std::move(empty.collector);
    work = turn();
    CHECK(empty.collector.resetSource(work).error() == Error::INVALID_STATE);
    auto secondReset = moved.resetSource(work);
    REQUIRE(secondReset);
    CHECK(secondReset->sequence == 2);
}
