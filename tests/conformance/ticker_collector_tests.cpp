// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "qd/collectors/ticker.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/profile.hpp"
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
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <new>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace p = org::ttldtor::qdcxx::protocol;
namespace e = org::ttldtor::qdcxx::encoding;
namespace s = org::ttldtor::qdcxx::collectors;
namespace q = org::ttldtor::qdcxx::events;
using Error = s::TickerError;
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
s::TickerWork turn() {
    return {10000, 1000000, 100000000, 100000000};
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
s::TickerLimits limits() {
    return {4, 16, 17, 2 * 1024 * 1024, 4 * 1024 * 1024, {16, 32, 128, 2048, 64, 4096, 64}};
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
    result.events = p::DataEventPolicy::BINARY_CATALOG_V1;
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
    std::int32_t flags = 0;     ///< Exact raw source flags.
};

/** Compose selected source tuples through actual symbol/compact codecs.
 * @param inputs Complete ordered records.
 * @param type Exact data family, STREAM_DATA by default.
 * @return Whole native admission frame.
 */
std::vector<std::byte> data(std::span<const Input> inputs, std::int32_t type = 10) {
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

/** Complete independent serialized source/cache graph. */
struct Fixture {
    c::LocalSchema schema;     ///< Retained explicit source context for test driver/keys.
    p::LiveSession live;       ///< Actual whole-frame decoder, never a fabricated event.
    s::TickerCollector ticker; ///< Shared latest cache with finite local views.
};

/** Create one independent source/cache graph.
 * @param storeEverything Explicit retention setting.
 * @param bounds Required finite collector limits.
 * @param quantum Explicit source-compatible retrieval balance.
 * @return Ready external session plus empty-view collector.
 */
Fixture fixture(bool storeEverything = false, s::TickerLimits bounds = limits(), std::size_t quantum = 100) {
    auto local = schema();
    auto live = session(local);
    auto copy = local;
    auto work = turn();
    auto ticker = s::TickerCollector::create(std::move(copy), bounds, {storeEverything, quantum}, work);
    REQUIRE(ticker);

    return {std::move(local), std::move(live), std::move(*ticker)};
}

/** Open one reserved pure view.
 * @param f Complete serialized fixture.
 * @return Fresh origin-bound view token.
 */
s::TickerViewToken open(Fixture &f) {
    auto work = turn();
    auto result = f.ticker.openView(work);
    REQUIRE(result);

    return std::move(*result);
}

/** Apply one exact current desired interest.
 * @param f Complete fixture.
 * @param view Exact view token.
 * @param symbol Borrowed exact non-null units.
 * @param record Local identity.
 */
void add(Fixture &f, const s::TickerViewToken &view, std::u16string_view symbol, std::int32_t record = 0) {
    const c::SubscriptionRequest request{{f.schema.recordContext(record), symbol}, {}};
    auto core = command();
    auto work = turn();
    REQUIRE(f.ticker.add(view, std::span(&request, 1), core, work));
}

/** Remove one membership without affecting another view.
 * @param f Complete fixture.
 * @param view Exact view token.
 * @param symbol Exact key text.
 * @param record Local record identity.
 */
void remove(Fixture &f, const s::TickerViewToken &view, std::u16string_view symbol, std::int32_t record = 0) {
    const c::SubscriptionKey key{f.schema.recordContext(record), symbol};
    auto core = command();
    auto work = turn();
    REQUIRE(f.ticker.remove(view, std::span(&key, 1), core, work));
}

/** Decode and atomically stage one complete controlled frame.
 * @param f Source/cache graph.
 * @param inputs Exact source tuples.
 * @return Actual cache result.
 */
s::TickerStage stage(Fixture &f, std::span<const Input> inputs) {
    auto event = admit(f.live, data(inputs));
    auto work = turn();
    auto result = f.ticker.stage(std::move(event), work);
    REQUIRE(result);

    return *result;
}

/** One owning scalar observation; does not extend borrowed record lifetime. */
struct Observed {
    std::int32_t record;        ///< Local record id.
    std::u16string symbol;      ///< Exact owned observed symbol.
    std::int64_t time;          ///< Signed raw source time.
    std::int32_t value;         ///< Ordinary compact data.
    std::int32_t flags;         ///< Normalized delivered flags.
    s::TickerDeliveryKind kind; ///< First activation value or subsequent live value.
};

/** Copy exact timed synthetic observations from an actual borrowed source record.
 * @param delivery Current successful offer.
 * @return Owning scalar projection for assertions/trace output.
 */
Observed observe(const s::TickerDelivery &delivery) {
    const auto &record = *delivery.value.sourceRecord;
    REQUIRE(record.typedValues.size() == 3);
    const auto hi = static_cast<std::uint32_t>(record.typedValues[0].raw);
    const auto lo = static_cast<std::uint32_t>(record.typedValues[1].raw);

    return {record.localId,
            std::u16string(record.symbol),
            std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(hi) << 32) | lo),
            static_cast<std::int32_t>(record.typedValues[2].raw),
            delivery.deliveryFlags,
            delivery.kind};
}

/** Drain at most a finite number of actual single offers.
 * @param f Complete fixture.
 * @param view Selected current view.
 * @param maximum Explicit retrieval capacity; zero does no work.
 * @return Actual owning observations in collector delivery order.
 */
std::vector<Observed> drain(Fixture &f, const s::TickerViewToken &view, std::size_t maximum = 256) {
    std::vector<Observed> output;
    auto work = turn();

    while (output.size() < maximum) {
        auto next = f.ticker.front(view, work);
        REQUIRE(next);

        if (!*next) {
            break;
        }

        output.push_back(observe(**next));
        REQUIRE(f.ticker.pop(view, work));
    }

    return output;
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

TEST_CASE("TICKER selected retention latest equality and late view share the actual cache") {
    for (const auto store : {false, true}) {
        auto f = fixture(store);
        auto one = open(f);
        const Input before[]{{0, u"ALPHA", 1, 10}};
        stage(f, before);
        add(f, one, u"ALPHA");
        CHECK(drain(f, one).size() == (store ? 1 : 0));
        const Input updates[]{{0, u"ALPHA", 2, 20}, {0, u"ALPHA", 3, 30}};
        auto changed = stage(f, updates);
        CHECK(changed.changed == 2);
        auto first = drain(f, one);
        REQUIRE(first.size() == 1);
        CHECK(first[0].value == 30);
        auto late = open(f);
        add(f, late, u"ALPHA");
        auto initial = drain(f, late);
        REQUIRE(initial.size() == 1);
        CHECK(initial[0].value == 30);
        const Input equal[]{{0, u"ALPHA", 3, 30}, {0, u"ALPHA", 3, 30}};
        CHECK(stage(f, equal).equal == 2);
        CHECK(drain(f, one).empty());
        CHECK(drain(f, late).empty());
        remove(f, one, u"ALPHA");
        remove(f, late, u"ALPHA");
        add(f, one, u"ALPHA");
        CHECK(drain(f, one).size() == (store ? 1 : 0));
    }
}

TEST_CASE("TICKER partial initial delivery conflates pending keys and orders live after initial") {
    auto f = fixture(true);
    auto view = open(f);
    const Input seed[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {1, u"ALPHA", 1, 30}};
    stage(f, seed);
    add(f, view, u"ALPHA");
    add(f, view, u"BETA");
    add(f, view, u"ALPHA", 1);
    auto first = drain(f, view, 1);
    REQUIRE(first.size() == 1);
    CHECK(first[0].symbol == u"ALPHA");
    const Input update[]{{0, u"BETA", 2, 21}, {0, u"ALPHA", 3, 12}};
    stage(f, update);
    auto rest = drain(f, view);
    REQUIRE(rest.size() == 3);
    CHECK(rest[0].symbol == u"BETA");
    CHECK(rest[0].value == 21);
    CHECK(rest[1].record == 1);
    CHECK(rest[2].value == 12);
    CHECK(rest[0].kind == s::TickerDeliveryKind::INITIAL);
    CHECK(rest[2].kind == s::TickerDeliveryKind::LIVE);
}

TEST_CASE("TICKER stalled view reserves metadata but never retains older source frames") {
    auto bounds = limits();
    bounds.cacheKeys = 1;
    bounds.cachedFrames = 2;
    auto f = fixture(false, bounds);
    auto fast = open(f);
    auto stalled = open(f);
    add(f, fast, u"ALPHA");
    add(f, stalled, u"ALPHA");

    for (std::int32_t value = 0; value < 256; ++value) {
        const Input input[]{{0, u"ALPHA", value, value}};
        stage(f, input);
        auto result = drain(f, fast);
        REQUIRE(result.size() == 1);
        CHECK(result[0].value == value);
        CHECK(f.ticker.pendingKeys(stalled) == 1);
        CHECK(f.ticker.cachedFrames() == 1);
    }

    auto last = drain(f, stalled);
    REQUIRE(last.size() == 1);
    CHECK(last[0].value == 255);
}

TEST_CASE("TICKER reset is a reserved exact global owner barrier and foreign epoch cannot leak") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(f, input);
    auto work = turn();
    auto old = f.ticker.front(view, work);
    REQUIRE((old && *old));
    auto oldEpoch = (**old).value.epoch;
    auto other = session(f.schema);
    auto foreign = admit(other, data(input));
    CHECK(f.ticker.stage(std::move(foreign), work).error() == Error::INVALID_EPOCH);
    CHECK(foreign.residentBytes() > 0);
    auto notice = f.ticker.reset(work);
    REQUIRE(notice);
    CHECK(notice->discarded == 1);
    CHECK(notice->evicted == 1);
    CHECK(f.ticker.cachedKeys() == 0);
    CHECK(f.ticker.front(view, work).error() == Error::RESET_PENDING);
    CHECK(f.ticker.acknowledgeReset(notice->sequence + 1, work).error() == Error::RESET_PENDING);
    REQUIRE(f.ticker.acknowledgeReset(notice->sequence, work));
    REQUIRE(f.ticker.stage(std::move(foreign), work));
    auto fresh = f.ticker.front(view, work);
    REQUIRE((fresh && *fresh));
    CHECK_FALSE(oldEpoch.matches((**fresh).value.epoch));
}

