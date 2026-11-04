// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/quote.hpp"
#include "qd/protocol/live_session.hpp"

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
#include <new>
#include <sstream>
#include <type_traits>
#include <utility>

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace q = org::ttldtor::qdcxx::events;
using Error = p::LiveError;
bool failAllocations = false;         ///< Narrow production-call allocation fault switch.
std::size_t allocationsRemaining = 0; ///< Successful allocation requests before fault injection.
bool trackAllocations = false;        ///< Track only explicit production calls, excluding test-framework storage.
std::size_t trackedBytes = 0;         ///< Outstanding request bytes allocated under tracking.
std::size_t trackedPeak = 0;          ///< Highest simultaneous tracked backing.

/** Ordinary allocation request header, excluding allocator bookkeeping from reported request bytes. */
struct alignas(std::max_align_t) AllocationHeader {
    std::size_t bytes; ///< Original requested backing bytes.
    bool tracked;      ///< Whether this request belongs to the active production observation.
};

/** Enable narrowly scoped production allocation failure without affecting assertions. */
struct AllocationFault {
    /** Arm one production operation.
     * @param count Successful requests permitted first.
     */
    explicit AllocationFault(std::size_t count) {
        allocationsRemaining = count;
        failAllocations = true;
    }

    /** Restore ordinary allocation before assertions execute. */
    ~AllocationFault() {
        failAllocations = false;
    }
};

/** Track request backing across operation-returned owners without tracking framework allocations. */
struct AllocationTracking {
    /** Begin one production-only observation, including earlier tracked owners still retained. */
    AllocationTracking() {
        trackedPeak = trackedBytes;
        trackAllocations = true;
    }

    /** Stop adding new tracked requests; existing tracked owners remain observable until destruction. */
    ~AllocationTracking() {
        trackAllocations = false;
    }
};

/** Supply finite independent caller-turn credits.
 * @return Explicit generous test credits, never production defaults.
 */
p::LiveWork turn() {
    return {100000, 10000000, 10000000, 10000000};
}

/** Build a small complete ordinary/indexed schema with independent provenance.
 * @return Validated owned configuration.
 */