TEST_CASE("TICKER shared overflow retains unconsumed frame but invalidates current offers") {
    auto bounds = limits();
    bounds.cacheKeys = 1;
    auto f = fixture(true, bounds);
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input one[]{{0, u"ALPHA", 1, 10}};
    stage(f, one);
    auto work = turn();
    REQUIRE(f.ticker.front(view, work));
    const Input two[]{{0, u"BETA", 2, 20}};
    auto incoming = admit(f.live, data(two));
    const auto bytes = incoming.residentBytes();
    CHECK(f.ticker.stage(std::move(incoming), work).error() == Error::RESOURCE_LIMIT);
    CHECK(incoming.residentBytes() == bytes);
    CHECK(f.ticker.state() == s::TickerState::STALE);
    CHECK(f.ticker.pop(view, work).error() == Error::NO_OFFER);
    auto cached = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE((cached && *cached));
    CHECK_FALSE((**cached).current);
}

TEST_CASE("TICKER raw REMOVE materializes defaults without erasure and flags do not dirty equal data") {
    for (const auto store : {false, true}) {
        auto f = fixture(store);
        auto view = open(f);
        add(f, view, u"ALPHA");
        add(f, view, u"BETA");
        const Input first[]{{0, u"ALPHA", 1, 10}};
        stage(f, first);
        REQUIRE(drain(f, view).size() == 1);
        const Input flagsOnly[]{{0, u"ALPHA", 1, 10, 1}};
        CHECK(stage(f, flagsOnly).equal == 1);
        CHECK(drain(f, view).empty());
        const Input removal[]{{0, u"ALPHA", 1, 0, 2}, {0, u"BETA", 0, 0, 2}};
        auto event = admit(f.live, data(removal));
        REQUIRE(event.records().size() == 2);
        CHECK(event.records()[0].eventFlags == 2);
        CHECK(event.records()[0].removalTime == 1);
        REQUIRE(event.records()[0].typedValues.size() == 3);
        CHECK(event.records()[0].typedValues[2].raw == 0);
        auto work = turn();
        REQUIRE(f.ticker.stage(std::move(event), work));
        auto output = drain(f, view);
        REQUIRE(output.size() == 2);
        CHECK(output[0].flags == 0);
        CHECK(output[1].flags == 0);
        auto cached = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
        REQUIRE((cached && *cached));
        CHECK((**cached).sourceRecord->eventFlags == 2);
        CHECK((**cached).current);
        CHECK(stage(f, removal).equal == 2);
        CHECK(drain(f, view).empty());
    }
}

TEST_CASE("TICKER pending remove readd is a fresh activation while a keeper retains latest") {
    auto f = fixture();
    auto view = open(f);
    auto keeper = open(f);
    add(f, view, u"ALPHA");
    add(f, view, u"BETA");
    add(f, view, u"ALPHA", 1);
    add(f, keeper, u"BETA");
    const Input seed[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {1, u"ALPHA", 1, 30}};
    stage(f, seed);
    REQUIRE(drain(f, view, 1).size() == 1);
    auto membership = f.ticker.desired(view)->entries()[1].membership;
    remove(f, view, u"BETA");
    CHECK_FALSE(f.ticker.desired(view)->isCurrent(membership));
    add(f, view, u"BETA");
    const Input update[]{{0, u"BETA", 2, 21}};
    stage(f, update);
    auto rest = drain(f, view);
    REQUIRE(rest.size() == 2);
    CHECK(rest[0].record == 1);
    CHECK(rest[1].symbol == u"BETA");
    CHECK(rest[1].kind == s::TickerDeliveryKind::INITIAL);
    auto held = drain(f, keeper);
    REQUIRE(held.size() == 1);
    CHECK(held[0].value == 21);
}

TEST_CASE("TICKER candidate reversal is dirty even when its final data equals the old value") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input seed[]{{0, u"ALPHA", 1, 10}};
    stage(f, seed);
    REQUIRE(drain(f, view).size() == 1);
    const Input reversal[]{{0, u"ALPHA", 1, 11}, {0, u"ALPHA", 1, 10}};
    CHECK(stage(f, reversal).changed == 2);
    auto output = drain(f, view);
    REQUIRE(output.size() == 1);
    CHECK(output[0].value == 10);
}

TEST_CASE("TICKER foreign stale reused and transferred tokens cannot select another view") {
    auto bounds = limits();
    bounds.viewSlots = 1;
    auto one = fixture(false, bounds);
    auto two = fixture(false, bounds);
    auto token = open(one);
    auto foreign = open(two);
    auto work = turn();
    CHECK(one.ticker.front(foreign, work).error() == Error::INVALID_VIEW);
    CHECK(one.ticker.openView(work).error() == Error::RESOURCE_LIMIT);
    REQUIRE(one.ticker.closeView(token, work));
    auto replacement = open(one);
    CHECK_FALSE(token.matches(replacement));
    CHECK(one.ticker.desired(token) == nullptr);
    CHECK(one.ticker.pop(token, work).error() == Error::INVALID_VIEW);
    add(one, replacement, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(one, input);
    REQUIRE(one.ticker.front(replacement, work));
    auto moved = std::move(one.ticker);
    CHECK(one.ticker.schema() == nullptr);
    CHECK(moved.desired(replacement));
    CHECK(moved.pop(replacement, work).error() == Error::NO_OFFER);
    REQUIRE(moved.front(replacement, work));
    REQUIRE(moved.pop(replacement, work));
}

TEST_CASE("TICKER work rejection clears borrowed candidate scratch and preserves the prior offer") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(f, input);
    auto work = turn();
    REQUIRE(f.ticker.front(view, work));
    const auto before = f.ticker.cacheBytes();
    {
        const Input update[]{{0, u"ALPHA", 2, 20}, {0, u"ALPHA", 3, 30}};
        auto event = admit(f.live, data(update));
        auto limited = turn();
        limited.textUnits = 0;
        CHECK(f.ticker.stage(std::move(event), limited).error() == Error::WORK_LIMIT);
        CHECK(event.residentBytes() > 0);
    }
    CHECK(f.ticker.cacheBytes() == before);
    REQUIRE(f.ticker.pop(view, work));
    const Input next[]{{0, u"ALPHA", 4, 40}};
    stage(f, next);
    auto output = drain(f, view);
    REQUIRE(output.size() == 1);
    CHECK(output[0].value == 40);
}

TEST_CASE("TICKER no-op rejected and aliased desired commands preserve exact publication barriers") {
    auto f = fixture(true);
    auto view = open(f);
    const Input input[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}};
    stage(f, input);
    add(f, view, u"ALPHA");
    auto work = turn();
    auto core = command();
    REQUIRE(f.ticker.front(view, work));
    const c::SubscriptionRequest same{{f.schema.recordContext(0), u"ALPHA"}, {}};
    auto unchanged = f.ticker.add(view, std::span(&same, 1), core, work);
    REQUIRE(unchanged);
    CHECK_FALSE(unchanged->changed);
    REQUIRE(f.ticker.pop(view, work));
    const c::SubscriptionRequest invalid{{f.schema.recordContext(0), u"*bad"}, {}};
    CHECK(f.ticker.add(view, std::span(&invalid, 1), core, work).error().subscription ==
          c::SubscriptionError::INVALID_SELECTOR);
    add(f, view, u"BETA");
    REQUIRE(f.ticker.front(view, work));
    const auto entries = f.ticker.desired(view)->entries();
    const c::SubscriptionRequest aliased[]{{entries[1].key, {}}};
    auto changed = f.ticker.replace(view, aliased, core, work);
    REQUIRE(changed);
    CHECK(changed->changed);
    CHECK(f.ticker.pop(view, work).error() == Error::NO_OFFER);
    auto remaining = drain(f, view);
    REQUIRE(remaining.size() == 1);
    CHECK(remaining[0].symbol == u"BETA");
}

TEST_CASE("TICKER peak frame and byte failures are explicit shared failures rather than final-size eviction") {
    auto bounds = limits();
    bounds.cacheKeys = 1;
    bounds.cachedFrames = 1;
    auto f = fixture(false, bounds);
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input old[]{{0, u"ALPHA", 1, 10}};
    stage(f, old);
    const Input update[]{{0, u"ALPHA", 2, 20}};
    auto event = admit(f.live, data(update));
    auto work = turn();
    CHECK(f.ticker.stage(std::move(event), work).error() == Error::RESOURCE_LIMIT);
    CHECK(f.ticker.cachedFrames() == 1);
    CHECK(f.ticker.diagnostic().incoming == 1);
    CHECK(f.ticker.diagnostic().cached == 1);
    auto reset = f.ticker.reset(work);
    REQUIRE(reset);
    CHECK(reset->previousCause == Error::RESOURCE_LIMIT);
    auto newer = f.ticker.reset(work);
    REQUIRE(newer);
    CHECK(newer->skipped == 1);
    CHECK(f.ticker.acknowledgeReset(reset->sequence, work).error() == Error::RESET_PENDING);
    REQUIRE(f.ticker.acknowledgeReset(newer->sequence, work));
    REQUIRE(f.ticker.stage(std::move(event), work));
    REQUIRE(drain(f, view).size() == 1);
}

TEST_CASE("TICKER fixed factory failure preserves schema and independent views reject core resource pressure") {
    auto local = schema();
    const auto context = local.recordContext(0);
    auto bounds = limits();
    bool succeeded = false;

    for (std::size_t allowance = 0; allowance < 24 && !succeeded; ++allowance) {
        auto work = turn();
        auto result = [&] {
            AllocationFault fault(allowance);

            return s::TickerCollector::create(std::move(local), bounds, {false, 100}, work);
        }();

        if (result) {
            succeeded = true;
            CHECK(result->schema()->recordContext(0).matches(context));
        } else {
            CHECK(result.error() == Error::ALLOCATION_FAILURE);
            CHECK(local.recordContext(0).matches(context));
        }
    }

    CHECK(succeeded);
    bounds.viewSubscriptions.entries = 1;
    auto f = fixture(false, bounds);
    auto one = open(f);
    auto two = open(f);
    add(f, one, u"ALPHA");
    add(f, two, u"BETA");
    auto core = command();
    auto work = turn();
    const c::SubscriptionRequest excessive{{f.schema.recordContext(0), u"BETA"}, {}};
    auto failed = f.ticker.add(one, std::span(&excessive, 1), core, work);
    REQUIRE_FALSE(failed);
    CHECK(failed.error().subscription == c::SubscriptionError::RESOURCE_LIMIT);
    CHECK(f.ticker.desired(two)->entries().size() == 1);
    const Input data[]{{0, u"BETA", 1, 20}};
    stage(f, data);
    REQUIRE(drain(f, two).size() == 1);
}

namespace {
/** Construct a generic actual session without inventing field types or reducing schema.
 * @param local Complete immutable local schema.
 * @return Ready session; real fixture descriptions are admitted separately.
 */
p::LiveSession genericSession(const c::LocalSchema &local) {
    std::vector<p::OutgoingRecord> declarations;

    for (const auto &record : local.records()) {
        p::OutgoingRecord declaration{17 + record.id, record.id, record.name, {}};

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });
            REQUIRE(lookup != record.lookups.end());
            declaration.fields.push_back(
                {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        declarations.push_back(std::move(declaration));
    }

    auto copy = local;
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(local, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(), work);
    REQUIRE(result);
    (void)admit(*result, ready());

    return std::move(*result);
}

/** Copy exact generic data for an owning prepared mapping, without conversions.
 * @param record Actual borrowed normalized source record.
 * @return Deep owned typed/null/text/opaque field values.
 */
std::vector<c::FieldValue> copyValues(const p::LiveRecordView &record) {
    std::vector<c::FieldValue> values;

    for (const auto &field : record.typedValues) {
        c::FieldValue value{field.type, field.raw};

        if (field.units) {
            value.units = std::u16string(*field.units);
        }

        if (field.bytes) {
            value.bytes = std::vector<std::byte>(field.bytes->begin(), field.bytes->end());
        }

        values.push_back(std::move(value));
    }

    return values;
}
} // namespace