c::LocalSchema schema() {
    const c::LocalRecord records[] = {
        {0, u"Alpha", false, {{8}}, {{u"x", 0}}},
        {1, u"Beta", false, {{8}, {8}}, {{u"x", 0}, {u"y", 1}}},
        {2, u"Indexed", true, {{8}, {8}, {8}}, {{u"hi", 0}, {u"lo", 1}, {u"x", 2}}, c::IndexedTimePairInt32{0, 1}}};
    auto result = c::LocalSchema::create(records, {3, 6, 6, 16, 128}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Explicit sparse outgoing IDs and requested field order.
 * @return Complete owned local-to-wire declarations.
 */
std::vector<p::OutgoingRecord> layouts() {
    return {{41, 0, u"Alpha", {{u"x", 8, 0}}},
            {3, 1, u"Beta", {{u"y", 8, 1}, {u"x", 8, 0}}},
            {7, 2, u"Indexed", {{u"hi", 8, 0}, {u"lo", 8, 1}, {u"x", 8, 2}}}};
}

/** Configure finite backing capacities for a test epoch.
 * @return Explicit per-frame, arena, owner and profile limits.
 */
p::LiveLimits limits() {
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
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
    result.historySnapshot = true;

    return result;
}

/** Construct the existing exact composer before transferring it into a session.
 * @param source Validated immutable schema.
 * @param declarations Complete outgoing declarations.
 * @return Owned usable composer without outstanding outputs.
 */
p::SubscriptionWriter composer(const c::LocalSchema &source, std::span<const p::OutgoingRecord> declarations) {
    p::SubscriptionWriterWork work{10000, 1000000, 1000000, 1000000};
    auto result = p::SubscriptionWriter::create(source, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Complete test epoch with separately retained local provenance for requests. */
struct Fixture {
    std::array<c::RecordContext, 3> context; ///< Immutable identity retained before successful configuration transfer.
    p::LiveSession session;                  ///< Owning pure live driver.
};

/** Prepare a fresh epoch with all potentially failing factory work observable.
 * @param bounds Explicit finite limits.
 * @return Fresh driver and local provenance, without borrowed configuration.
 */
Fixture fixture(p::LiveLimits bounds = limits()) {
    auto local = schema();
    auto writer = composer(local, layouts());
    std::array<c::RecordContext, 3> contexts{local.recordContext(0), local.recordContext(1), local.recordContext(2)};
    auto work = turn();
    auto result = p::LiveSession::create(std::move(local), std::move(writer), bounds, work);
    REQUIRE(result);

    return {std::move(contexts), std::move(*result)};
}

/** Append one shared canonical compact-long primitive.
 * @param bytes Test-owned buffer.
 * @param value Signed raw value.
 */
void number(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto result = e::writeCompactLong(value, buffer);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append an exact UTFString using the existing canonical codec.
 * @param bytes Test-owned buffer.
 * @param value Borrowed exact nullable text.
 */
void text(std::vector<std::byte> &bytes, std::optional<std::u16string_view> value) {
    std::array<std::byte, 8192> buffer{};
    const auto result = e::writeUtfString(value, buffer, {8192, 1024});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap an exact test payload in canonical length/type framing.
 * @param type Exact message identity.
 * @param payload Borrowed complete payload.
 * @return Owned whole envelope.
 */
std::vector<std::byte> frame(std::int32_t type, std::span<const std::byte> payload) {
    std::vector<std::byte> body;
    number(body, type);
    body.insert(body.end(), payload.begin(), payload.end());
    std::vector<std::byte> result;
    number(result, static_cast<std::int64_t>(body.size()));
    result.insert(result.end(), body.begin(), body.end());

    return result;
}

/** Compose independent source-backed descriptor grammar for peer-input tests.
 * @param properties Exact top-level delta properties.
 * @param send Exact peer send declarations.
 * @param receive Exact peer receive declarations.
 * @return Whole type1 envelope, including raw DXP3 magic.
 */
std::vector<std::byte> protocol(std::span<const p::LivePropertyView> properties = {},
                                std::span<const p::LiveMessageView> send = {},
                                std::span<const p::LiveMessageView> receive = {}) {
    std::vector<std::byte> payload{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    const auto pairs = [&](std::span<const p::LivePropertyView> entries) {
        number(payload, static_cast<std::int64_t>(entries.size()));

        for (const auto &entry : entries) {
            text(payload, entry.key);
            text(payload, entry.value);
        }
    };
    const auto messages = [&](std::span<const p::LiveMessageView> entries) {
        number(payload, static_cast<std::int64_t>(entries.size()));

        for (const auto &entry : entries) {
            number(payload, entry.id);
            text(payload, entry.name);
            pairs(entry.properties);
        }
    };
    pairs(properties);
    messages(send);
    messages(receive);

    return frame(1, payload);
}

/** Supply a complete ordinary peer readiness/data descriptor.
 * @return Whole modern type1 envelope, including optional informational PART advertisement.
 */
std::vector<std::byte> ready() {
    const p::LivePropertyView properties[]{{u"type", u"qtp"}, {u"opt", u"hs,unknown"}};
    const p::LiveMessageView send[]{
        {10, u"TICKER_DATA", {}}, {15, u"STREAM_DATA", {}}, {20, u"HISTORY_DATA", {}}, {4, u"PART", {}}};
    const p::LiveMessageView receive[]{{11, u"TICKER_ADD_SUBSCRIPTION", {}},  {12, u"TICKER_REMOVE_SUBSCRIPTION", {}},
                                       {16, u"STREAM_ADD_SUBSCRIPTION", {}},  {17, u"STREAM_REMOVE_SUBSCRIPTION", {}},
                                       {21, u"HISTORY_ADD_SUBSCRIPTION", {}}, {22, u"HISTORY_REMOVE_SUBSCRIPTION", {}}};

    return protocol(properties, send, receive);
}

/** Compose one exact source description without inferring a local record ID.
 * @param id Peer sender ID.
 * @param name Exact peer record name.
 * @param fields Exact ordered name/full-type pairs.
 * @return Whole type2 envelope.
 */
std::vector<std::byte> description(std::int32_t id, std::u16string_view name,
                                   std::span<const std::pair<std::u16string_view, std::int32_t>> fields) {
    std::vector<std::byte> payload;
    number(payload, id);
    text(payload, name);
    number(payload, static_cast<std::int64_t>(fields.size()));

    for (const auto &[key, type] : fields) {
        text(payload, key);
        number(payload, type);
    }

    return frame(2, payload);
}

/** Append an FD exact-symbol tuple followed by its sender ID and raw compact fields.
 * @param payload Test-owned complete data payload.
 * @param id Directional peer sender ID.
 * @param symbol Exact non-null UTF-16 units.
 * @param values Complete raw compact-long field sequence.
 */
void record(std::vector<std::byte> &payload, std::int32_t id, std::u16string_view symbol,
            std::span<const std::int64_t> values) {
    payload.push_back(std::byte{0xfd});
    number(payload, static_cast<std::int64_t>(symbol.size()));

    for (const auto unit : symbol) {
        std::array<std::byte, 4> encoded{};
        const auto count = e::writeUtfChar(unit, encoded).written;
        payload.insert(payload.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(count));
    }

    number(payload, id);

    for (const auto value : values) {
        number(payload, value);
    }
}

/** Feed a complete expected-success test envelope.
 * @param session Pure driver.
 * @param bytes Whole input frame.
 * @return Independent immutable admitted event.
 */
p::LiveEvent admit(p::LiveSession &session, std::span<const std::byte> bytes) {
    auto work = turn();
    auto result = session.feed(bytes, work);
    INFO("error=", static_cast<int>(session.diagnostic().error), " stage=", session.diagnostic().stage);
    REQUIRE(result.status == p::LiveStatus::EVENT);
    REQUIRE(result.accepted == bytes.size());
    REQUIRE(result.event);

    return std::move(*result.event);
}

/** Read reviewed immutable Java-produced fixture bytes without normalization.
 * @param relative Source-root-relative fixture path.
 * @return Exact owned binary bytes.
 */
std::vector<std::byte> read(std::string_view relative) {
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_LIVE_SOURCE_ROOT) / relative, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Find an effective property in a borrowed exact list.
 * @param entries Exact property list.
 * @param key Non-null lookup key.
 * @return Nullable value of the first matching key, or disengaged when absent/null.
 */
std::optional<std::u16string_view> property(std::span<const p::LivePropertyView> entries, std::u16string_view key) {
    for (const auto &entry : entries) {
        if (entry.key && *entry.key == key) {
            return entry.value;
        }
    }

    return {};
}
} // namespace

/** Allocate ordinary backing with production-only fault and requested-byte observation.
 * @param count Exact ordinary allocation request, excluding this test's bookkeeping header.
 * @return Suitably aligned backing; an armed fault or allocation failure throws bad_alloc.
 */
void *operator new(std::size_t count) {
    if (failAllocations) {
        if (allocationsRemaining == 0) {
            throw std::bad_alloc{};
        }

        --allocationsRemaining;
    }

    if (count > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)) {
        throw std::bad_alloc{};
    }

    auto *header = static_cast<AllocationHeader *>(std::malloc(sizeof(AllocationHeader) + count));

    if (!header) {
        throw std::bad_alloc{};
    }

    header->bytes = count;
    header->tracked = trackAllocations;

    if (header->tracked) {
        trackedBytes += count;
        trackedPeak = std::max(trackedPeak, trackedBytes);
    }

    return header + 1;
}

/** Release tracked or untracked ordinary backing through its test-only header.
 * @param pointer Prior allocation or null.
 */
void operator delete(void *pointer) noexcept {
    if (!pointer) {
        return;
    }

    auto *header = static_cast<AllocationHeader *>(pointer) - 1;

    if (header->tracked) {
        trackedBytes -= header->bytes;
    }

    std::free(header);
}

/** Route sized deallocation through the same request observation.
 * @param pointer Prior allocation or null.
 * @param count Compiler-supplied request size, redundant with the preserved header.
 */
void operator delete(void *pointer, std::size_t count) noexcept {
    static_cast<void>(count);
    ::operator delete(pointer);
}

/** Observe byte-array backing through the same ordinary allocation path.
 * @param count Exact array allocation request.
 * @return Suitably aligned backing or a thrown bad_alloc.
 */
void *operator new[](std::size_t count) {
    return ::operator new(count);
}

/** Release observed array backing.
 * @param pointer Prior array allocation or null.
 */
void operator delete[](void *pointer) noexcept {
    ::operator delete(pointer);
}

/** Release observed sized-array backing.
 * @param pointer Prior array allocation or null.
 * @param count Compiler-supplied size, redundant with the header.
 */
void operator delete[](void *pointer, std::size_t count) noexcept {
    static_cast<void>(count);
    ::operator delete(pointer);
}

TEST_CASE("fresh epochs transfer only at successful creation and keep opaque generation identity") {
    auto local = schema();
    auto writer = composer(local, layouts());
    auto foreign = schema();
    auto work = turn();
    auto rejected = p::LiveSession::create(std::move(foreign), std::move(writer), limits(), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::INVALID_CONFIGURATION);
    CHECK(writer.matchesSchema(local));
    CHECK(foreign.records().size() == 3);
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    const std::int32_t ids[]{41};
    auto pending = writer.describe(ids, writerWork);
    REQUIRE(pending);
    auto outstanding = p::LiveSession::create(std::move(local), std::move(writer), limits(), work);
    REQUIRE_FALSE(outstanding);
    CHECK(outstanding.error() == Error::INVALID_CONFIGURATION);
    CHECK(writer.matchesSchema(local));
    auto first = fixture();
    auto second = fixture();
    CHECK(first.session.peer().identity().generation() == 0);
    CHECK_FALSE(first.session.peer().identity().matches(second.session.peer().identity()));
    CHECK_FALSE(p::LiveIdentity{}.matches(p::LiveIdentity{}));
    CHECK_FALSE(first.session.peer().identity().sameEpoch(second.session.peer().identity()));
    CHECK_FALSE(p::LiveIdentity{}.sameEpoch(p::LiveIdentity{}));
    auto old = first.session.peer();
    auto event = admit(first.session, ready());
    CHECK(event.kind() == p::LiveEventKind::PROTOCOL);
    CHECK(event.records().empty());
    CHECK(event.messageType() == 1);
    CHECK(event.snapshot().identity().generation() == 1);
    CHECK_FALSE(old.identity().matches(event.snapshot().identity()));
    CHECK(old.identity().sameEpoch(event.snapshot().identity()));
    CHECK(first.session.peer().identity().sameEpoch(event.snapshot().identity()));
    CHECK(first.session.historySnapshotEnabled());
}

TEST_CASE("captured protocol full delta unchanged and fresh reset preserve exact effective properties") {
    auto f = fixture();
    const auto full = read("tests/fixtures/subscriptions/expected/session/wire/s01-full.bin");
    const auto delta = read("tests/fixtures/subscriptions/expected/session/wire/s01-delta.bin");
    const auto unchanged = read("tests/fixtures/subscriptions/expected/session/wire/s01-unchanged.bin");
    auto before = admit(f.session, full);
    auto after = admit(f.session, delta);
    CHECK(property(before.snapshot().protocol().properties, u"changed") == u"old");
    CHECK(property(after.snapshot().protocol().properties, u"changed") == u"new");
    CHECK(property(after.snapshot().protocol().properties, u"retained") == u"first");
    CHECK(property(after.snapshot().protocol().properties, u"reset") == u"");
    CHECK(after.snapshot().protocol().send.size() == 2);
    CHECK(after.snapshot().protocol().receive.size() == 1);
    auto retained = admit(f.session, unchanged);
    CHECK(property(retained.snapshot().protocol().properties, u"retained") == u"first");
    auto fresh = fixture();
    auto reset = admit(fresh.session, delta);
    CHECK_FALSE(property(reset.snapshot().protocol().properties, u"retained"));
    CHECK(reset.snapshot().protocol().receive.empty());
    CHECK_FALSE(reset.snapshot().identity().matches(after.snapshot().identity()));
}

TEST_CASE("whole-frame data admission preserves earlier completed frames and drops a failed current prefix") {
    auto f = fixture();
    (void)admit(f.session, ready());
    const std::pair<std::u16string_view, std::int32_t> fields[]{{u"x", 8}};
    (void)admit(f.session, description(90, u"Alpha", fields));
    std::vector<std::byte> body;
    const std::int64_t first[]{17};
    record(body, 90, u"PREFIX", first);
    const auto beforeEvent = f.session.residentBytes();
    auto earlier = admit(f.session, frame(15, body));
    CHECK(earlier.residentBytes() == f.session.residentBytes() - beforeEvent);
    CHECK(earlier.releaseBytes() == f.session.residentBytes());
    CHECK(earlier.releaseBytes() >= earlier.residentBytes());
    REQUIRE(earlier.records().size() == 1);
    CHECK(earlier.records()[0].typedValues[0].raw == 17);
    record(body, 90, u"LATE", {});
    auto work = turn();
    auto failed = f.session.feed(frame(15, body), work);
    CHECK(failed.status == p::LiveStatus::FAILED);
    CHECK_FALSE(failed.event);
    CHECK(f.session.diagnostic().error == Error::MALFORMED);
    CHECK(earlier.records()[0].symbol == u"PREFIX");
    CHECK(earlier.records()[0].typedValues[0].raw == 17);
    const auto eventBytes = earlier.residentBytes();
    auto moved = std::move(earlier);
    CHECK(earlier.residentBytes() == 0);
    CHECK(earlier.releaseBytes() == 0);
    CHECK(moved.residentBytes() == eventBytes);
    CHECK(moved.releaseBytes() == f.session.residentBytes());
    CHECK(moved.records()[0].symbol == u"PREFIX");
}

TEST_CASE("all captured heartbeat masks round trip canonical compact widths and remain controls") {
    auto f = fixture();

    for (unsigned mask = 0; mask < 16; ++mask) {
        std::ostringstream name;
        name << "tests/fixtures/subscriptions/expected/session/wire/s02-heartbeat-mask-" << std::setw(2)
             << std::setfill('0') << mask << ".bin";
        const auto captured = read(name.str());
        auto event = admit(f.session, captured);
        REQUIRE(event.heartbeat());
        const auto beat = *event.heartbeat();
        CHECK(beat.timeMillis.has_value() == ((mask & 1U) != 0));
        CHECK(beat.timeMark.has_value() == ((mask & 2U) != 0));
        CHECK(beat.deltaMark.has_value() == ((mask & 4U) != 0));
        CHECK(beat.lagMark.has_value() == ((mask & 8U) != 0));
        CHECK(event.kind() == p::LiveEventKind::HEARTBEAT);
        CHECK(event.records().empty());
        auto work = turn();
        auto output = f.session.composeHeartbeat(beat, work);
        REQUIRE(output);
        CHECK(std::ranges::equal(output->bytes(), captured));
    }

    auto empty = admit(f.session, std::array{std::byte{0}});
    CHECK(empty.kind() == p::LiveEventKind::EMPTY_HEARTBEAT);
    CHECK_FALSE(empty.messageType());
    CHECK_FALSE(empty.heartbeat());
    auto noPayload = admit(f.session, std::array{std::byte{1}, std::byte{0}});
    REQUIRE(noPayload.heartbeat());
    CHECK_FALSE(noPayload.heartbeat()->hasPayload);
    auto work = turn();
    auto output = f.session.composeHeartbeat({false, {}, {}, {}, {}}, work);
    REQUIRE(output);
    CHECK(std::ranges::equal(output->bytes(), std::array{std::byte{1}, std::byte{0}}));
    const p::LiveHeartbeat extrema{true, std::numeric_limits<std::int64_t>::min(),
                                   std::numeric_limits<std::int32_t>::min(), -1,
                                   std::numeric_limits<std::int32_t>::max()};
    auto wide = f.session.composeHeartbeat(extrema, work);
    REQUIRE(wide);
    auto decoded = admit(f.session, wide->bytes());
    CHECK(decoded.heartbeat()->timeMillis == extrema.timeMillis);
    CHECK(decoded.heartbeat()->timeMark == extrema.timeMark);
    CHECK(decoded.heartbeat()->deltaMark == extrema.deltaMark);
    CHECK(decoded.heartbeat()->lagMark == extrema.lagMark);
}

TEST_CASE("fragmentation at every byte boundary preserves controls records and exact UTF16") {
    const std::pair<std::u16string_view, std::int32_t> fields[]{{u"x", 8}};
    std::vector<std::byte> data;
    const char16_t symbol[]{u'A', 0, static_cast<char16_t>(0xd800)};
    const std::int64_t value[]{-91};
    record(data, 70, std::u16string_view(symbol, 3), value);
    std::vector<std::byte> trace = ready();

    for (const auto &bytes :
         {description(70, u"Alpha", fields), frame(15, data), std::vector{std::byte{2}, std::byte{0}, std::byte{0}}}) {
        trace.insert(trace.end(), bytes.begin(), bytes.end());
    }

    for (std::size_t split = 0; split <= trace.size(); ++split) {
        auto f = fixture();
        std::size_t offset = 0;
        std::vector<p::LiveEvent> events;

        while (offset < trace.size()) {
            auto work = turn();
            const auto count = offset < split ? split - offset : trace.size() - offset;
            auto result = f.session.feed(std::span(trace).subspan(offset, count), work);
            REQUIRE(result.status != p::LiveStatus::FAILED);
            REQUIRE(result.accepted != 0);
            offset += result.accepted;

            if (result.event) {
                events.push_back(std::move(*result.event));
            }
        }

        REQUIRE(events.size() == 4);
        CHECK(events[0].kind() == p::LiveEventKind::PROTOCOL);
        CHECK(events[1].kind() == p::LiveEventKind::DESCRIPTIONS);
        REQUIRE(events[2].records().size() == 1);
        CHECK(events[2].records()[0].symbol == std::u16string_view(symbol, 3));
        CHECK(events[2].records()[0].typedValues[0].raw == -91);
        CHECK(events[3].records().empty());
        CHECK(f.session.finish() == p::LiveStatus::CLOSED);
    }
}

TEST_CASE("captured receive-add readiness and explicit protocol authentication separately gate outputs") {
    for (unsigned mask = 0; mask < 4; ++mask) {
        auto f = fixture();
        const c::SubscriptionRequest request{{f.context[0], u"IBM"}, {}};
        auto work = turn();
        auto early = f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work);
        REQUIRE_FALSE(early);
        CHECK(early.error() == Error::NOT_READY);
        const auto file = "tests/fixtures/subscriptions/expected/session/wire/s11-subscription-readiness-" +
                          std::to_string(mask) + "-peer.bin";
        (void)admit(f.session, read(file));
        auto add = f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work);
        auto remove = f.session.subscribe(p::SubscriptionMessage::TICKER_REMOVE, std::span(&request, 1), work);
        CHECK(add.has_value() == ((mask & 1U) != 0));
        CHECK(remove.has_value() == (mask == 3));
    }

    auto bounds = limits();
    bounds.credentials = p::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
    auto f = fixture(bounds);
    (void)admit(f.session, ready());
    const c::SubscriptionRequest request{{f.context[0], u"IBM"}, {}};
    const p::LivePropertyView challenge[]{{u"authentication", u"LOGIN synthetic-realm"}};
    (void)admit(f.session, protocol(challenge));
    CHECK(f.session.authentication() == p::LiveAuthentication::REQUIRED);
    auto work = turn();
    auto blocked = f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work);
    REQUIRE_FALSE(blocked);
    CHECK(blocked.error() == Error::AUTHENTICATION_REQUIRED);
    auto authorized = f.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work);
    REQUIRE(authorized);
    CHECK(f.session.authentication() == p::LiveAuthentication::AUTHORIZATION_EMITTED);
    CHECK_FALSE(f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work));
    const p::LivePropertyView complete[]{{u"authentication", u""}};
    (void)admit(f.session, protocol(complete));
    CHECK(f.session.authentication() == p::LiveAuthentication::ACKNOWLEDGED);
    CHECK(f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work));
    auto renewed = f.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work);
    REQUIRE(renewed);
    const std::pair<std::u16string_view, std::int32_t> field[]{{u"x", 8}};
    (void)admit(f.session, description(9, u"Alpha", field));
    CHECK(f.session.authentication() == p::LiveAuthentication::AUTHORIZATION_EMITTED);
    CHECK_FALSE(f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work));
    (void)admit(f.session, protocol(complete));
    CHECK(f.session.authentication() == p::LiveAuthentication::ACKNOWLEDGED);
    auto plain = fixture();
    auto forbidden = plain.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work);
    REQUIRE_FALSE(forbidden);
    CHECK(forbidden.error() == Error::CREDENTIALS_FORBIDDEN);
}