TEST_CASE(
    "TICKER complete standard Quote and Profile fixture bodies retain all fields through latest and prepared mapping") {
    for (const bool profile : {false, true}) {
        const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};
        auto defaults = profile ? q::defaultProfileSchema(schemaBounds) : q::defaultQuoteSchema(schemaBounds);
        REQUIRE(defaults);
        REQUIRE(defaults->records()[0].fields.size() == (profile ? 16 : 8));
        auto local = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        auto copy = *local;
        auto work = turn();
        auto ticker = s::TickerCollector::create(std::move(copy), limits(), {true, 100}, work);
        REQUIRE(ticker);
        std::vector<c::FieldValue> expected;
        {
            auto live = genericSession(*local);
            const auto relative = profile ? "tests/fixtures/encoding/field-types/expected/profile-stream.qtp"
                                          : "tests/fixtures/protocol/quote/expected/time-1.qtp";
            std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_TICKER_SOURCE_ROOT) / relative,
                                std::ios::binary);
            REQUIRE(input.good());
            const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
            const auto capture = std::as_bytes(std::span(bytes));
            p::FrameReader reader(liveLimits().frame);
            std::size_t offset = 0;

            while (offset < capture.size()) {
                auto framed = reader.feed(capture.subspan(offset));
                REQUIRE(framed.frame);
                offset += framed.accepted;

                if (framed.frame->type == 2) {
                    (void)admit(live, frame(2, framed.frame->payload));
                } else if (framed.frame->type == 15) {
                    // Native controlled TICKER envelope preserves the exact Java-produced STREAM field body.
                    auto event = admit(live, frame(10, framed.frame->payload));
                    REQUIRE(event.records().size() == 1);
                    REQUIRE(event.records()[0].typedValues.size() == (profile ? 16 : 8));
                    expected = copyValues(event.records()[0]);
                    REQUIRE(ticker->stage(std::move(event), work));
                }
            }

            CHECK(live.finish() == p::LiveStatus::CLOSED);
        }
        auto view = ticker->openView(work);
        REQUIRE(view);
        const c::SubscriptionRequest request{{local->recordContext(0), u"IBM"}, {}};
        auto core = command();
        REQUIRE(ticker->add(*view, std::span(&request, 1), core, work));
        auto next = ticker->front(*view, work);
        REQUIRE((next && *next));
        auto values = copyValues(*(**next).value.sourceRecord);
        REQUIRE(values.size() == expected.size());

        for (std::size_t i = 0; i < values.size(); ++i) {
            CHECK(values[i].type == expected[i].type);
            CHECK(values[i].raw == expected[i].raw);
            CHECK(values[i].units == expected[i].units);
            CHECK(values[i].bytes == expected[i].bytes);
        }

        const q::MappingLimits mappingBounds{128, 256, 32, 64, 64, 128, 16384, 1000000};

        if (profile) {
            auto mapping = q::PreparedProfile::prepare(*local, 0, mappingBounds);
            REQUIRE(mapping);
            auto mapped = mapping->map(u"IBM", {(**next).value.sourceRecord->context, values}, {128, 1024, 8192}, 128);
            REQUIRE(mapped);
            CHECK(mapped->flags == 224);
            REQUIRE(mapped->description);
            CHECK(mapped->description->size() == 6);
            CHECK(mapped->statusReason == expected[15].units);
        } else {
            auto mapping = q::PreparedQuote::prepare(*local, 0, mappingBounds);
            REQUIRE(mapping);
            auto mapped = mapping->map(u"IBM", {(**next).value.sourceRecord->context, values}, 128);
            REQUIRE(mapped);
            CHECK(mapped->timeMs == 1000);
        }

        REQUIRE(ticker->pop(*view, work));
        CHECK(ticker->cachedKeys() == 1);
        CHECK(ticker->cachedFrames() == 1);
    }
}

namespace {
/** Encode exact UTF-16 for the shared observation grammar.
 * @param value Exact non-null units.
 * @return Canonical lowercase four-hex-digit units with u: prefix.
 */
std::string units(std::u16string_view value) {
    std::ostringstream out;
    out << "u:" << std::hex << std::setfill('0');

    for (const auto unit : value) {
        out << std::setw(4) << static_cast<unsigned>(unit);
    }

    return out.str();
}

/** Actual native trace driver; no expected output is read or synthesized here. */
class Trace {
    std::ostream &out_;            ///< Caller-owned result stream.
    std::string name_;             ///< Exact named production scenario.
    std::size_t subscription_ = 0; ///< Actual desired command ordinal.
    std::size_t batch_ = 0;        ///< Matching selected source batch ordinal.

    public:
    Fixture f;                             ///< Actual live decoder/shared collector graph.
    std::vector<s::TickerViewToken> views; ///< Real opened pure view selections.

    /** Construct one actual selected profile.
     * @param out Owned output file stream.
     * @param name Exact production scenario.
     * @param store Explicit retention flag passed to collector creation.
     */
    Trace(std::ostream &out, std::string name, bool store) : out_(out), name_(std::move(name)), f(fixture(store)) {
    }

    /** Use an explicitly larger bounded actual admission profile.
     * @param out Caller-owned output stream.
     * @param name Exact selected production scenario.
     * @param graph Complete independently owned source/cache graph.
     */
    Trace(std::ostream &out, std::string name, Fixture &&graph)
        : out_(out), name_(std::move(name)), f(std::move(graph)) {
    }

    /** Emit one actual semantic observation coordinate.
     * @param phase Exact source phase.
     * @param index Record/control index.
     * @param key Semantic property.
     * @param value Actual typed value string.
     */
    void row(std::string_view phase, std::int64_t index, std::string_view key, std::string value) {
        out_ << name_ << '\t' << phase << '\t' << index << '\t' << key << '\t' << value << '\n';
    }

    /** Emit an actual integer.
     * @param phase Exact phase.
     * @param index Observation ordinal.
     * @param key Property.
     * @param value Actual integer.
     */
    void integer(std::string_view phase, std::int64_t index, std::string_view key, std::int64_t value) {
        row(phase, index, key, "i:" + std::to_string(value));
    }

    /** Emit an actual boolean.
     * @param phase Exact phase.
     * @param key Property at index zero.
     * @param value Actual boolean.
     */
    void boolean(std::string_view phase, std::string_view key, bool value) {
        row(phase, 0, key, value ? "b:1" : "b:0");
    }

    /** Open an actual view and record its scenario-local ordinal.
     * @return Index of the new real view token.
     */
    std::size_t agent() {
        views.push_back(open(f));

        return views.size() - 1;
    }

    /** Apply and record one exact actual desired command.
     * @param agent View index.
     * @param removing True for remove.
     * @param record Local record identity.
     * @param symbol Exact selected units.
     */
    void sub(std::size_t agent, bool removing, std::int32_t record, std::u16string_view symbol) {
        if (removing) {
            remove(f, views[agent], symbol, record);
        } else {
            add(f, views[agent], symbol, record);
        }

        integer("subscription-input", subscription_, "agent", static_cast<std::int64_t>(agent));
        row("subscription-input", subscription_, "operation", removing ? "t:remove" : "t:add");
        row("subscription-input", subscription_, "record", units(f.schema.records()[record].name));
        row("subscription-input", subscription_++, "symbol", units(symbol));
    }

    /** Emit fields from an actual decoded or offered source record.
     * @param phase Exact input/output phase.
     * @param index Actual record ordinal.
     * @param record Real admitted immutable view.
     * @param flags Actual source flags or normalized delivery flags.
     */
    void record(std::string_view phase, std::int64_t index, const p::LiveRecordView &record, std::int32_t flags) {
        REQUIRE(record.typedValues.size() == 3);
        const auto hi = static_cast<std::uint32_t>(record.typedValues[0].raw);
        const auto lo = static_cast<std::uint32_t>(record.typedValues[1].raw);
        row(phase, index, "record", units(f.schema.records()[record.localId].name));
        row(phase, index, "symbol", units(record.symbol));
        integer(phase, index, "raw-time", std::bit_cast<std::int64_t>((static_cast<std::uint64_t>(hi) << 32) | lo));
        integer(phase, index, "value", record.typedValues[2].raw);
        integer(phase, index, "flags", flags);
    }

    /** Decode first, emit materialized INPUT, then apply the whole actual event.
     * @param inputs Complete representable tuples; REMOVE requires ordinary value zero.
     */
    void send(std::initializer_list<Input> inputs) {
        send(std::span(inputs.begin(), inputs.size()));
    }

    /** Decode a bounded dynamic input batch before recording any materialized input.
     * @param inputs Complete representable source tuples, unchanged through the call.
     */
    void send(std::span<const Input> inputs) {
        auto event = admit(f.live, data(inputs));
        const auto phase = "data-input-batch-" + std::to_string(batch_++);
        std::int64_t index = 0;

        for (const auto &item : event.records()) {
            record(phase, index++, item, item.eventFlags);
        }

        auto work = turn();
        REQUIRE(f.ticker.stage(std::move(event), work));
    }

    /** Preserve Java ordinal of an explicitly excluded direct-only unrepresentable operand. */
    void omitUnrepresentableBatch() {
        ++batch_;
    }