TEST_CASE("immutable replaced sender generations and returned records outlive the session") {
    std::optional<p::LiveEvent> retained;
    p::LiveSnapshot old;
    p::LiveIdentity identity;
    {
        auto f = fixture();
        (void)admit(f.session, ready());
        const std::pair<std::u16string_view, std::int32_t> x[]{{u"x", 8}};
        const std::pair<std::u16string_view, std::int32_t> y[]{{u"y", 8}, {u"x", 8}};
        (void)admit(f.session, description(4, u"Alpha", x));
        old = f.session.peer();
        identity = old.identity();
        std::vector<std::byte> data;
        const std::int64_t value[]{5};
        record(data, 4, u"", value);
        retained.emplace(admit(f.session, frame(15, data)));
        (void)admit(f.session, description(4, u"Beta", y));
        CHECK_FALSE(identity.matches(f.session.peer().identity()));
        REQUIRE(old.descriptions().size() == 1);
        CHECK(old.descriptions()[0].name == u"Alpha");
        CHECK(f.session.peer().descriptions()[0].name == u"Beta");
        std::vector<std::byte> current;
        const std::int64_t values[]{17, 23};
        record(current, 4, u"NEW", values);
        auto event = admit(f.session, frame(15, current));
        REQUIRE(event.records().size() == 1);
        CHECK(event.records()[0].localId == 1);
        CHECK(event.records()[0].typedValues[0].raw == 23);
        CHECK(event.records()[0].typedValues[1].raw == 17);
    }
    REQUIRE(retained);
    CHECK(retained->records()[0].symbol.empty());
    CHECK(retained->records()[0].typedValues[0].raw == 5);
    CHECK(old.descriptions()[0].name == u"Alpha");
    CHECK(identity.matches(old.identity()));
}

TEST_CASE("late malformed descriptions unsupported full types and actual wrappers never publish partial state") {
    const std::pair<std::u16string_view, std::int32_t> field[]{{u"x", 8}};

    for (unsigned mode = 0; mode < 3; ++mode) {
        auto f = fixture();
        (void)admit(f.session, ready());
        (void)admit(f.session, description(1, u"Alpha", field));
        auto old = f.session.peer();
        std::vector<std::byte> bad;

        if (mode == 0) {
            const auto valid = description(2, u"Beta", field);
            p::FrameReader reader(limits().frame);
            auto decoded = reader.feed(valid);
            REQUIRE(decoded.frame);
            auto payload = decoded.frame->payload;
            number(payload, 3);
            text(payload, u"Late");
            number(payload, 1);
            bad = frame(2, payload);
        } else if (mode == 1) {
            const std::pair<std::u16string_view, std::int32_t> unsupported[]{{u"x", 7}};
            bad = description(1, u"Alpha", unsupported);
        } else {
            bad = frame(4, {});
        }

        auto work = turn();
        auto result = f.session.feed(bad, work);
        CHECK(result.status == p::LiveStatus::FAILED);
        CHECK_FALSE(result.event);
        CHECK(f.session.peer().identity().matches(old.identity()));
        CHECK(f.session.peer().descriptions()[0].name == u"Alpha");
        CHECK(f.session.diagnostic().error == (mode == 0 ? Error::MALFORMED : Error::UNSUPPORTED));
    }
}

TEST_CASE("unknown described records skip exact fields without losing following known records") {
    auto f = fixture();
    (void)admit(f.session, ready());
    const std::pair<std::u16string_view, std::int32_t> field[]{{u"x", 8}};
    (void)admit(f.session, description(99, u"Unknown", field));
    (void)admit(f.session, description(7, u"Alpha", field));
    std::vector<std::byte> body;
    const std::int64_t value[]{11};
    record(body, 99, u"SKIP", value);
    record(body, 7, u"KEEP", value);
    auto event = admit(f.session, frame(15, body));
    REQUIRE(event.records().size() == 1);
    CHECK(event.records()[0].symbol == u"KEEP");
    CHECK(event.records()[0].senderId == 7);
}

TEST_CASE("requested outgoing layouts history raw bounds and partial writes retain their full owners") {
    std::optional<p::LivePendingOutput> retained;
    {
        auto f = fixture();
        (void)admit(f.session, ready());
        auto work = turn();
        const std::int32_t ids[]{3};
        auto described = f.session.describe(ids, work);
        REQUIRE(described);
        p::FrameReader framer(limits().frame);
        auto frame = framer.feed(described->bytes());
        REQUIRE(frame.frame);
        const auto parsed = p::parseDescribeRecords(frame.frame->payload, limits().descriptions);
        REQUIRE(parsed.status == p::DescriptionStatus::OK);
        REQUIRE(parsed.descriptions.size() == 1);
        CHECK(parsed.descriptions[0].senderId == 3);
        CHECK(parsed.descriptions[0].fields[0].name == u"y");
        CHECK(parsed.descriptions[0].fields[1].name == u"x");
        const char16_t symbol[]{u'A', 0, static_cast<char16_t>(0xdc00)};
        const c::SubscriptionRequest request{{f.context[2], std::u16string_view(symbol, 3)},
                                             std::numeric_limits<std::int64_t>::min()};
        auto output = f.session.subscribe(p::SubscriptionMessage::HISTORY_ADD, std::span(&request, 1), work);
        REQUIRE(output);
        const auto bytes = output->bytes();
        const auto before = f.session.residentBytes();
        REQUIRE(output->consume(1));
        CHECK(output->pendingBytes().size() + 1 == bytes.size());
        CHECK(f.session.residentBytes() == before);
        auto rejected = output->consume(bytes.size());
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error() == Error::INVALID_CONSUMPTION);
        REQUIRE(output->consume(output->pendingBytes().size()));
        CHECK(output->pendingBytes().empty());
        CHECK(output->bytes().size() == bytes.size());
        retained.emplace(std::move(*output));
    }
    REQUIRE(retained);
    CHECK_FALSE(retained->bytes().empty());
    CHECK(retained->pendingBytes().empty());
}

TEST_CASE("raw removal virtual FIELD metadata and empty data family identity remain exact") {
    auto bounds = limits();
    bounds.fieldTime = p::FieldTimePolicy::READ;
    auto f = fixture(bounds);
    (void)admit(f.session, ready());
    const std::pair<std::u16string_view, std::int32_t> fields[]{
        {u"EventTime", 56}, {u"EventSequence", 72}, {u"hi", 8}, {u"lo", 8}, {u"x", 8}};
    (void)admit(f.session, description(8, u"Indexed", fields));
    std::vector<std::byte> payload{std::byte{0xf9}};
    number(payload, 2);
    const std::int64_t values[]{-17, 0, std::numeric_limits<std::int64_t>::min()};
    record(payload, 8, u"INDEX", values);
    auto event = admit(f.session, frame(20, payload));
    REQUIRE(event.records().size() == 1);
    const auto &record = event.records()[0];
    CHECK(record.eventFlags == 2);
    CHECK(record.removalTime == std::numeric_limits<std::int64_t>::min());
    REQUIRE(record.fieldTime);
    CHECK(record.fieldTime->rawSeconds == -17);
    CHECK(record.fieldTime->rawSequence == 0);
    CHECK(record.typedValues[0].raw == std::numeric_limits<std::int32_t>::min());
    CHECK(record.typedValues[1].raw == 0);
    CHECK(record.typedValues[2].raw == 0);

    for (const auto type : {10, 15, 20}) {
        auto empty = admit(f.session, frame(type, {}));
        CHECK(empty.kind() == p::LiveEventKind::DATA);
        CHECK(empty.messageType() == type);
        CHECK(empty.records().empty());
    }
}

TEST_CASE("failed factory allocations preserve both caller owners and every admitted backing request is bounded") {
    bool completed = false;

    for (std::size_t allocation = 0; allocation < 24; ++allocation) {
        auto local = schema();
        auto writer = composer(local, layouts());
        const auto identity = local.recordContext(0);
        auto work = turn();
        auto result = [&] {
            AllocationFault fault(allocation);

            return p::LiveSession::create(std::move(local), std::move(writer), limits(), work);
        }();

        if (result) {
            completed = true;
            break;
        }

        CHECK(result.error() == Error::ALLOCATION_FAILURE);
        CHECK(local.recordContext(0).matches(identity));
        CHECK(writer.matchesSchema(local));
        CHECK(writer.retainedBytes() == 0);
        CHECK(work.operations < turn().operations);
    }

    CHECK(completed);
    CHECK(trackedBytes == 0);
    {
        auto local = schema();
        auto writer = composer(local, layouts());
        auto work = turn();
        auto result = [&] {
            AllocationTracking tracking;

            return p::LiveSession::create(std::move(local), std::move(writer), limits(), work);
        }();
        REQUIRE(result);
        CHECK(trackedBytes <= result->residentBytes());
        CHECK(trackedPeak <= result->peakBytes());
        const auto input = ready();
        auto event = [&] {
            AllocationTracking tracking;

            return result->feed(input, work);
        }();
        REQUIRE(event.event);
        CHECK(trackedBytes <= result->residentBytes());
        CHECK(trackedPeak <= result->peakBytes());
        auto output = [&] {
            AllocationTracking tracking;

            return result->composeHeartbeat({true, 7, 0, -1, {}}, work);
        }();
        REQUIRE(output);
        CHECK(trackedBytes <= result->residentBytes());
        CHECK(trackedPeak <= result->peakBytes());
    }
    CHECK(trackedBytes == 0);
}

TEST_CASE("candidate owner scratch and simultaneous peak limits fail visibly without state replacement") {
    for (const auto mode : {0, 1, 2}) {
        auto bounds = limits();

        if (mode == 0) {
            bounds.owners = 1;
        }

        if (mode == 1) {
            bounds.scratchBytes = 0;
        }

        if (mode == 2) {
            auto probe = fixture();
            auto old = probe.session.peer();
            (void)admit(probe.session, ready());
            bounds.residentBytes = probe.session.peakBytes() - 1;
        }

        auto f = fixture(bounds);
        auto old = f.session.peer();
        const auto before = f.session.residentBytes();
        auto work = turn();
        auto result = f.session.feed(ready(), work);
        CHECK(result.status == p::LiveStatus::FAILED);
        CHECK_FALSE(result.event);
        CHECK(f.session.diagnostic().error == Error::RESOURCE_LIMIT);
        CHECK(f.session.peer().identity().matches(old.identity()));
        CHECK(f.session.residentBytes() == before);
        CHECK(f.session.peakBytes() <= bounds.residentBytes);
    }
}