    /** Retrieve actual collector offers with finite capacity and emit exact counts.
     * @param phase Exact observation phase.
     * @param agent Actual selected view.
     * @param maximum Explicit sink capacity.
     * @return Actual owning observations for dependent source selection.
     */
    std::vector<Observed> retrieve(std::string_view phase, std::size_t agent, std::size_t maximum = 256) {
        std::vector<Observed> output;
        auto work = turn();

        while (output.size() < maximum) {
            auto next = f.ticker.front(views[agent], work);
            REQUIRE(next);

            if (!*next) {
                break;
            }

            record(phase, static_cast<std::int64_t>(output.size()), *(**next).value.sourceRecord,
                   (**next).deliveryFlags);
            output.push_back(observe(**next));
            REQUIRE(f.ticker.pop(views[agent], work));
        }

        integer(phase, -1, "record-count", static_cast<std::int64_t>(output.size()));

        // Buffer-internal Java more is intentionally outside the native semantic comparison.
        return output;
    }

    /** Query actual current cache availability.
     * @param phase Exact availability phase.
     */
    void availability(std::string_view phase) {
        auto work = turn();
        const auto a = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
        const auto b = f.ticker.latest({f.schema.recordContext(0), u"BETA"}, work);
        REQUIRE(a);
        REQUIRE(b);
        boolean(phase, "first-A-available", a->has_value());
        boolean(phase, "first-B-available", b->has_value());
    }
};
} // namespace

TEST_CASE("TICKER emits actual decoded INPUT and collector OUTPUT for locked selected production comparisons") {
    const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_TICKER_CAPTURE_DIR);
    std::filesystem::create_directories(directory);
    std::ofstream out(directory / "observations.tsv", std::ios::binary);
    REQUIRE(out.good());
    out << "case\tphase\tindex\tkey\tvalue\n";

    for (const bool store : {false, true}) {
        const auto suffix = store ? "true" : "false";
        {
            Trace t(out, std::string("ticker-latest-retention-") + suffix, store);
            const auto one = t.agent();
            t.send({{0, u"ALPHA", 1, 10}});
            t.sub(one, false, 0, u"ALPHA");
            t.retrieve("initial-after-unsubscribed-data", one);
            t.send({{0, u"ALPHA", 2, 20}});
            t.send({{0, u"ALPHA", 3, 30}});
            t.retrieve("latest-before-retrieve", one);
            const auto late = t.agent();
            t.sub(late, false, 0, u"ALPHA");
            t.retrieve("late-agent-initial", late);
            t.send({{0, u"ALPHA", 4, 40}});
            t.retrieve("live-first", one);
            t.retrieve("live-late", late);
            t.sub(one, true, 0, u"ALPHA");
            t.sub(late, true, 0, u"ALPHA");
            t.sub(one, false, 0, u"ALPHA");
            t.retrieve("last-unsubscribe-readd", one);
            t.sub(one, true, 0, u"ALPHA");
            t.send({{0, u"ALPHA", 5, 50}});
            t.sub(one, false, 0, u"ALPHA");
            t.retrieve("unsubscribed-update-readd", one);
        }
        {
            Trace t(out, std::string("ticker-equal-value-retention-") + suffix, store);
            const auto one = t.agent();
            t.sub(one, false, 0, u"ALPHA");
            t.send({{0, u"ALPHA", 1, 10}});
            t.retrieve("baseline", one);
            t.send({{0, u"ALPHA", 1, 10}});
            t.send({{0, u"ALPHA", 1, 10}});
            t.retrieve("identical-twice", one);
            t.send({{0, u"ALPHA", 1, 11}});
            t.send({{0, u"ALPHA", 1, 11}});
            t.retrieve("changed-value-twice", one);
            t.send({{0, u"ALPHA", 2, 11}});
            t.retrieve("time-only", one);
            t.send({{0, u"ALPHA", 2, 11, 1}});
            t.retrieve("flags-only", one);
            t.send({{0, u"ALPHA", 2, 11}});
            t.retrieve("flags-cleared", one);
        }
        {
            Trace t(out, std::string("ticker-raw-remove-retention-") + suffix, store);
            const auto one = t.agent();
            t.sub(one, false, 0, u"ALPHA");
            t.sub(one, false, 0, u"BETA");
            t.send({{0, u"ALPHA", 1, 10}});
            t.retrieve("baseline", one);
            t.availability("baseline-availability");
            // Java direct-distributor REMOVE(time1,value10) is not wire-representable. It was observed unchanged.
            t.omitUnrepresentableBatch();
            t.send({{0, u"ALPHA", 1, 0, 2}});
            t.retrieve("remove-existing-zero-value", one);
            t.send({{0, u"BETA", 0, 0, 2}});
            t.retrieve("remove-absent", one);
            t.availability("after-remove-availability");
            t.send({{0, u"BETA", 0, 0, 2}});
            t.retrieve("remove-absent-repeated", one);
            t.send({{0, u"ALPHA", 0, 0, 2}});
            t.retrieve("remove-existing-zero-time", one);
            t.send({{0, u"BETA", 1, 0, 2}});
            t.retrieve("remove-absent-time-only", one);
            t.sub(one, true, 0, u"ALPHA");
            t.sub(one, true, 0, u"BETA");
            t.availability("unsubscribed-availability");
            t.sub(one, false, 0, u"ALPHA");
            t.sub(one, false, 0, u"BETA");
            t.retrieve("readded-after-remove", one);
            t.send({{0, u"ALPHA", 2, 0, 2}});
            t.retrieve("remove-time-only", one);
            t.send({{0, u"ALPHA", 2, 0, 1}});
            t.retrieve("remove-flags-only", one);
            t.send({{0, u"ALPHA", 2, 0}});
            t.retrieve("remove-flags-cleared", one);
            t.availability("final-availability");
        }
    }

    {
        Trace t(out, "ticker-partial-initial-live-cutover", true);
        const Input seed[]{{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {1, u"ALPHA", 1, 30}};
        t.send({seed[0], seed[1], seed[2]});
        const auto one = t.agent();

        for (const auto &item : seed) {
            t.sub(one, false, item.record, item.symbol);
        }

        const auto first = t.retrieve("initial-first", one, 1);
        REQUIRE(first.size() == 1);
        t.integer("partial-control", 0, "sink-capacity-records", 1);
        t.boolean("partial-control", "more-after-first", *t.f.ticker.pendingKeys(t.views[one]) != 0);
        const auto unconsumed = std::ranges::find_if(seed, [&](const auto &item) {
            return item.record != first[0].record || item.symbol != first[0].symbol;
        });
        REQUIRE(unconsumed != std::end(seed));
        t.send({{unconsumed->record, unconsumed->symbol, 2, unconsumed->value + 1}});
        t.send({{first[0].record, first[0].symbol, 3, first[0].value + 2}});
        t.retrieve("remaining-initial-and-live", one);
        t.retrieve("after-drain", one);
    }
    {
        Trace t(out, "ticker-pending-readd-with-keeper", false);
        const auto one = t.agent();
        const auto keeper = t.agent();
        t.sub(one, false, 0, u"ALPHA");
        t.sub(one, false, 0, u"BETA");
        t.sub(one, false, 1, u"ALPHA");
        t.sub(keeper, false, 0, u"BETA");
        t.send({{0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {1, u"ALPHA", 1, 30}});
        REQUIRE(t.retrieve("initial-first", one, 1).size() == 1);
        t.integer("partial-control", 0, "sink-capacity-records", 1);
        t.boolean("partial-control", "more-after-first", *t.f.ticker.pendingKeys(t.views[one]) != 0);
        t.sub(one, true, 0, u"BETA");
        t.sub(one, false, 0, u"BETA");
        t.send({{0, u"BETA", 2, 21}});
        t.retrieve("remaining-after-readd", one);
        t.retrieve("keeper-after-update", keeper);
        t.retrieve("after-drain", one);
    }
    out.close();
    REQUIRE(out.good());
}

TEST_CASE("TICKER exact old plus candidate byte capacity admits replacement and one less fails visibly") {
    for (const bool exact : {true, false}) {
        auto local = schema();
        auto live = session(local);
        const Input first[]{{0, u"ALPHA", 1, 10}};
        auto event = admit(live, data(first));
        const auto backing = event.residentBytes();
        auto bounds = limits();
        bounds.cacheKeys = 1;
        bounds.cachedFrames = 2;
        bounds.cacheBytes = backing * 2 - (exact ? 0 : 1);
        auto copy = local;
        auto work = turn();
        auto ticker = s::TickerCollector::create(std::move(copy), bounds, {false, 100}, work);
        REQUIRE(ticker);
        Fixture f{std::move(local), std::move(live), std::move(*ticker)};
        auto view = open(f);
        add(f, view, u"ALPHA");
        REQUIRE(f.ticker.stage(std::move(event), work));
        CHECK(f.ticker.cacheBytes() == backing);
        const Input next[]{{0, u"ALPHA", 2, 20}};
        auto replacement = admit(f.live, data(next));
        REQUIRE(replacement.residentBytes() == backing);
        auto result = f.ticker.stage(std::move(replacement), work);

        if (exact) {
            REQUIRE(result);
            CHECK(replacement.residentBytes() == 0);
            CHECK(f.ticker.cachedFrames() == 1);
            CHECK(f.ticker.cacheBytes() == backing);
        } else {
            REQUIRE_FALSE(result);
            CHECK(result.error() == Error::RESOURCE_LIMIT);
            CHECK(replacement.residentBytes() == backing);
            CHECK(f.ticker.state() == s::TickerState::STALE);
        }
    }
}

TEST_CASE("TICKER zero capacities checked arithmetic and empty admission have finite explicit outcomes") {
    auto bounds = limits();
    bounds.viewSlots = 0;
    bounds.cacheKeys = 0;
    bounds.cachedFrames = 0;
    bounds.cacheBytes = 0;
    auto f = fixture(false, bounds);
    auto work = turn();
    CHECK(f.ticker.openView(work).error() == Error::RESOURCE_LIMIT);
    const Input input[]{{0, u"ALPHA", 1, 10}};

    for (unsigned i = 0; i < 32; ++i) {
        auto event = admit(f.live, data(input));
        auto result = f.ticker.stage(std::move(event), work);
        REQUIRE(result);
        CHECK(result->ignored == 1);
        CHECK(event.residentBytes() > 0);
    }

    CHECK(f.ticker.cachedKeys() == 0);
    CHECK(f.ticker.cacheBytes() == 0);
    CHECK(f.ticker.state() == s::TickerState::CURRENT);
    auto local = schema();
    const auto context = local.recordContext(0);
    bounds = limits();
    bounds.cacheKeys = std::numeric_limits<std::size_t>::max();
    auto invalid = s::TickerCollector::create(std::move(local), bounds, {false, 100}, work);
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == Error::INVALID_CONFIGURATION);
    CHECK(local.recordContext(0).matches(context));
    bounds = limits();
    bounds.residentBytes = 0;
    CHECK(s::TickerCollector::create(std::move(local), bounds, {false, 100}, work).error() == Error::RESOURCE_LIMIT);
    bounds = limits();
    CHECK(s::TickerCollector::create(std::move(local), bounds, {false, 0}, work).error() ==
          Error::INVALID_CONFIGURATION);
}