TEST_CASE("late work failure preserves published state and output rejection never refunds charged work") {
    auto probe = fixture();
    const auto input = ready();
    auto used = turn();
    const auto initial = used;
    auto successful = probe.session.feed(input, used);
    REQUIRE(successful.event);
    const auto visits = initial.visits - used.visits;
    REQUIRE(visits > 0);
    auto f = fixture();
    auto old = f.session.peer();
    auto scarce = turn();
    scarce.visits = visits - 1;
    const auto before = f.session.residentBytes();
    auto failed = f.session.feed(input, scarce);
    CHECK(failed.accepted == input.size());
    CHECK(failed.status == p::LiveStatus::FAILED);
    CHECK_FALSE(failed.event);
    CHECK(f.session.diagnostic().error == Error::WORK_LIMIT);
    CHECK(f.session.peer().identity().matches(old.identity()));
    CHECK(f.session.residentBytes() == before);
    CHECK(scarce.operations < turn().operations);
    CHECK(scarce.visits < visits - 1);
    auto outgoing = fixture();
    auto adequate = turn();
    auto result = outgoing.session.composeHeartbeat({true, 13, -1, {}, {}}, adequate);
    REQUIRE(result);
    const auto bytes = turn().bytes - adequate.bytes;
    result = std::unexpected(Error::INVALID_STATE);
    const auto resident = outgoing.session.residentBytes();
    auto limited = turn();
    limited.bytes = bytes - 1;
    auto rejected = outgoing.session.composeHeartbeat({true, 13, -1, {}, {}}, limited);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == Error::WORK_LIMIT);
    CHECK(outgoing.session.residentBytes() == resident);
    CHECK(limited.bytes < bytes - 1);
    auto retry = turn();
    CHECK(outgoing.session.composeHeartbeat({true, 13, -1, {}, {}}, retry));
}

TEST_CASE("sustained feed exceeds a per-frame byte quota without a finite-file lifetime cap") {
    auto f = fixture();
    (void)admit(f.session, ready());
    const auto resident = f.session.residentBytes();
    const std::array beat{std::byte{2}, std::byte{0}, std::byte{0}};
    constexpr std::size_t FRAMES = 10000;
    static_assert(FRAMES * beat.size() > 8192);

    for (std::size_t index = 0; index < FRAMES; ++index) {
        auto work = turn();
        auto result = f.session.feed(beat, work);
        REQUIRE(result.status == p::LiveStatus::EVENT);
        REQUIRE(result.event);
        CHECK(result.event->records().empty());
    }

    CHECK(f.session.residentBytes() == resident);
    CHECK(f.session.finish() == p::LiveStatus::CLOSED);
}

TEST_CASE("real default Quote Java fields traverse a live generation into prepared event mapping") {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};
    auto defaultSchema = q::defaultQuoteSchema(schemaBounds);
    REQUIRE(defaultSchema);
    auto local = c::LocalSchema::create(defaultSchema->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
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

    auto writer = composer(*local, std::span(&declaration, 1));
    auto work = turn();
    auto session = p::LiveSession::create(std::move(*local), std::move(writer), limits(), work);
    REQUIRE(session);
    (void)admit(*session, ready());
    auto clockMetadata = session->composeHeartbeat({true, 999999, {}, {}, {}}, work);
    REQUIRE(clockMetadata);
    (void)admit(*session, clockMetadata->bytes());
    const auto capture = read("tests/fixtures/protocol/quote/expected/time-1.qtp");
    p::FrameReader framer(limits().frame);
    std::size_t offset = 0;
    std::optional<q::Quote> quote;

    while (offset < capture.size()) {
        auto framed = framer.feed(std::span(capture).subspan(offset));
        REQUIRE(framed.frame);
        offset += framed.accepted;

        if (framed.frame->type != 2 && framed.frame->type != 15) {
            continue;
        }

        // Preserve exact original description/data envelopes; only the separate tape descriptor is excluded.
        const auto start = static_cast<std::size_t>(framed.frame->startOffset);
        const auto size = static_cast<std::size_t>(framed.frame->endOffset - framed.frame->startOffset);
        auto event = admit(*session, std::span(capture).subspan(start, size));

        for (const auto &generic : event.records()) {
            std::vector<c::FieldValue> values;

            for (const auto &field : generic.typedValues) {
                values.push_back({field.type, field.raw});
            }

            auto mapped = mapping->map(generic.symbol, {generic.context, values}, 128);
            REQUIRE(mapped);
            quote.emplace(std::move(*mapped));
        }
    }

    REQUIRE(quote);
    CHECK(quote->symbol == u"IBM");
    CHECK(quote->bidTimeMs == 1000);
    CHECK(quote->askTimeMs == 1000);
    CHECK(quote->timeMs == 1000);
    CHECK(quote->eventTime == 0);
    CHECK(session->finish() == p::LiveStatus::CLOSED);
}

TEST_CASE("nullable array object and UTFString values preserve exact payload ownership after session destruction") {
    std::optional<p::LiveEvent> retained;
    {
        const c::LocalRecord textRecord{0,
                                        u"Text",
                                        false,
                                        {{10}, {137}, {9}, {233}, {249}},
                                        {{u"array", 0}, {u"string", 1}, {u"bytes", 2}, {u"custom", 3}, {u"serial", 4}}};
        auto local =
            c::LocalSchema::create(std::span(&textRecord, 1), {1, 5, 5, 16, 128}, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        const p::OutgoingRecord declaration{
            14,
            0,
            u"Text",
            {{u"array", 10, 0}, {u"string", 137, 1}, {u"bytes", 9, 2}, {u"custom", 233, 3}, {u"serial", 249, 4}}};
        auto writer = composer(*local, std::span(&declaration, 1));
        auto work = turn();
        auto session = p::LiveSession::create(std::move(*local), std::move(writer), limits(), work);
        REQUIRE(session);
        (void)admit(*session, ready());
        const std::pair<std::u16string_view, std::int32_t> fields[]{
            {u"array", 10}, {u"string", 137}, {u"bytes", 9}, {u"custom", 233}, {u"serial", 249}};
        (void)admit(*session, description(32, u"Text", fields));
        std::vector<std::byte> data;
        record(data, 32, u"NULL", {});
        number(data, -1);
        text(data, u"");
        number(data, 0);
        number(data, -1);
        number(data, 3);
        data.insert(data.end(), {std::byte{0}, std::byte{0xff}, std::byte{0}});
        std::u16string exact(40, u'A');
        exact[0] = 0;
        exact[1] = static_cast<char16_t>(0xd800);
        exact[2] = static_cast<char16_t>(0xdc00);
        exact[3] = static_cast<char16_t>(0xdc00);
        record(data, 32, u"TEXT", {});
        std::array<std::byte, 512> encoded{};
        const auto array = e::writeCharArray(exact, encoded, {512, 128});
        REQUIRE(array.status == e::Status::OK);
        data.insert(data.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(array.written));
        text(data, exact);
        number(data, 2);
        data.insert(data.end(), {std::byte{0}, std::byte{0xff}});
        number(data, 0);
        number(data, -1);
        retained.emplace(admit(*session, frame(15, data)));
    }
    REQUIRE(retained);
    REQUIRE(retained->records().size() == 2);
    const auto fields = retained->records()[0].typedValues;
    REQUIRE(fields.size() == 5);
    CHECK_FALSE(fields[0].units);
    REQUIRE(fields[1].units);
    CHECK(fields[1].units->empty());
    REQUIRE(fields[2].bytes);
    CHECK(fields[2].bytes->empty());
    CHECK_FALSE(fields[3].bytes);
    REQUIRE(fields[4].bytes);
    CHECK(std::ranges::equal(*fields[4].bytes, std::array{std::byte{0}, std::byte{0xff}, std::byte{0}}));
    const auto nonempty = retained->records()[1].typedValues;
    REQUIRE(nonempty[0].units);
    REQUIRE(nonempty[1].units);
    CHECK(*nonempty[0].units == *nonempty[1].units);
    CHECK(nonempty[0].units->size() == 40);
    CHECK((*nonempty[0].units)[0] == 0);
    CHECK((*nonempty[0].units)[1] == static_cast<char16_t>(0xd800));
    CHECK((*nonempty[0].units)[3] == static_cast<char16_t>(0xdc00));
    REQUIRE(nonempty[3].bytes);
    CHECK(nonempty[3].bytes->empty());
    CHECK_FALSE(nonempty[4].bytes);
}

TEST_CASE("data and output allocation faults preserve earlier owners and do not leak reservations") {
    const std::pair<std::u16string_view, std::int32_t> fields[]{{u"x", 8}};
    std::vector<std::byte> body;
    const std::int64_t value[]{19};
    record(body, 90, u"PRESERVED", value);
    const auto input = frame(15, body);
    bool decoded = false;

    for (std::size_t fault = 0; fault < 8; ++fault) {
        auto f = fixture();
        (void)admit(f.session, ready());
        (void)admit(f.session, description(90, u"Alpha", fields));
        auto earlier = admit(f.session, input);
        auto old = f.session.peer();
        const auto before = f.session.residentBytes();
        auto work = turn();
        auto result = [&] {
            AllocationFault injection(fault);

            return f.session.feed(input, work);
        }();

        if (result.event) {
            decoded = true;
            break;
        }

        CHECK(result.status == p::LiveStatus::FAILED);
        CHECK(f.session.diagnostic().error == Error::ALLOCATION_FAILURE);
        CHECK(f.session.peer().identity().matches(old.identity()));
        CHECK(f.session.residentBytes() == before);
        CHECK(earlier.records()[0].typedValues[0].raw == 19);
        CHECK(earlier.records()[0].symbol == u"PRESERVED");
        CHECK(work.operations < turn().operations);
    }

    CHECK(decoded);
    bool composed = false;

    for (std::size_t fault = 0; fault < 8; ++fault) {
        auto f = fixture();
        auto work = turn();
        auto old = f.session.composeHeartbeat({true, 17, {}, {}, {}}, work);
        REQUIRE(old);
        const auto before = f.session.residentBytes();
        auto result = [&] {
            AllocationFault injection(fault);

            return f.session.composeHeartbeat({true, 18, {}, {}, {}}, work);
        }();

        if (result) {
            composed = true;
            break;
        }

        CHECK(result.error() == Error::ALLOCATION_FAILURE);
        CHECK(f.session.residentBytes() == before);
        CHECK_FALSE(old->bytes().empty());
        auto retry = turn();
        CHECK(f.session.composeHeartbeat({true, 18, {}, {}, {}}, retry));
    }

    CHECK(composed);
}

TEST_CASE("captured client challenge refusal and ready transitions retain reasons without implicit retry") {
    auto bounds = limits();
    bounds.credentials = p::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
    auto f = fixture(bounds);
    (void)admit(f.session,
                read("tests/fixtures/subscriptions/expected/session/wire/S12-client-login-refused-required.bin"));
    CHECK(f.session.authentication() == p::LiveAuthentication::REQUIRED);
    CHECK(property(f.session.peer().protocol().properties, u"authentication") == u"LOGIN synthetic-realm");
    auto work = turn();
    REQUIRE(f.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work));
    (void)admit(f.session,
                read("tests/fixtures/subscriptions/expected/session/wire/S12-client-login-refused-refused.bin"));
    CHECK(f.session.authentication() == p::LiveAuthentication::REQUIRED);
    CHECK(property(f.session.peer().protocol().properties, u"authentication") == u"synthetic-refused");
    CHECK_FALSE(property(f.session.peer().protocol().properties, u"authorization"));
    REQUIRE(f.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work));
    (void)admit(f.session,
                read("tests/fixtures/subscriptions/expected/session/wire/S12-client-login-success-ready.bin"));
    CHECK(f.session.authentication() == p::LiveAuthentication::ACKNOWLEDGED);
}

TEST_CASE("native controls produce an independent literal Java callback ledger") {
    auto bounds = limits();
    bounds.credentials = p::LiveCredentialPolicy::VERIFIED_TLS_BASIC;
    auto f = fixture(bounds);
    auto work = turn();
    std::vector<std::byte> bytes{std::byte{0}};
    const auto append = [&](std::expected<p::LivePendingOutput, Error> output) {
        REQUIRE(output);
        bytes.insert(bytes.end(), output->bytes().begin(), output->bytes().end());
    };
    append(f.session.composeHeartbeat({false, {}, {}, {}, {}}, work));
    const p::LivePropertyView initialProperties[]{{u"type", u"qtp"}, {u"reset", u"old"}};
    const p::LivePropertyView initialOverrides[]{{u"label", u"own"}};
    const p::LiveMessageView initialSend[]{{11, u"TICKER_ADD_SUBSCRIPTION", initialOverrides}};
    const p::LiveMessageView initialReceive[]{{10, u"TICKER_DATA", {}}};
    append(f.session.composeProtocol({initialProperties, initialSend, initialReceive}, work));
    const p::LivePropertyView deltaProperties[]{{u"reset", u""}, {u"extra", u"new"}};
    const p::LivePropertyView deltaOverrides[]{{u"label", u""}};
    const p::LiveMessageView deltaSend[]{{11, u"TICKER_ADD_SUBSCRIPTION", deltaOverrides}};
    append(f.session.composeProtocol({deltaProperties, deltaSend, {}}, work));
    append(f.session.composeProtocol({}, work));
    append(f.session.authorize(u"Basic c3ludGhldGljOnRva2Vu", work));

    for (unsigned mask = 0; mask < 16; ++mask) {
        p::LiveHeartbeat beat;

        if ((mask & 1U) != 0) {
            beat.timeMillis = 17;
        }

        if ((mask & 2U) != 0) {
            beat.timeMark = std::numeric_limits<std::int32_t>::min();
        }

        if ((mask & 4U) != 0) {
            beat.deltaMark = std::numeric_limits<std::int32_t>::max();
        }

        if ((mask & 8U) != 0) {
            beat.lagMark = -7;
        }

        append(f.session.composeHeartbeat(beat, work));
    }

    // This literal oracle expectation is independent of native parse/composition results.
    std::string expected = R"(kind|index|section|message_id|key|value
protocol|0|property|-|U00720065007300650074|U006F006C0064
protocol|0|property|-|U0074007900700065|U007100740070
protocol|0|send|11|U005400490043004B00450052005F004100440044005F0053005500420053004300520049005000540049004F004E|-
protocol|0|send-property|11|U006C006100620065006C|U006F0077006E
protocol|0|send-property|11|U00720065007300650074|U006F006C0064
protocol|0|send-property|11|U0074007900700065|U007100740070
protocol|0|receive|10|U005400490043004B00450052005F0044004100540041|-
protocol|0|receive-property|10|U00720065007300650074|U006F006C0064
protocol|0|receive-property|10|U0074007900700065|U007100740070
protocol|1|property|-|U00650078007400720061|U006E00650077
protocol|1|property|-|U00720065007300650074|U
protocol|1|property|-|U0074007900700065|U007100740070
protocol|1|send|11|U005400490043004B00450052005F004100440044005F0053005500420053004300520049005000540049004F004E|-
protocol|1|send-property|11|U00650078007400720061|U006E00650077
protocol|1|send-property|11|U006C006100620065006C|U
protocol|1|send-property|11|U00720065007300650074|U
protocol|1|send-property|11|U0074007900700065|U007100740070
protocol|1|receive|10|U005400490043004B00450052005F0044004100540041|-
protocol|1|receive-property|10|U00650078007400720061|U006E00650077
protocol|1|receive-property|10|U00720065007300650074|U
protocol|1|receive-property|10|U0074007900700065|U007100740070
protocol|2|property|-|U00650078007400720061|U006E00650077
protocol|2|property|-|U00720065007300650074|U
protocol|2|property|-|U0074007900700065|U007100740070
protocol|2|send|11|U005400490043004B00450052005F004100440044005F0053005500420053004300520049005000540049004F004E|-
protocol|2|send-property|11|U00650078007400720061|U006E00650077
protocol|2|send-property|11|U006C006100620065006C|U
protocol|2|send-property|11|U00720065007300650074|U
protocol|2|send-property|11|U0074007900700065|U007100740070
protocol|2|receive|10|U005400490043004B00450052005F0044004100540041|-
protocol|2|receive-property|10|U00650078007400720061|U006E00650077
protocol|2|receive-property|10|U00720065007300650074|U
protocol|2|receive-property|10|U0074007900700065|U007100740070
protocol|3|property|-|U0061007500740068006F00720069007A006100740069006F006E|REDACTED
protocol|3|property|-|U00650078007400720061|U006E00650077
protocol|3|property|-|U00720065007300650074|U
protocol|3|property|-|U0074007900700065|U007100740070
protocol|3|send|11|U005400490043004B00450052005F004100440044005F0053005500420053004300520049005000540049004F004E|-
protocol|3|send-property|11|U0061007500740068006F00720069007A006100740069006F006E|REDACTED
protocol|3|send-property|11|U00650078007400720061|U006E00650077
protocol|3|send-property|11|U006C006100620065006C|U
protocol|3|send-property|11|U00720065007300650074|U
protocol|3|send-property|11|U0074007900700065|U007100740070
protocol|3|receive|10|U005400490043004B00450052005F0044004100540041|-
protocol|3|receive-property|10|U0061007500740068006F00720069007A006100740069006F006E|REDACTED
protocol|3|receive-property|10|U00650078007400720061|U006E00650077
protocol|3|receive-property|10|U00720065007300650074|U
protocol|3|receive-property|10|U0074007900700065|U007100740070
heartbeat|4|mask|-|-|0
heartbeat|5|mask|-|-|1
heartbeat|5|field|-|timeMillis|17
heartbeat|6|mask|-|-|2
heartbeat|6|field|-|timeMark|-2147483648
heartbeat|7|mask|-|-|3
heartbeat|7|field|-|timeMillis|17
heartbeat|7|field|-|timeMark|-2147483648
heartbeat|8|mask|-|-|4
heartbeat|8|field|-|deltaMark|2147483647
heartbeat|9|mask|-|-|5
heartbeat|9|field|-|timeMillis|17
heartbeat|9|field|-|deltaMark|2147483647
heartbeat|10|mask|-|-|6
heartbeat|10|field|-|timeMark|-2147483648
heartbeat|10|field|-|deltaMark|2147483647
heartbeat|11|mask|-|-|7
heartbeat|11|field|-|timeMillis|17
heartbeat|11|field|-|timeMark|-2147483648
heartbeat|11|field|-|deltaMark|2147483647
heartbeat|12|mask|-|-|8
heartbeat|12|field|-|lagMark|-7
heartbeat|13|mask|-|-|9
heartbeat|13|field|-|timeMillis|17
heartbeat|13|field|-|lagMark|-7
heartbeat|14|mask|-|-|10
heartbeat|14|field|-|timeMark|-2147483648
heartbeat|14|field|-|lagMark|-7
heartbeat|15|mask|-|-|11
heartbeat|15|field|-|timeMillis|17
heartbeat|15|field|-|timeMark|-2147483648
heartbeat|15|field|-|lagMark|-7
heartbeat|16|mask|-|-|12
heartbeat|16|field|-|deltaMark|2147483647
heartbeat|16|field|-|lagMark|-7
heartbeat|17|mask|-|-|13
heartbeat|17|field|-|timeMillis|17
heartbeat|17|field|-|deltaMark|2147483647
heartbeat|17|field|-|lagMark|-7
heartbeat|18|mask|-|-|14
heartbeat|18|field|-|timeMark|-2147483648
heartbeat|18|field|-|deltaMark|2147483647
heartbeat|18|field|-|lagMark|-7
heartbeat|19|mask|-|-|15
heartbeat|19|field|-|timeMillis|17
heartbeat|19|field|-|timeMark|-2147483648
heartbeat|19|field|-|deltaMark|2147483647
heartbeat|19|field|-|lagMark|-7
)";
    std::ranges::replace(expected, '|', '\t');
    const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_LIVE_CAPTURE_DIR);
    std::filesystem::create_directories(directory);
    std::ofstream wire(directory / "native-controls.qtp", std::ios::binary | std::ios::trunc);
    wire.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    REQUIRE(wire.good());
    std::ofstream ledger(directory / "expected.tsv", std::ios::binary | std::ios::trunc);
    ledger.write(expected.data(), static_cast<std::streamsize>(expected.size()));
    REQUIRE(ledger.good());
}