TEST_CASE("TICKER successful admission offer and pop allocate no backing beyond fixed reserved storage") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    auto event = admit(f.live, data(input));
    auto work = turn();
    auto staged = [&] {
        AllocationFault fault(0);

        return f.ticker.stage(std::move(event), work);
    }();
    REQUIRE(staged);
    auto next = [&] {
        AllocationFault fault(0);

        return f.ticker.front(view, work);
    }();
    REQUIRE((next && *next));
    auto popped = [&] {
        AllocationFault fault(0);

        return f.ticker.pop(view, work);
    }();
    REQUIRE(popped);
    CHECK(f.ticker.pendingKeys(view) == 0);
    CHECK(f.ticker.cachedFrames() == 1);
}

TEST_CASE("TICKER quantum boundary orders a live update before a newly joined initial key") {
    auto f = fixture(true, limits(), 2);
    auto view = open(f);
    const Input seed[]{{0, u"A", 1, 1}, {0, u"B", 1, 2}, {0, u"C", 1, 3}, {0, u"D", 1, 4}, {0, u"F", 1, 6}};
    stage(f, seed);

    for (const auto symbol : {u"A", u"B", u"C", u"D"}) {
        add(f, view, symbol);
    }

    auto first = drain(f, view, 2);
    REQUIRE(first.size() == 2);
    CHECK(first[0].symbol == u"A");
    CHECK(first[1].symbol == u"B");
    const Input update[]{{0, u"A", 2, 10}};
    stage(f, update);
    add(f, view, u"F");
    auto rest = drain(f, view);
    REQUIRE(rest.size() == 4);
    CHECK(rest[0].symbol == u"C");
    CHECK(rest[1].symbol == u"D");
    CHECK(rest[2].symbol == u"A");
    CHECK(rest[3].symbol == u"F");
    CHECK(rest[2].kind == s::TickerDeliveryKind::LIVE);
    CHECK(rest[3].kind == s::TickerDeliveryKind::INITIAL);
}

TEST_CASE("TICKER whole failed frames and foreign schema never publish completed decoder prefixes") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input valid[]{{0, u"ALPHA", 1, 10}};
    stage(f, valid);
    REQUIRE(drain(f, view).size() == 1);
    const Input failed[]{{0, u"ALPHA", 2, 20}, {0, u"ALPHA", 3, 30}};
    auto bytes = data(failed);
    bytes.pop_back();
    REQUIRE(std::to_integer<unsigned>(bytes[0]) < 128);
    bytes[0] = static_cast<std::byte>(std::to_integer<unsigned>(bytes[0]) - 1);
    auto protocolWork = liveTurn();
    auto decoded = f.live.feed(bytes, protocolWork);
    CHECK(decoded.status == p::LiveStatus::FAILED);
    CHECK_FALSE(decoded.event);
    auto work = turn();
    auto cached = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE((cached && *cached));
    CHECK((**cached).sourceRecord->typedValues[2].raw == 10);
    CHECK(f.ticker.pendingKeys(view) == 0);
    auto other = fixture();
    auto foreign = admit(other.live, data(valid));
    auto blankCopy = f.schema;
    auto blank = s::TickerCollector::create(std::move(blankCopy), limits(), {true, 100}, work);
    REQUIRE(blank);
    CHECK(blank->stage(std::move(foreign), work).error() == Error::INVALID_CONTEXT);
    CHECK(foreign.residentBytes() > 0);
    CHECK(blank->cachedKeys() == 0);
}

TEST_CASE("TICKER changed binding generations retain original source provenance in one pinned epoch") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    add(f, view, u"BETA");
    const Input first[]{{0, u"ALPHA", 1, 10}};
    stage(f, first);
    auto work = turn();
    auto a = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE((a && *a));
    const auto original = (**a).bindingGeneration;
    const auto epoch = (**a).epoch;
    (void)admit(f.live, description(73, 0));
    const Input second[]{{0, u"BETA", 2, 20}};
    stage(f, second);
    auto b = f.ticker.latest({f.schema.recordContext(0), u"BETA"}, work);
    REQUIRE((b && *b));
    CHECK((**b).bindingGeneration > original);
    CHECK(epoch.matches((**b).epoch));
    auto retained = f.ticker.latest({f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE((retained && *retained));
    CHECK((**retained).bindingGeneration == original);
    CHECK(drain(f, view).size() == 2);
}

TEST_CASE("TICKER exact nullable text and opaque latest values survive the original serialized driver") {
    const c::LocalRecord declaration{0,
                                     u"Text",
                                     false,
                                     {{10}, {137}, {9}, {233}, {249}},
                                     {{u"array", 0}, {u"string", 1}, {u"bytes", 2}, {u"custom", 3}, {u"serial", 4}}};
    auto local =
        c::LocalSchema::create(std::span(&declaration, 1), {1, 5, 5, 16, 128}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto copy = *local;
    auto work = turn();
    auto ticker = s::TickerCollector::create(std::move(copy), limits(), {true, 100}, work);
    REQUIRE(ticker);
    std::u16string exact(40, u'A');
    exact[0] = 0;
    exact[1] = static_cast<char16_t>(0xd800);
    exact[2] = static_cast<char16_t>(0xdc00);
    {
        auto live = genericSession(*local);
        std::vector<std::byte> payload;
        number(payload, 32);
        text(payload, u"Text");
        number(payload, 5);

        for (std::size_t i = 0; i < declaration.fields.size(); ++i) {
            text(payload, declaration.lookups[i].name);
            number(payload, declaration.fields[i].serialTypeId);
        }

        (void)admit(live, frame(2, payload));
        payload.clear();
        /** Append one exact non-null symbol and actual sender selection.
         * @param value Exact units preserved by the real decoder.
         */
        const auto symbol = [&](std::u16string_view value) {
            payload.push_back(std::byte{0xfd});
            number(payload, static_cast<std::int64_t>(value.size()));

            for (const auto unit : value) {
                std::array<std::byte, 4> bytes{};
                const auto written = e::writeUtfChar(unit, bytes).written;
                payload.insert(payload.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(written));
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
        auto array = e::writeCharArray(exact, encoded, {512, 128});
        REQUIRE(array.status == e::Status::OK);
        payload.insert(payload.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(array.written));
        text(payload, exact);
        number(payload, 2);
        payload.insert(payload.end(), {std::byte{0}, std::byte{0xff}});
        number(payload, 0);
        number(payload, -1);
        auto event = admit(live, frame(10, payload));
        auto staged = ticker->stage(std::move(event), work);
        REQUIRE(staged);
        CHECK(staged->changed == 2);
        auto equal = admit(live, frame(10, payload));
        auto unchanged = ticker->stage(std::move(equal), work);
        REQUIRE(unchanged);
        CHECK(unchanged->equal == 2);
        CHECK_FALSE(unchanged->retained);
        CHECK(ticker->cachedFrames() == 1);
    }
    auto view = ticker->openView(work);
    REQUIRE(view);
    const c::SubscriptionRequest keys[]{{{local->recordContext(0), u"NULL"}, {}},
                                        {{local->recordContext(0), u"TEXT"}, {}}};
    auto core = command();
    REQUIRE(ticker->add(*view, keys, core, work));
    auto first = ticker->front(*view, work);
    REQUIRE((first && *first));
    const auto nulls = (**first).value.sourceRecord->typedValues;
    REQUIRE(nulls.size() == 5);
    CHECK_FALSE(nulls[0].units);
    REQUIRE(nulls[1].units);
    CHECK(nulls[1].units->empty());
    REQUIRE(nulls[2].bytes);
    CHECK(nulls[2].bytes->empty());
    CHECK_FALSE(nulls[3].bytes);
    REQUIRE(nulls[4].bytes);
    CHECK(nulls[4].bytes->size() == 3);
    REQUIRE(ticker->pop(*view, work));
    auto second = ticker->front(*view, work);
    REQUIRE((second && *second));
    const auto fields = (**second).value.sourceRecord->typedValues;
    REQUIRE(fields[0].units);
    REQUIRE(fields[1].units);
    CHECK(*fields[0].units == exact);
    CHECK(*fields[1].units == exact);
    REQUIRE(fields[2].bytes);
    CHECK(fields[2].bytes->size() == 2);
    REQUIRE(fields[3].bytes);
    CHECK(fields[3].bytes->empty());
    CHECK_FALSE(fields[4].bytes);
    REQUIRE(ticker->pop(*view, work));
}

TEST_CASE("TICKER zero initialization visits cannot construct a large fixed frame array") {
    auto local = schema();
    auto bounds = limits();
    bounds.viewSlots = 0;
    bounds.cacheKeys = 0;
    bounds.cachedFrames = 4096;
    auto work = turn();
    work.visits = 0;
    auto result = s::TickerCollector::create(std::move(local), bounds, {false, 100}, work);
    CHECK_FALSE(result);

    if (!result) {
        CHECK(result.error() == Error::WORK_LIMIT);
    }
}

TEST_CASE("TICKER pop work includes every reserved view invalidation and preserves an offer on rejection") {
    auto bounds = limits();
    bounds.viewSlots = 64;
    bounds.cacheKeys = 1;
    bounds.cachedFrames = 2;
    auto f = fixture(false, bounds);
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(f, input);
    auto work = turn();
    REQUIRE(f.ticker.front(view, work));
    s::TickerWork insufficient{1, 4, 0, 0};
    auto result = f.ticker.pop(view, insufficient);
    CHECK_FALSE(result);

    if (!result) {
        CHECK(result.error() == Error::WORK_LIMIT);
    }

    CHECK(f.ticker.pendingKeys(view) == 1);
    REQUIRE(f.ticker.pop(view, work));
}

TEST_CASE("TICKER mutation work preflights repeated long desired scans before atomic publication") {
    auto bounds = limits();
    bounds.viewSlots = 1;
    bounds.cacheKeys = 1;
    bounds.cachedFrames = 2;
    bounds.viewSubscriptions = {16, 1024, 128, 16384, 2048, 32768, 2048};
    auto f = fixture(false, bounds);
    auto view = open(f);
    std::vector<std::u16string> symbols;
    symbols.reserve(1001);

    for (unsigned i = 0; i < 1000; ++i) {
        auto value = std::to_string(i);
        std::u16string symbol = u"K";
        symbol.append(value.begin(), value.end());
        symbols.push_back(std::move(symbol));
    }

    symbols.push_back(u"ZCACHE");
    std::vector<c::SubscriptionRequest> requests;

    for (const auto &symbol : symbols) {
        requests.push_back({{f.schema.recordContext(0), symbol}, {}});
    }

    auto core = command();
    core.keyVisits = 100000000;
    core.symbolUnits = 100000000;
    auto work = turn();
    REQUIRE(f.ticker.add(view, requests, core, work));
    const Input input[]{{0, u"ZCACHE", 1, 10}};
    stage(f, input);
    const auto revision = f.ticker.desired(view)->revision();
    const c::SubscriptionRequest added{{f.schema.recordContext(0), u"NEW"}, {}};
    auto commandWork = command();
    s::TickerWork limited{1, 1500, 1000000, 100000000};
    auto result = f.ticker.add(view, std::span(&added, 1), commandWork, limited);
    CHECK_FALSE(result);

    if (!result) {
        CHECK(result.error().error == Error::WORK_LIMIT);
    }

    CHECK(f.ticker.desired(view)->revision() == revision);
    CHECK(f.ticker.desired(view)->entries().size() == 1001);
}

namespace {
/** Supply a bounded actual profile for the measured default-100 boundary.
 * @return One view, 128 cache/desired slots and one real frame holding 104 complete records.
 */
Fixture largeFixture() {
    auto local = schema();
    auto admission = liveLimits();
    admission.data.processedRecords = 128;
    admission.data.emittedRecords = 128;
    admission.eventBytes = 262144;
    auto live = session(local, admission);
    auto copy = local;
    auto bounds = limits();
    bounds.viewSlots = 1;
    bounds.cacheKeys = 128;
    bounds.cachedFrames = 3;
    bounds.viewSubscriptions = {16, 128, 128, 16384, 256, 32768, 256};
    auto work = turn();
    auto ticker = s::TickerCollector::create(std::move(copy), bounds, {true, 100}, work);
    REQUIRE(ticker);

    return {std::move(local), std::move(live), std::move(*ticker)};
}

/** Construct the exact source-selected default-quantum symbol.
 * @param ordinal Bounded scenario ordinal 0..103.
 * @return Q followed by exactly three decimal UTF-16 digits.
 */
std::u16string quantumSymbol(unsigned ordinal) {
    std::u16string result = u"Q000";
    result[1] = static_cast<char16_t>(u'0' + ordinal / 100);
    result[2] = static_cast<char16_t>(u'0' + ordinal / 10 % 10);
    result[3] = static_cast<char16_t>(u'0' + ordinal % 10);

    return result;
}
} // namespace

TEST_CASE("TICKER measured default quantum retains an untraversed update link before remaining initials") {
    auto f = largeFixture();
    auto view = open(f);
    std::vector<std::u16string> symbols;
    symbols.reserve(104);

    for (unsigned i = 0; i < 104; ++i) {
        symbols.push_back(quantumSymbol(i));
    }

    std::vector<Input> input;

    for (unsigned i = 0; i < 104; ++i) {
        input.push_back({0, symbols[i], 1, static_cast<std::int32_t>(i + 10)});
    }

    stage(f, input);

    for (const auto &symbol : symbols) {
        add(f, view, symbol);
    }

    auto first = drain(f, view, 1);
    REQUIRE(first.size() == 1);
    const Input update[]{{first[0].record, first[0].symbol, 2, first[0].value + 1000}};
    stage(f, update);
    auto initials = drain(f, view, 99);
    REQUIRE(initials.size() == 99);
    auto boundary = drain(f, view, 1);
    REQUIRE(boundary.size() == 1);
    CHECK(boundary[0].symbol == first[0].symbol);
    CHECK(boundary[0].value == first[0].value + 1000);
    auto rest = drain(f, view);
    REQUIRE(rest.size() == 4);

    for (std::size_t i = 0; i < rest.size(); ++i) {
        CHECK(rest[i].symbol == symbols[100 + i]);
    }
}

TEST_CASE("TICKER abandoned front cannot advance retrieval credit before acknowledgement") {
    std::vector<std::u16string> baseline;

    for (const bool peek : {false, true}) {
        auto f = fixture(true, limits(), 2);
        auto view = open(f);
        const Input input[]{{0, u"A", 1, 10}, {0, u"B", 1, 20}, {0, u"C", 1, 30}};
        stage(f, input);
        add(f, view, u"A");
        add(f, view, u"B");
        REQUIRE(drain(f, view, 1).size() == 1);
        remove(f, view, u"B");
        const Input update[]{{0, u"A", 2, 11}};
        stage(f, update);

        if (peek) {
            auto work = turn();
            auto offer = f.ticker.front(view, work);
            REQUIRE((offer && *offer));
        }

        add(f, view, u"C");
        const auto records = drain(f, view);
        REQUIRE(records.size() == 2);

        if (!peek) {
            for (const auto &record : records) {
                baseline.push_back(record.symbol);
            }
        } else {
            for (std::size_t i = 0; i < records.size(); ++i) {
                CHECK(records[i].symbol == baseline[i]);
            }
        }
    }
}

TEST_CASE("TICKER detached marker copies and final release stay thread safe after owner retirement") {
    std::atomic<unsigned> ready{0};
    std::atomic<bool> go{false};
    std::atomic<bool> failed{false};
    std::vector<std::jthread> threads;
    {
        auto f = fixture();
        auto view = open(f);
        add(f, view, u"ALPHA");
        const Input input[]{{0, u"ALPHA", 1, 10}};
        stage(f, input);
        auto work = turn();
        auto next = f.ticker.front(view, work);
        REQUIRE((next && *next));

        for (unsigned i = 0; i < 4; ++i) {
            threads.emplace_back([&, token = view, epoch = (**next).value.epoch]() mutable {
                ++ready;
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

                while (!go.load() && std::chrono::steady_clock::now() < deadline) {
                    std::this_thread::yield();
                }

                if (!go.load()) {
                    failed = true;

                    return;
                }

                for (unsigned n = 0; n < 10000; ++n) {
                    auto copy = token;
                    auto moved = std::move(copy);
                    copy = moved;
                    auto generation = epoch;
                    auto second = std::move(generation);
                    generation = second;

                    if (!copy.valid() || !copy.matches(token) || !generation.matches(epoch) ||
                        copy.matches(generation)) {
                        failed = true;
                    }
                }

                token = {};
                epoch = {};
            });
        }

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

        while (ready.load() != 4 && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }

        CHECK(ready.load() == 4);
    }
    // Only detached fixed markers cross this boundary. All protocol owners were released by their serialized owner.
    go = true;

    for (auto &thread : threads) {
        thread.join();
    }

    CHECK_FALSE(failed.load());
}

TEST_CASE("TICKER emits actual default quantum boundary input and output for production comparison") {
    const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_TICKER_CAPTURE_DIR);
    std::filesystem::create_directories(directory);
    std::ofstream out(directory / "quantum-observations.tsv", std::ios::binary);
    REQUIRE(out.good());
    out << "case\tphase\tindex\tkey\tvalue\n";
    Trace t(out, "ticker-initial-update-quantum-boundary", largeFixture());
    std::vector<std::u16string> symbols;
    symbols.reserve(104);

    for (unsigned i = 0; i < 104; ++i) {
        symbols.push_back(quantumSymbol(i));
    }

    std::vector<Input> input;

    for (unsigned i = 0; i < 104; ++i) {
        input.push_back({0, symbols[i], 1, static_cast<std::int32_t>(i + 10)});
    }

    t.send(input);
    const auto view = t.agent();

    for (const auto &symbol : symbols) {
        t.sub(view, false, 0, symbol);
    }

    const auto first = t.retrieve("initial-first", view, 1);
    REQUIRE(first.size() == 1);
    t.integer("initial-first-control", 0, "sink-capacity-records", 1);
    t.integer("initial-first-control", 0, "cumulative-delivered", static_cast<std::int64_t>(first.size()));
    t.boolean("initial-first-control", "provider-more", *t.f.ticker.pendingKeys(t.views[view]) != 0);
    t.send({{first[0].record, first[0].symbol, 2, first[0].value + 1000}});
    const auto before = t.retrieve("initial-next-99", view, 99);
    t.integer("initial-next-99-control", 0, "sink-capacity-records", 99);
    t.integer("initial-next-99-control", 0, "cumulative-delivered",
              static_cast<std::int64_t>(first.size() + before.size()));
    t.boolean("initial-next-99-control", "provider-more", *t.f.ticker.pendingKeys(t.views[view]) != 0);
    const auto boundary = t.retrieve("first-after-quantum", view, 1);
    t.integer("first-after-quantum-control", 0, "sink-capacity-records", 1);
    t.integer("first-after-quantum-control", 0, "cumulative-delivered",
              static_cast<std::int64_t>(first.size() + before.size() + boundary.size()));
    t.boolean("first-after-quantum-control", "provider-more", *t.f.ticker.pendingKeys(t.views[view]) != 0);
    t.boolean("quantum-comparison", "same-key-as-consumed",
              !boundary.empty() && boundary[0].record == first[0].record && boundary[0].symbol == first[0].symbol);
    t.retrieve("remainder", view);
    t.retrieve("after-drain", view);
    out.close();
    REQUIRE(out.good());
}

TEST_CASE("TICKER front work rejection preserves a prepared offer and retry performs one acknowledgement") {
    auto f = fixture();
    auto view = open(f);
    add(f, view, u"ALPHA");
    const Input input[]{{0, u"ALPHA", 1, 10}};
    stage(f, input);
    auto work = turn();
    auto first = f.ticker.front(view, work);
    REQUIRE((first && *first));
    s::TickerWork insufficient{1, 0, 0, 0};
    CHECK(f.ticker.front(view, insufficient).error() == Error::WORK_LIMIT);
    CHECK(f.ticker.pendingKeys(view) == 1);
    REQUIRE(f.ticker.pop(view, work));
    CHECK(f.ticker.pendingKeys(view) == 0);
    CHECK(f.ticker.counts().delivered == 1);
}