TEST_CASE("opaque nullable UTF16 descriptor values and name-only identities survive exact merging") {
    auto f = fixture();
    const char16_t unusual[]{u'X', 0, static_cast<char16_t>(0xd800)};
    const p::LivePropertyView properties[]{{u"empty", u""}, {u"null", {}}, {u"raw", std::u16string_view(unusual, 3)}};
    const p::LiveMessageView receive[]{{-1, u"TICKER_ADD_SUBSCRIPTION", {}}};
    auto event = admit(f.session, protocol(properties, {}, receive));
    const auto snapshot = event.snapshot();
    REQUIRE(snapshot.protocol().properties.size() == 3);
    CHECK(snapshot.protocol().properties[0].value == u"");
    CHECK_FALSE(snapshot.protocol().properties[1].value);
    CHECK(snapshot.protocol().properties[2].value == std::u16string_view(unusual, 3));
    REQUIRE(snapshot.protocol().receive.size() == 1);
    CHECK(snapshot.protocol().receive[0].id == 11);
    const c::SubscriptionRequest request{{f.context[0], u""}, {}};
    auto work = turn();
    CHECK(f.session.subscribe(p::SubscriptionMessage::TICKER_ADD, std::span(&request, 1), work));
    auto wrong = fixture();
    const p::LiveMessageView mismatched[]{{11, u"STREAM_ADD_SUBSCRIPTION", {}}};
    (void)admit(wrong.session, protocol({}, {}, mismatched));
    CHECK_FALSE(wrong.session.subscribe(p::SubscriptionMessage::TICKER_ADD, {}, work));
    const p::LiveMessageView corrected[]{{11, u"TICKER_ADD_SUBSCRIPTION", {}}};
    (void)admit(wrong.session, protocol({}, {}, corrected));
    CHECK_FALSE(wrong.session.subscribe(p::SubscriptionMessage::TICKER_ADD, {}, work));
}

TEST_CASE("fresh sender tables do not infer old IDs and truncated or unsupported controls fail explicitly") {
    const std::pair<std::u16string_view, std::int32_t> fields[]{{u"x", 8}};
    auto first = fixture();
    (void)admit(first.session, ready());
    (void)admit(first.session, description(4, u"Alpha", fields));
    std::vector<std::byte> payload;
    const std::int64_t value[]{1};
    record(payload, 4, u"IBM", value);
    const auto data = frame(15, payload);
    auto earlier = admit(first.session, data);
    auto fresh = fixture();
    (void)admit(fresh.session, ready());
    auto work = turn();
    auto missing = fresh.session.feed(data, work);
    CHECK(missing.status == p::LiveStatus::FAILED);
    CHECK_FALSE(missing.event);
    CHECK(fresh.session.diagnostic().error == Error::MALFORMED);
    CHECK(earlier.records()[0].symbol == u"IBM");
    auto truncated = fixture();
    auto partial = truncated.session.feed(std::array{std::byte{4}, std::byte{0}}, work);
    CHECK(partial.status == p::LiveStatus::NEED_INPUT);
    CHECK(truncated.session.finish() == p::LiveStatus::FAILED);
    CHECK(truncated.session.diagnostic().error == Error::MALFORMED);
    CHECK(truncated.session.feed(data, work).accepted == 0);
    auto unsupported = fixture();
    auto unknownMask = unsupported.session.feed(std::array{std::byte{2}, std::byte{0}, std::byte{16}}, work);
    CHECK(unknownMask.status == p::LiveStatus::FAILED);
    CHECK(unsupported.session.diagnostic().error == Error::UNSUPPORTED);
    auto extension = fixture();
    auto extended = protocol();
    p::FrameReader reader(limits().frame);
    auto envelope = reader.feed(extended);
    REQUIRE(envelope.frame);
    number(envelope.frame->payload, -1);
    envelope.frame->payload.push_back(std::byte{0});
    auto refused = extension.session.feed(frame(1, envelope.frame->payload), work);
    CHECK(refused.status == p::LiveStatus::FAILED);
    CHECK(extension.session.diagnostic().error == Error::UNSUPPORTED);
    CHECK(extension.session.diagnostic().stage == "descriptor-extension");
}

TEST_CASE("creation charges configuration work before resource rejection with zero scan credits") {
    c::LocalRecord record{0, u"Alpha", false, {{8}}, {{u"x", 0}}};

    for (unsigned index = 0; index < 127; ++index) {
        const auto ascii = "alias" + std::to_string(index);
        record.lookups.push_back({std::u16string(ascii.begin(), ascii.end()), 0});
    }

    auto source =
        c::LocalSchema::create(std::span(&record, 1), {1, 1, 128, 32, 4096}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(source);
    const p::OutgoingRecord declaration{9, 0, u"Alpha", {{u"x", 8, 0}}};
    auto writer = composer(*source, std::span(&declaration, 1));
    auto bounds = limits();
    bounds.residentBytes = 0;
    p::LiveWork work{1, 0, 0, 0};
    auto result = [&] {
        AllocationFault noAllocations(0);

        return p::LiveSession::create(std::move(*source), std::move(writer), bounds, work);
    }();
    REQUIRE_FALSE(result);
    CHECK(result.error() == Error::WORK_LIMIT);
    CHECK(work.operations == 0);
    CHECK(work.visits == 0);
    CHECK(writer.matchesSchema(*source));
    CHECK(source->records()[0].lookups.size() == 128);
}

TEST_CASE("cached capacity metrics follow actual schema copies assignments and noexcept ownership moves") {
    auto original = schema();
    const auto context = original.recordContext(0);
    const auto originalBytes = original.storageBytes();
    auto copied = original.boundedCopy(limits().context.schema);
    REQUIRE(copied);
    CHECK(copied->sharesIdentityWith(original));
    CHECK(copied->storageBytes() >= sizeof(c::LocalSchema));
    auto independent = schema();
    const auto independentIdentity = independent.recordContext(0);
    const auto independentBytes = independent.storageBytes();
    bool allocationFailed = false;
    try {
        AllocationFault failure(0);
        independent = original;
    } catch (const std::bad_alloc &) {
        allocationFailed = true;
    }
    CHECK(allocationFailed);
    CHECK(independent.recordContext(0).matches(independentIdentity));
    CHECK(independent.storageBytes() == independentBytes);
    independent = original;
    CHECK(independent.recordContext(0).matches(context));
    const auto copyBytes = independent.storageBytes();
    auto moved = std::move(independent);
    CHECK(moved.storageBytes() == copyBytes);
    CHECK(independent.storageBytes() == sizeof(c::LocalSchema));
    CHECK_FALSE(independent.sharesIdentityWith(original));
    CHECK(original.storageBytes() == originalBytes);
    auto writer = composer(original, layouts());
    const auto writerBytes = writer.storageBytes();
    auto transferred = std::move(writer);
    CHECK(transferred.storageBytes() == writerBytes);
    CHECK(writer.storageBytes() == 0);
    CHECK(transferred.matchesSchema(original));
    static_assert(std::is_nothrow_move_constructible_v<c::LocalSchema>);
    static_assert(std::is_nothrow_move_assignable_v<c::LocalSchema>);
    static_assert(std::is_nothrow_move_constructible_v<p::SubscriptionWriter>);
}

TEST_CASE("fixed schema identity retains exact empty provenance without declaration copies") {
    c::SchemaIdentity retained;
    c::SchemaIdentity copied;
    const c::SchemaIdentity invalid;
    CHECK_FALSE(invalid.valid());
    CHECK_FALSE(invalid.matches(invalid));

    {
        auto empty = c::LocalSchema::create({}, limits().context.schema);
        auto other = c::LocalSchema::create({}, limits().context.schema);
        REQUIRE(empty);
        REQUIRE(other);
        CHECK_FALSE(empty->recordContext(0).valid());

        {
            AllocationFault noAllocation(0);
            retained = empty->identity();
            copied = retained;
        }

        CHECK(retained.valid());
        CHECK(retained.matches(copied));
        CHECK_FALSE(retained.matches(other->identity()));
        CHECK_FALSE(retained.matches(invalid));
        auto moved = std::move(*empty);
        CHECK_FALSE(empty->identity().valid());
        CHECK(retained.matches(moved.identity()));
    }

    CHECK(retained.valid());
    CHECK(retained.matches(copied));
    auto moved = std::move(copied);
    CHECK_FALSE(copied.valid());
    CHECK(moved.matches(retained));
}

namespace {
/** Read a deterministic official-Java EndpointId envelope, never live endpoint metadata. */
std::vector<std::byte> endpointWitness(std::string_view name) {
    return read(std::string("tests/fixtures/subscriptions/endpoint-identity-v1/wire/") + std::string(name) + ".bin");
}

/** Check failed descriptor admission against an actually retained previous peer snapshot. */
p::LiveDiagnostic endpointRefusal(std::string_view name, Error expected, p::LiveLimits bounds = limits(),
                                  std::optional<p::LiveWork> supplied = std::nullopt) {
    INFO("witness=", name);
    auto f = fixture(bounds);
    const p::LivePropertyView previous[]{{u"retained", u"stable"}};
    auto retained = admit(f.session, protocol(previous));
    const auto identity = retained.snapshot().identity();
    auto credits = supplied.value_or(turn());
    const auto result = f.session.feed(endpointWitness(name), credits);
    CHECK(result.status == p::LiveStatus::FAILED);
    CHECK_FALSE(result.event);
    CHECK(f.session.diagnostic().error == expected);
    CHECK(f.session.peer().identity().matches(identity));
    CHECK(property(f.session.peer().protocol().properties, u"retained") == u"stable");
    CHECK_FALSE(property(f.session.peer().protocol().properties, u"witness"));
    CHECK(property(retained.snapshot().protocol().properties, u"retained") == u"stable");
    CHECK_FALSE(property(retained.snapshot().protocol().properties, u"witness"));

    return f.session.diagnostic();
}
} // namespace

TEST_CASE("EndpointId accepts official absent null empty and full signed synthetic witnesses") {
    constexpr std::array names{"absent",        "null",       "present",  "empty-uid-name-min",
                               "null-name-max", "utf16-name", "long-uid", "long-name"};

    for (const auto name : names) {
        INFO("witness=", name);
        auto f = fixture();
        const p::LivePropertyView previous[]{{u"retained", u"stable"}};
        auto retained = admit(f.session, protocol(previous));
        auto event = admit(f.session, endpointWitness(name));
        CHECK(event.kind() == p::LiveEventKind::PROTOCOL);
        CHECK(event.messageType() == 1);
        CHECK(property(event.snapshot().protocol().properties, u"witness") == u"synthetic-endpoint");
        CHECK(property(event.snapshot().protocol().properties, u"retained") == u"stable");
        CHECK(event.snapshot().protocol().properties.size() == 2);
        CHECK(event.snapshot().protocol().send.empty());
        CHECK(event.snapshot().protocol().receive.empty());
        CHECK_FALSE(property(retained.snapshot().protocol().properties, u"witness"));
        CHECK(property(retained.snapshot().protocol().properties, u"retained") == u"stable");
    }
}

TEST_CASE("EndpointId incremental framing publishes only one complete descriptor") {
    for (const auto name : {"null", "present", "empty-uid-name-min", "utf16-name"}) {
        const auto bytes = endpointWitness(name);

        for (std::size_t split = 0; split <= bytes.size(); ++split) {
            INFO("witness=", name, " split=", split);
            auto f = fixture();
            auto credits = turn();
            auto first = f.session.feed(std::span(bytes).first(split), credits);
            CHECK(first.accepted == split);

            if (split < bytes.size()) {
                CHECK(first.status == p::LiveStatus::NEED_INPUT);
                CHECK_FALSE(first.event);
                CHECK(f.session.peer().protocol().properties.empty());
                credits = turn();
                auto last = f.session.feed(std::span(bytes).subspan(split), credits);
                CHECK(last.accepted == bytes.size() - split);
                REQUIRE(last.status == p::LiveStatus::EVENT);
                REQUIRE(last.event);
                CHECK(last.event->kind() == p::LiveEventKind::PROTOCOL);
            } else {
                REQUIRE(first.status == p::LiveStatus::EVENT);
                REQUIRE(first.event);
            }

            CHECK(property(f.session.peer().protocol().properties, u"witness") == u"synthetic-endpoint");
        }

        auto f = fixture();
        std::size_t published = 0;

        for (std::size_t index = 0; index < bytes.size(); ++index) {
            auto credits = turn();
            auto step = f.session.feed(std::span(bytes).subspan(index, 1), credits);
            CHECK(step.accepted == 1);

            if (step.event) {
                ++published;
                CHECK(index + 1 == bytes.size());
            }

            CHECK(step.status == (index + 1 == bytes.size() ? p::LiveStatus::EVENT : p::LiveStatus::NEED_INPUT));
        }

        CHECK(published == 1);
    }
}

TEST_CASE("EndpointId malformed fields and unknown suffixes preserve previous descriptor") {
    for (const auto name : {"invalid-uid-negative", "invalid-uid-overflow", "truncated-uid-length", "truncated-uid",
                            "truncated-empty-uid", "truncated-name", "truncated-counter"}) {
        endpointRefusal(name, Error::MALFORMED);
    }

    for (const auto name : {"extra-after-present", "extra-after-null"}) {
        endpointRefusal(name, Error::UNSUPPORTED);
    }
}

TEST_CASE("EndpointId reuses frame text and aggregate bounds without a UID text ceiling") {
    auto smallTextBytes = limits();
    smallTextBytes.protocol.text.bodyBytes = 32;
    auto accepted = fixture(smallTextBytes);
    auto event = admit(accepted.session, endpointWitness("long-uid"));
    CHECK(event.kind() == p::LiveEventKind::PROTOCOL);
    endpointRefusal("frame-excess", Error::RESOURCE_LIMIT);

    auto nameBound = limits();
    nameBound.protocol.text.codeUnits = 32;
    endpointRefusal("long-name", Error::RESOURCE_LIMIT, nameBound);
    auto aggregate = limits();
    aggregate.protocol.textUnits =
        std::u16string_view(u"witness").size() + std::u16string_view(u"synthetic-endpoint").size();
    endpointRefusal("present", Error::RESOURCE_LIMIT, aggregate);
}

TEST_CASE("EndpointId visit and whole payload work shortage never publishes a partial delta") {
    auto visits = turn();
    visits.visits = 3; // One property consumes one visit, leaving less than the three-primitive endpoint allowance.
    const auto fixedShort = endpointRefusal("present", Error::WORK_LIMIT, limits(), visits);
    CHECK(fixedShort.stage == "descriptor-endpoint");
    CHECK(fixedShort.consumed == 34);
    constexpr std::size_t UID_BYTES = 256;
    constexpr std::size_t PROPERTY_VISITS = 1;
    constexpr std::size_t ENDPOINT_PRIMITIVE_VISITS = 3;
    auto uidVisits = turn();
    uidVisits.visits = PROPERTY_VISITS + ENDPOINT_PRIMITIVE_VISITS + UID_BYTES - 1;
    const auto uidShort = endpointRefusal("long-uid", Error::WORK_LIMIT, limits(), uidVisits);
    CHECK(uidShort.stage == "descriptor-endpoint");
    CHECK(uidShort.consumed == 36); // Base34 plus the two-byte compact UID length; no raw UID advancement.
    const auto bytes = endpointWitness("long-uid");
    p::FrameReader reader(limits().frame);
    const auto envelope = reader.feed(bytes);
    REQUIRE(envelope.frame);
    auto byteWork = turn();
    byteWork.bytes = bytes.size() + envelope.frame->payload.size() - 1;
    const auto payloadShort = endpointRefusal("long-uid", Error::WORK_LIMIT, limits(), byteWork);
    CHECK(payloadShort.stage == "protocol");
    CHECK(payloadShort.consumed == 0);
}
