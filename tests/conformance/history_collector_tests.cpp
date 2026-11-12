// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "../../src/collectors/history_replay.hpp"
#include "qd/collectors/history.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/market_maker.hpp"
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
#include <stdexcept>
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
using Error = s::HistoryError;
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
s::HistoryWork turn() {
    return {100000, 1000000000000ULL, 1000000000000ULL, 1000000000000ULL};
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
s::HistoryLimits limits() {
    return {16,
            256,
            128,
            64,
            2 * 1024 * 1024,
            4,
            256,
            64,
            2 * 1024 * 1024,
            32 * 1024 * 1024,
            {16, 32, 128, 2048, 64, 4096, 64}};
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

/** Independent source/collector, with all protocol access serialized. */
struct Fixture {
    c::LocalSchema schema;       ///< Exact retained local identity.
    p::LiveSession live;         ///< Actual complete-frame driver.
    s::HistoryCollector history; ///< Bounded generic HISTORY owner.
};

/** Build one controlled source graph.
 * @param store Retention mode.
 * @param bounds Explicit finite logical capacities.
 * @param retransmit Exact snapshot retransmission records, borrowed through construction.
 * @return Ready actual source and empty collector.
 */
Fixture fixture(bool store = false, s::HistoryLimits bounds = limits(), std::span<const std::int32_t> retransmit = {}) {
    auto local = schema();
    auto live = session(local);
    auto copied = local;
    auto work = turn();
    auto collector = s::HistoryCollector::create(
        std::move(copied), bounds, {store, 100, 10000, c::SubscriptionSelectorPolicy::EXACT_ONLY, retransmit}, work);
    REQUIRE(collector);

    return {std::move(local), std::move(live), std::move(*collector)};
}

/** Open a bounded projection.
 * @param f Serialized graph.
 * @param snapshot True for snapshot/transaction flags.
 * @return Detached view identity.
 */
s::HistoryViewToken open(Fixture &f, bool snapshot = true) {
    auto work = turn();
    auto result = f.history.openView({snapshot}, work);
    REQUIRE(result);

    return std::move(*result);
}

/** Apply one exact generic HISTORY interest.
 * @param f Source graph.
 * @param view Local view.
 * @param bound Inclusive raw lower bound.
 * @param record Local ID.
 * @param symbol Exact units.
 */
void add(Fixture &f, const s::HistoryViewToken &view, std::int64_t bound = 0, std::int32_t record = 0,
         std::u16string_view symbol = u"ALPHA") {
    const c::SubscriptionRequest requests[]{{{f.schema.recordContext(record), symbol}, bound}};
    auto work = turn();
    auto coreWork = command();
    auto result = f.history.add(view, requests, coreWork, work);
    REQUIRE(result);
}

/** Stage actual source tuples.
 * @param f Serialized graph.
 * @param values Ordered raw inputs.
 * @return Exact whole-frame outcome.
 */
s::HistoryStage send(Fixture &f, std::initializer_list<Input> values) {
    auto event = admit(f.live, data(std::span<const Input>(values.begin(), values.size())));
    auto work = turn();
    auto result = f.history.stage(std::move(event), work);
    INFO("history error=", result ? -1 : static_cast<int>(result.error().error));
    REQUIRE(result);

    return *result;
}

/** Detached projection tuple for assertions; no borrowed payload escapes pop. */
struct Output {
    std::int64_t time;  ///< Raw delivered index.
    std::int64_t value; ///< Exact generic Value field.
    std::int32_t flags; ///< Produced delivery flags.
};

/** Drain a finite owner prefix.
 * @param f Graph.
 * @param view Active view.
 * @param maximum Maximum acknowledged projections.
 * @return Actual tuples, not raw decoder flags.
 */
std::vector<Output> drain(Fixture &f, const s::HistoryViewToken &view, std::size_t maximum = 1024) {
    std::vector<Output> out;
    auto work = turn();

    while (out.size() < maximum) {
        auto value = f.history.front(view, work);
        REQUIRE(value);

        if (!*value) {
            break;
        }

        out.push_back({(**value).record.rawTime, (**value).record.fields[2].raw, (**value).record.eventFlags});
        REQUIRE(f.history.pop(view, work));
    }

    return out;
}
} // namespace

TEST_CASE("HISTORY empty and complete snapshots expose actual projected boundaries") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 0, 0, 14}});
    auto empty = drain(f, view);
    REQUIRE(empty.size() == 1);
    CHECK(empty[0].time == 0);
    CHECK(empty[0].flags == 14);
    auto complete = fixture();
    auto completeView = open(complete);
    add(complete, completeView);
    send(complete, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    auto values = drain(complete, completeView);
    REQUIRE(values.size() == 3);
    CHECK(values[0].time == 3);
    CHECK(values[0].value == 30);
    CHECK(values[0].flags == 4);
    CHECK(values[1].time == 2);
    CHECK(values[1].flags == 0);
    CHECK(values[2].time == 0);
    CHECK(values[2].flags == 10);
}

TEST_CASE("HISTORY retransmits only the configured snapshot records after input storage is reused") {
    std::array<std::int32_t, 1> selected{0};
    auto configured = fixture(false, limits(), selected);
    selected.front() = 1;
    auto standard = fixture();

    for (auto *f : {&configured, &standard}) {
        const auto view = open(*f);
        add(*f, view);
        send(*f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
        REQUIRE(drain(*f, view).size() == 2);
        send(*f, {{0, u"ALPHA", 3, 31, 4}});
        const auto replay = drain(*f, view);
        REQUIRE(replay.size() == 1);
        CHECK(replay[0].time == 3);
        CHECK(replay[0].value == 31);
        CHECK(((replay[0].flags & 4) != 0) == (f == &configured));
    }
}

TEST_CASE("HISTORY pending transaction progress remains observable and explicitly incomplete") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    send(f, {{0, u"ALPHA", 3, 31, 1}});
    auto pending = drain(f, view);
    REQUIRE(pending.size() == 1);
    CHECK(pending[0].flags == 1);
    auto work = turn();
    auto state = f.history.rangeStatus(view, {f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE(state);
    CHECK(state->sourceTransactionPending);
    CHECK_FALSE(state->completeRequestedRange);
    send(f, {{0, u"ALPHA", 2, 21, 1}});
    pending = drain(f, view);
    REQUIRE(pending.size() == 1);
    CHECK(pending[0].flags == 1);
    send(f, {{0, u"ALPHA", 1, 11, 0}});
    auto end = drain(f, view);
    REQUIRE(end.size() == 1);
    CHECK(end[0].flags == 0);
    work = turn();
    state = f.history.rangeStatus(view, {f.schema.recordContext(0), u"ALPHA"}, work);
    REQUIRE(state);
    CHECK(state->completeRequestedRange);
}

namespace {
using Coordinate = std::tuple<std::string, std::string, std::int32_t, std::string>;

/** Encode exact UTF16 in the locked semantic TSV grammar.
 * @param value Exact units, including NUL/unpaired units.
 * @return Canonical lowercase hexadecimal representation.
 */
std::string units(std::u16string_view value) {
    std::ostringstream out;
    out << "u:" << std::hex << std::setfill('0');

    for (auto u : value) {
        out << std::setw(4) << static_cast<unsigned>(u);
    }

    return out.str();
}

/** Read exact committed production observations, never handwritten output rows.
 * @return Full coordinate/value ledger.
 */
std::map<Coordinate, std::string> observations() {
    std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_HISTORY_SOURCE_ROOT) /
                        "tests/fixtures/subscriptions/expected/collectors/observations.tsv");
    REQUIRE(input);
    std::map<Coordinate, std::string> result;
    std::string line;
    std::getline(input, line);

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream row(line);
        std::string name, phase, index, key, value;

        if (!std::getline(row, name, '\t') || !std::getline(row, phase, '\t') || !std::getline(row, index, '\t') ||
            !std::getline(row, key, '\t')) {
            throw std::runtime_error("Malformed production observation row");
        }

        std::getline(row, value);
        result.emplace(Coordinate{name, phase, std::stoi(index), key}, value);
    }

    return result;
}

/** Detached actual projection for finite-retrieval controls. */
struct TraceRecord {
    std::int32_t record;   ///< Actual local ID.
    std::u16string symbol; ///< Exact delivered symbol copy.
    std::int64_t time;     ///< Actual projected index.
    std::int32_t value;    ///< Actual Value raw field.
    std::int32_t flags;    ///< Actual projected flags.
};

/** Controlled actual native path, binding every emitted row to production observations. */
struct Trace {
    Fixture f;                                         ///< Real decoder and collector.
    std::string name;                                  ///< Named production scenario.
    std::ostream &out;                                 ///< Retained actual native evidence.
    const std::map<Coordinate, std::string> &expected; ///< Actual locked Java coordinate ledger.
    std::vector<s::HistoryViewToken> views;            ///< Independent bounded views.
    std::size_t batch = 0;                             ///< Source batch ordinal.
    std::size_t subscription = 0;                      ///< Exact command ordinal.

    /** Create one independent measured scenario.
     * @param scenario Production case ID.
     * @param store Explicit retention.
     * @param stream Evidence output.
     * @param golden Production observations.
     */
    Trace(std::string scenario, bool store, std::ostream &stream, const std::map<Coordinate, std::string> &golden)
        : f(fixture(store)), name(std::move(scenario)), out(stream), expected(golden) {
    }

    /** Emit one actual row and compare its exact locked value.
     * @param phase Exact scenario phase.
     * @param index Record/control ordinal, or -1 for count.
     * @param key Exact coordinate key.
     * @param value Actual native value.
     */
    void row(std::string_view phase, std::int32_t index, std::string_view key, const std::string &value) {
        out << name << '\t' << phase << '\t' << index << '\t' << key << '\t' << value << '\n';
        const auto it = expected.find(Coordinate{name, std::string(phase), index, std::string(key)});
        INFO(name, " / ", phase, " / ", index, " / ", key);
        REQUIRE(it != expected.end());
        CHECK(value == it->second);
    }

    /** Emit exact signed integer.
     * @param phase Phase.
     * @param index Ordinal.
     * @param key Coordinate.
     * @param value Actual integer.
     */
    void integer(std::string_view phase, std::int32_t index, std::string_view key, std::int64_t value) {
        row(phase, index, key, "i:" + std::to_string(value));
    }

    /** Create one pure agent-equivalent view.
     * @param snapshot Projection mode.
     * @return Stable local view ordinal.
     */
    std::size_t agent(bool snapshot = true) {
        views.push_back(open(f, snapshot));

        return views.size() - 1;
    }

    /** Apply and record an actual local command.
     * @param view View ordinal.
     * @param remove Remove rather than add.
     * @param record Local record.
     * @param symbol Exact symbol.
     * @param bound Raw inclusive lower bound for add.
     */
    void sub(std::size_t view, bool remove, std::int32_t record = 0, std::u16string_view symbol = u"ALPHA",
             std::int64_t bound = 0) {
        const auto index = static_cast<std::int32_t>(subscription++);
        auto work = turn();
        auto coreWork = command();
        const c::SubscriptionKey key{f.schema.recordContext(record), symbol};

        if (remove) {
            REQUIRE(f.history.remove(views[view], std::span(&key, 1), coreWork, work));
        } else {
            const c::SubscriptionRequest request{key, bound};
            REQUIRE(f.history.add(views[view], std::span(&request, 1), coreWork, work));
        }

        integer("subscription-input", index, "agent", static_cast<std::int64_t>(view));
        row("subscription-input", index, "operation", remove ? "t:remove" : "t:add");
        row("subscription-input", index, "record", units(f.schema.records()[static_cast<std::size_t>(record)].name));
        row("subscription-input", index, "symbol", units(symbol));

        if (!remove) {
            integer("subscription-input", index, "raw-bound", bound);
        }
    }

    /** Materialize actual postdecoder input before staging it.
     * @param inputs Complete source batch.
     */
    void send(std::span<const Input> inputs) {
        auto event = admit(f.live, data(inputs));
        const auto phase = "data-input-batch-" + std::to_string(batch++);

        for (std::size_t i = 0; i < event.records().size(); ++i) {
            const auto &r = event.records()[i];
            const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(r.typedValues[0].raw)) << 32) |
                              static_cast<std::uint32_t>(r.typedValues[1].raw);
            row(phase, static_cast<std::int32_t>(i), "record",
                units(f.schema.records()[static_cast<std::size_t>(r.localId)].name));
            row(phase, static_cast<std::int32_t>(i), "symbol", units(r.symbol));
            integer(phase, static_cast<std::int32_t>(i), "raw-time", std::bit_cast<std::int64_t>(bits));
            integer(phase, static_cast<std::int32_t>(i), "value", r.typedValues[2].raw);
            integer(phase, static_cast<std::int32_t>(i), "flags", r.eventFlags);
        }

        REQUIRE(event.records().size() == inputs.size());
        auto work = turn();
        auto result = f.history.stage(std::move(event), work);
        INFO("stage error=", result ? -1 : static_cast<int>(result.error().error));
        REQUIRE(result);
    }

    /** Materialize one fixed batch.
     * @param inputs Actual controlled source tuples.
     */
    void send(std::initializer_list<Input> inputs) {
        send(std::span<const Input>(inputs.begin(), inputs.size()));
    }

    /** Emit actual projected delivery prefix and exact count.
     * @param phase Production retrieval phase.
     * @param view View ordinal.
     * @param maximum Actual finite acknowledgment bound, or large bounded drain.
     * @param limited Emit provider controls for a finite observed call.
     * @return Actual detached tuples for selecting a subsequent source update.
     */
    std::vector<TraceRecord> take(std::string_view phase, std::size_t view, std::size_t maximum = 1024,
                                  bool limited = false) {
        std::vector<TraceRecord> result;
        auto work = turn();

        while (result.size() < maximum) {
            auto offered = f.history.front(views[view], work);
            REQUIRE(offered);

            if (!*offered) {
                break;
            }

            const auto &r = (**offered).record;
            const auto i = static_cast<std::int32_t>(result.size());
            row(phase, i, "record", units(f.schema.records()[static_cast<std::size_t>(r.localId)].name));
            row(phase, i, "symbol", units(r.symbol));
            integer(phase, i, "raw-time", r.rawTime);
            integer(phase, i, "value", r.fields[2].raw);
            integer(phase, i, "flags", r.eventFlags);
            result.push_back({r.localId, std::u16string(r.symbol), r.rawTime,
                              static_cast<std::int32_t>(r.fields[2].raw), r.eventFlags});
            REQUIRE(f.history.pop(views[view], work));
        }

        integer(phase, -1, "record-count", static_cast<std::int64_t>(result.size()));

        if (limited) {
            auto next = f.history.front(views[view], work);
            REQUIRE(next);
            const auto control = std::string(phase) + "-control";
            integer(control, 0, "sink-capacity-records", static_cast<std::int64_t>(maximum));
            row(control, 0, "provider-more", *next ? "b:1" : "b:0");
        }

        return result;
    }
};
} // namespace

TEST_CASE("HISTORY emits actual decoder and collector traces matching locked production observations") {
    const auto golden = observations();
    const auto directory = std::filesystem::path(ORG_TTLDTOR_QDCXX_HISTORY_CAPTURE_DIR);
    std::filesystem::create_directories(directory);
    std::ofstream output(directory / "observations.tsv", std::ios::binary);
    REQUIRE(output);
    output << "case\tphase\tindex\tkey\tvalue\n";
    {
        Trace t("history-duplicate-two-clients", false, output, golden);
        auto one = t.agent();
        auto two = t.agent();
        t.sub(one, false, 0, u"ALPHA", 2);
        t.sub(one, false, 0, u"ALPHA", 2);
        t.sub(two, false, 0, u"ALPHA", 2);
        t.send({{0, u"ALPHA", 3, 11, 0}});
        t.take("first-client-data", one);
        t.take("second-client-data", two);
        t.sub(one, true);
        t.sub(one, true);
        t.send({{0, u"ALPHA", 4, 12, 0}});
        t.take("removed-client-data", one);
        t.take("remaining-client-data", two);
        t.sub(two, true);
    }
    {
        Trace t("history-raw-bound-change-aggregate-inclusive", true, output, golden);
        const auto lower = (std::int64_t{1} << 32) + 2;
        auto one = t.agent();
        auto two = t.agent();
        t.sub(one, false, 0, u"ALPHA", lower);
        t.sub(two, false, 0, u"ALPHA", lower - 1);
        t.send({{0, u"ALPHA", lower + 1, 31, 4}, {0, u"ALPHA", lower, 30, 0}, {0, u"ALPHA", lower - 1, 29, 8}});
        t.take("inclusive-first", one);
        t.take("inclusive-second", two);
        t.sub(two, false, 0, u"ALPHA", lower + 1);
        t.take("changed-bound-data", two);
        t.sub(one, true);
        t.sub(two, false, 0, u"ALPHA", -1);
        t.take("negative-bound-data", two);
    }

    for (const auto kind :
         {std::string("empty"), std::string("complete"), std::string("snip"), std::string("restart-overlap")}) {
        Trace t("history-snapshot-" + kind + "-agent-true", false, output, golden);
        auto one = t.agent();
        t.sub(one, false);

        if (kind == "empty") {
            t.send({{0, u"ALPHA", 0, 0, 14}});
            t.take("empty-snapshot", one);
            continue;
        }

        t.send({{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
        t.take("baseline-complete", one);

        if (kind == "snip") {
            t.send({{0, u"ALPHA", 5, 50, 4}, {0, u"ALPHA", 2, 22, 16}});
            t.take("snipped", one);
        } else if (kind == "restart-overlap") {
            t.send({{0, u"ALPHA", 4, 40, 4}});
            t.take("incomplete-first-begin", one);
            t.send({{0, u"ALPHA", 3, 33, 4}});
            t.take("restarted-begin", one);
            t.send({{0, u"ALPHA", 1, 11, 0}, {0, u"ALPHA", 0, 0, 10}});
            t.take("restart-completed", one);
            auto late = t.agent();
            t.sub(late, false);
            t.take("late-current-snapshot", late);
        } else {
            t.send({{0, u"ALPHA", 2, 21, 0}});
            t.take("live-update", one);
            t.send({{0, u"ALPHA", 2, 0, 2}});
            t.take("remove-existing", one);
            t.send({{0, u"ALPHA", 2, 0, 2}});
            t.take("remove-absent", one);
        }
    }

    {
        Trace t("history-snapshot-complete-agent-false", false, output, golden);
        auto one = t.agent(false);
        t.sub(one, false);
        t.send({{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
        t.take("baseline-complete", one);
        t.send({{0, u"ALPHA", 2, 21, 0}});
        t.take("live-update", one);
        t.send({{0, u"ALPHA", 2, 0, 2}});
        t.take("remove-existing", one);
        t.send({{0, u"ALPHA", 2, 0, 2}});
        t.take("remove-absent", one);
    }

    for (const bool batch : {false, true}) {
        Trace t(batch ? "history-transaction-single-batch" : "history-transaction-separate-batches", false, output,
                golden);
        auto one = t.agent();
        t.sub(one, false);
        t.send({{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
        t.take("baseline", one);

        if (batch) {
            t.send({{0, u"ALPHA", 3, 31, 1}, {0, u"ALPHA", 2, 21, 1}, {0, u"ALPHA", 1, 11, 0}});
            t.take("transaction-batch", one);
        } else {
            t.send({{0, u"ALPHA", 3, 31, 1}});
            t.take("pending-first", one);
            t.send({{0, u"ALPHA", 2, 21, 1}});
            t.take("pending-second", one);
            t.send({{0, u"ALPHA", 1, 11, 0}});
            t.take("transaction-end", one);
        }
    }

    {
        Trace t("history-transaction-record-key-isolation", false, output, golden);
        auto one = t.agent();
        t.sub(one, false);
        t.sub(one, false, 0, u"BETA");
        t.sub(one, false, 1);
        t.send({{0, u"ALPHA", 3, 30, 5}});
        t.take("record-A-pending", one);
        t.send({{0, u"BETA", 0, 0, 14}});
        t.take("other-key-complete", one);
        t.send({{1, u"ALPHA", 0, 0, 14}});
        t.take("other-record-complete", one);
        t.send({{0, u"ALPHA", 0, 0, 10}});
        t.take("record-A-complete", one);
    }
    {
        Trace t("history-interrupted-transaction-unsubscribe", false, output, golden);
        auto one = t.agent();
        auto keeper = t.agent();
        t.sub(one, false);
        t.sub(keeper, false);
        t.send({{0, u"ALPHA", 3, 30, 5}});
        t.take("pending-before-withdraw", one);
        t.sub(one, true);
        t.take("withdrawn-agent", one);
        t.sub(one, false);
        t.take("readded-pending-view", one);
        t.send({{0, u"ALPHA", 0, 0, 10}});
        t.take("readded-after-end", one);
        t.take("keeper-after-end", keeper);
    }
    {
        Trace t("history-partial-initial-live-quantum", false, output, golden);
        auto one = t.agent();
        t.sub(one, false);
        std::vector<Input> initial;

        for (std::int32_t n = 104; n != 0; --n) {
            initial.push_back({0, u"ALPHA", n, n + 10, n == 104 ? 4 : 0});
        }

        initial.push_back({0, u"ALPHA", 0, 0, 10});
        t.send(initial);
        auto consumed = t.take("initial-first", one, 1, true);
        REQUIRE(consumed.size() == 1);
        auto original = consumed[0];
        t.row("update-selection", 0, "record",
              units(t.f.schema.records()[static_cast<std::size_t>(original.record)].name));
        t.row("update-selection", 0, "symbol", units(original.symbol));
        t.integer("update-selection", 0, "original-time", original.time);
        t.integer("update-selection", 0, "original-value", original.value);
        t.integer("initial-first-control", 0, "cumulative-delivered", 1);
        t.send({{original.record, original.symbol, original.time, original.value + 1000, 0}});
        auto next = t.take("initial-next-99", one, 99, true);
        auto count = 1 + next.size();
        t.integer("initial-next-99-control", 0, "cumulative-delivered", static_cast<std::int64_t>(count));
        count += t.take("first-after-quantum", one, 1, true).size();
        t.integer("first-after-quantum-control", 0, "cumulative-delivered", static_cast<std::int64_t>(count));
        t.take("remainder", one);
        t.take("after-drain", one);
    }
    {
        Trace t("history-signed-inner-index-conflation", false, output, golden);
        const auto high = (std::int64_t{2} << 32) | 7;
        const auto low = (std::int64_t{1} << 32) | 7;
        auto one = t.agent();
        t.sub(one, false, 0, u"ALPHA", std::numeric_limits<std::int64_t>::min());
        t.send({{0, u"ALPHA", high, 20, 4},
                {0, u"ALPHA", low, 10, 0},
                {0, u"ALPHA", -1, -10, 0},
                {0, u"ALPHA", std::numeric_limits<std::int64_t>::min(), 0, 10}});
        t.take("baseline", one);
        t.send({{0, u"ALPHA", low, 10, 0}});
        t.send({{0, u"ALPHA", low, 10, 0}});
        t.take("identical-twice", one);
        t.send({{0, u"ALPHA", low, 11, 1}, {0, u"ALPHA", low, 12, 0}});
        t.take("same-index-conflated", one);
        t.send({{0, u"ALPHA", high, 21, 1}});
        t.take("pending-higher-index", one);
        t.send({{0, u"ALPHA", -1, -11, 0}});
        t.take("transaction-end-negative-index", one);
        t.send({{0, u"ALPHA", low, 0, 2}});
        t.send({{0, u"ALPHA", low, 0, 2}});
        t.take("remove-twice", one);
        t.send({{0, u"ALPHA", low, 13, 0}});
        t.take("readded-index", one);
    }

    for (const bool store : {false, true}) {
        Trace t(store ? "history-retention-true" : "history-retention-false", store, output, golden);
        t.send({{0, u"ALPHA", 2, 20, 4}, {0, u"ALPHA", 0, 0, 10}});
        auto one = t.agent();
        t.sub(one, false);
        t.take("preadd-view", one);
        t.send({{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
        t.take("active", one);
        t.sub(one, true);
        t.sub(one, false);
        t.take("readd", one);
        t.send({{0, u"ALPHA", 3, 31, 0}});
        t.take("post-readd-live", one);
        t.send({{0, u"ALPHA", 4, 40, 4}, {0, u"ALPHA", 0, 0, 10}});
        t.take("fresh-begin", one);
    }

    output.flush();
    REQUIRE(output.good());
}

TEST_CASE("HISTORY shared stale transition advances detached as-of evidence and preserves input") {
    auto bounds = limits();
    bounds.cacheFrames = 2;
    auto f = fixture(false, bounds);
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    send(f, {{0, u"ALPHA", 3, 31, 0}});
    (void)drain(f, view);
    REQUIRE(f.history.frameCount() == 2);
    auto work = turn();
    const c::SubscriptionKey key{f.schema.recordContext(0), u"ALPHA"};
    auto before = f.history.rangeStatus(view, key, work);
    REQUIRE(before);
    REQUIRE(before->completeRequestedRange);
    const Input input[]{{0, u"ALPHA", 2, 21, 0}};
    auto event = admit(f.live, data(input));
    const auto retainedBytes = event.residentBytes();
    work = turn();
    auto result = f.history.stage(std::move(event), work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == Error::RESOURCE_LIMIT);
    CHECK(event.residentBytes() == retainedBytes);
    CHECK(f.history.state() == s::HistoryState::STALE);
    work = turn();
    auto after = f.history.rangeStatus(view, key, work);
    REQUIRE(after);
    CHECK_FALSE(after->completeRequestedRange);
    CHECK(after->observationSerial > before->observationSerial);
    CHECK(after->epoch.matches(before->epoch));
    CHECK(f.history.desired(view)->isCurrent(before->membership));
    CHECK(f.history.desired(view)->isCurrent(after->membership));
}

TEST_CASE("HISTORY empty-state forgetting preflights other-view pending traversal at scale") {
    auto bounds = limits();
    bounds.partitions = 1001;
    bounds.points = 2;
    bounds.viewSlots = 2;
    bounds.pendingItems = 1000;
    bounds.pendingFrames = 2;
    bounds.cacheFrames = 3;
    bounds.cacheBytes = 8 * 1024 * 1024;
    bounds.pendingBytes = 8 * 1024 * 1024;
    bounds.residentBytes = 64 * 1024 * 1024;
    bounds.viewSubscriptions.entries = 1;
    bounds.viewSubscriptions.peakEntries = 2;
    auto local = schema();
    auto admission = liveLimits();
    admission.frame = {65536, 65545};
    admission.data.payloadBytes = 65536;
    admission.data.processedRecords = 1024;
    admission.data.emittedRecords = 1024;
    admission.data.emittedValues = 4096;
    admission.data.wireFields = 4096;
    admission.data.symbol.inputBytes = 65536;
    admission.eventBytes = 2 * 1024 * 1024;
    admission.residentBytes = 32 * 1024 * 1024;
    auto live = session(local, admission);
    auto copy = local;
    auto work = turn();
    auto collector = s::HistoryCollector::create(std::move(copy), bounds, {false, 100, 10000}, work);
    REQUIRE(collector);
    Fixture f{std::move(local), std::move(live), std::move(*collector)};
    auto healthy = open(f);
    auto churn = open(f);
    add(f, healthy);

    for (std::size_t n = 0; n < 1000; ++n) {
        const auto digits = std::to_string(n);
        std::u16string key = u"K";

        for (const auto digit : digits) {
            key.push_back(static_cast<char16_t>(digit));
        }

        add(f, churn, 0, 0, key);
        const c::SubscriptionKey removed{f.schema.recordContext(0), key};
        auto coreWork = command();
        work = turn();
        REQUIRE(f.history.remove(churn, std::span(&removed, 1), coreWork, work));
    }

    REQUIRE(f.history.partitionCount() == 1001);
    send(f, {{0, u"ALPHA", 2, 20, 4}, {0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, healthy);
    std::vector<Input> updates;

    for (std::int32_t n = 0; n < 1000; ++n) {
        updates.push_back({0, u"ALPHA", (n % 2) + 1, 100 + n, 0});
    }

    auto event = admit(f.live, data(std::span<const Input>(updates)));
    work = turn();
    auto staged = f.history.stage(std::move(event), work);
    REQUIRE(staged);
    REQUIRE(staged->failedViews == 0);
    REQUIRE(f.history.pendingCount(healthy) == 1000);
    work = turn();
    work.visits = 200000; // Less than1000empty-key ×1000other-viewpending examinations.
    auto result = f.history.forgetUnobserved(work);
    CHECK_FALSE(result);

    if (!result) {
        CHECK(result.error().error == Error::WORK_LIMIT);
    }

    CHECK(f.history.partitionCount() == 1001);
    CHECK(f.history.pendingCount(healthy) == 1000);
}

/** Test-only ordinary allocation hook; never active while the framework reports results.
 * @param size Requested ordinary bytes.
 * @return Aligned malloc backing, or throw bad_alloc.
 */
void *operator new(std::size_t size) {
    if (failAllocations) {
        if (!allocationsRemaining) {
            throw std::bad_alloc();
        }

        --allocationsRemaining;
    }

    if (auto *value = std::malloc(size ? size : 1)) {
        return value;
    }

    throw std::bad_alloc();
}

/** Delegate ordinary array allocation to the same narrow hook.
 * @param size Requested bytes.
 * @return Owned backing, or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release ordinary test backing.
 * @param value Original pointer, or null.
 */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** Release ordinary array backing.
 * @param value Original pointer.
 */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** Release sized ordinary backing.
 * @param value Original pointer.
 * @param size Compiler-supplied size, intentionally unused.
 */
void operator delete(void *value, std::size_t size) noexcept {
    (void)size;
    std::free(value);
}

/** Release sized array backing.
 * @param value Original pointer.
 * @param size Compiler-supplied size, intentionally unused.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    (void)size;
    std::free(value);
}

TEST_CASE("HISTORY factory rejects insufficient fixed resident backing before any allocation") {
    auto local = schema();
    const auto identity = local.recordContext(0);
    s::HistoryLimits bounds{};
    bounds.partitions = 100;
    bounds.residentBytes = 1;
    auto work = turn();
    auto result = [&]() {
        AllocationFault fault(0);

        return s::HistoryCollector::create(std::move(local), bounds, {false, 100, 10000}, work);
    }();
    REQUIRE_FALSE(result);
    CHECK(result.error().error == Error::RESOURCE_LIMIT);
    CHECK(local.recordContext(0).matches(identity));
    CHECK(local.records().size() == 2);
}

TEST_CASE("HISTORY refuses invalid snapshot retransmission IDs without consuming the schema") {
    const auto refuse = [](std::span<const std::int32_t> records) {
        auto local = schema();
        const auto identity = local.recordContext(0);
        auto work = turn();
        const auto result = s::HistoryCollector::create(
            std::move(local), limits(), {false, 100, 10000, c::SubscriptionSelectorPolicy::EXACT_ONLY, records}, work);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == Error::INVALID_CONFIGURATION);
        CHECK(local.recordContext(0).matches(identity));
    };
    constexpr std::array<std::int32_t, 1> negative{-1};
    constexpr std::array<std::int32_t, 1> absent{2};
    constexpr std::array<std::int32_t, 2> duplicate{0, 0};
    refuse(negative);
    refuse(absent);
    refuse(duplicate);
}

TEST_CASE("HISTORY certifies only the covered clean requested range while a lower view remains incomplete") {
    auto f = fixture();
    auto low = open(f);
    auto high = open(f);
    add(f, low, 0);
    add(f, high, 2);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}});
    auto covered = drain(f, high);
    REQUIRE(covered.size() == 2);
    CHECK(covered.back().time == 2);
    CHECK(covered.back().flags == 8);
    auto work = turn();
    const c::SubscriptionKey key{f.schema.recordContext(0), u"ALPHA"};
    auto highStatus = f.history.rangeStatus(high, key, work);
    auto lowStatus = f.history.rangeStatus(low, key, work);
    REQUIRE(highStatus);
    REQUIRE(lowStatus);
    CHECK(highStatus->snapshot == s::HistorySnapshotState::RECEIVING);
    CHECK(highStatus->completeRequestedRange);
    CHECK_FALSE(lowStatus->completeRequestedRange);
    (void)drain(f, low);
    work = turn();
    lowStatus = f.history.rangeStatus(low, key, work);
    REQUIRE(lowStatus);
    CHECK_FALSE(lowStatus->completeRequestedRange);
    send(f, {{0, u"ALPHA", 1, 11, 16}});
    (void)drain(f, low);
    work = turn();
    highStatus = f.history.rangeStatus(high, key, work);
    lowStatus = f.history.rangeStatus(low, key, work);
    REQUIRE(highStatus);
    REQUIRE(lowStatus);
    CHECK(highStatus->completeRequestedRange);
    CHECK(lowStatus->snapshot == s::HistorySnapshotState::TRUNCATED);
    CHECK_FALSE(lowStatus->completeRequestedRange);
    send(f, {{0, u"ALPHA", 3, 31, 1}});
    work = turn();
    highStatus = f.history.rangeStatus(high, key, work);
    REQUIRE(highStatus);
    CHECK(highStatus->sourceTransactionPending);
    CHECK_FALSE(highStatus->completeRequestedRange);
}

namespace {
/** Remove one exact key through the authoritative local owner.
 * @param f Graph.
 * @param view Current view.
 * @param symbol Exact text.
 * @param record Local record ID.
 */
void removeKey(Fixture &f, const s::HistoryViewToken &view, std::u16string_view symbol = u"ALPHA",
               std::int32_t record = 0) {
    auto work = turn();
    auto coreWork = command();
    const c::SubscriptionKey key{f.schema.recordContext(record), symbol};
    REQUIRE(f.history.remove(view, std::span(&key, 1), coreWork, work));
}

/** Query detached as-of range evidence.
 * @param f Graph.
 * @param view Current active/failed selection.
 * @param symbol Exact key.
 * @return Scoped copied evidence.
 */
s::HistoryRangeStatus range(Fixture &f, const s::HistoryViewToken &view, std::u16string_view symbol = u"ALPHA") {
    auto work = turn();
    auto result = f.history.rangeStatus(view, {f.schema.recordContext(0), symbol}, work);
    REQUIRE(result);

    return std::move(*result);
}
} // namespace

TEST_CASE("HISTORY work and rejected or no-op mutations preserve the exact prepared offer") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    auto work = turn();
    auto offer = f.history.front(view, work);
    REQUIRE(offer);
    REQUIRE(*offer);
    const auto *fields = (**offer).record.fields.data();
    work = turn();
    work.visits = 0;
    auto retry = f.history.front(view, work);
    REQUIRE_FALSE(retry);
    CHECK(retry.error().error == Error::WORK_LIMIT);
    CHECK(fields[2].raw == 30);
    auto coreWork = command();
    work = turn();
    const c::SubscriptionRequest same{{f.schema.recordContext(0), u"ALPHA"}, 0};
    auto unchanged = f.history.add(view, std::span(&same, 1), coreWork, work);
    REQUIRE(unchanged);
    CHECK_FALSE(unchanged->changed);
    const c::SubscriptionRequest contradictory[]{{same.key, 0}, {same.key, 1}};
    coreWork = command();
    work = turn();
    auto rejected = f.history.add(view, contradictory, coreWork, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().subscriptionError == c::SubscriptionError::CONTRADICTORY_INPUT);
    CHECK(fields[2].raw == 30);
    work = turn();
    work.visits = 0;
    auto ack = f.history.pop(view, work);
    REQUIRE_FALSE(ack);
    CHECK(ack.error().error == Error::WORK_LIMIT);
    CHECK(fields[2].raw == 30);
    work = turn();
    REQUIRE(f.history.pop(view, work));
    CHECK(drain(f, view).size() == 2);
}

TEST_CASE("HISTORY failed candidate source can die without retaining dangling spans or changing the offer") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    auto work = turn();
    auto offered = f.history.front(view, work);
    REQUIRE(offered);
    REQUIRE(*offered);
    const auto old = (**offered).record.fields[2].raw;
    {
        const Input input[]{{0, u"ALPHA", 2, 20, 0}};
        auto event = admit(f.live, data(input));
        const auto bytes = event.residentBytes();
        work = turn();
        work.bytes = 0;
        auto rejected = f.history.stage(std::move(event), work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == Error::WORK_LIMIT);
        CHECK(event.residentBytes() == bytes);
    }
    CHECK((**offered).record.fields[2].raw == old);
    work = turn();
    REQUIRE(f.history.pop(view, work));
    CHECK(f.history.state() == s::HistoryState::CURRENT);
    CHECK(f.history.pointCount() == 1);
}

TEST_CASE("HISTORY failed local FIFO retires only its reserved view after whole shared admission") {
    auto bounds = limits();
    bounds.pendingItems = 1;
    auto f = fixture(false, bounds);
    auto healthy = open(f);
    auto slow = open(f);
    add(f, healthy);
    add(f, slow);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, healthy);
    (void)drain(f, slow);
    send(f, {{0, u"ALPHA", 3, 31, 0}});
    (void)drain(f, healthy);
    REQUIRE(f.history.pendingCount(slow) == 1);
    const auto result = send(f, {{0, u"ALPHA", 2, 21, 0}});
    CHECK(result.failedViews == 1);
    CHECK(f.history.viewState(slow) == s::HistoryViewState::FAILED);
    CHECK(f.history.viewState(healthy) == s::HistoryViewState::ACTIVE);
    CHECK(f.history.state() == s::HistoryState::CURRENT);
    const auto failure = f.history.viewFailure(slow);
    REQUIRE(failure);
    CHECK(failure->pendingItemsDiscarded == 1);
    CHECK(failure->unknownFollowingLoss);
    REQUIRE(f.history.desired(slow));
    CHECK(f.history.desired(slow)->entries().size() == 1);
    CHECK_FALSE(range(f, slow).completeRequestedRange);
    auto delivered = drain(f, healthy);
    REQUIRE(delivered.size() == 1);
    CHECK(delivered[0].value == 21);
    CHECK(range(f, healthy).completeRequestedRange);
    auto work = turn();
    REQUIRE(f.history.closeView(slow, work));
    CHECK(f.history.viewState(slow) == s::HistoryViewState::INVALID);
}

TEST_CASE("HISTORY local owner peak includes old plus incoming same-index replacement") {
    auto bounds = limits();
    bounds.pendingFrames = 1;
    bounds.pendingItems = 8;
    auto f = fixture(false, bounds);
    auto healthy = open(f);
    auto slow = open(f);
    add(f, healthy);
    add(f, slow);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, healthy);
    (void)drain(f, slow);
    send(f, {{0, u"ALPHA", 3, 31, 0}});
    (void)drain(f, healthy);
    const auto result = send(f, {{0, u"ALPHA", 3, 32, 0}});
    CHECK(result.failedViews == 1);
    CHECK(f.history.viewState(slow) == s::HistoryViewState::FAILED);
    auto values = drain(f, healthy);
    REQUIRE(values.size() == 1);
    CHECK(values[0].value == 32);
    CHECK(f.history.state() == s::HistoryState::CURRENT);
}

TEST_CASE("HISTORY shared rejection does not publish an earlier candidate local failure") {
    auto bounds = limits();
    bounds.pendingItems = 1;
    bounds.points = 2;
    auto f = fixture(false, bounds);
    auto healthy = open(f);
    auto slow = open(f);
    add(f, healthy);
    add(f, slow);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, healthy);
    (void)drain(f, slow);
    send(f, {{0, u"ALPHA", 3, 31, 0}});
    (void)drain(f, healthy);
    const Input inputs[]{{0, u"ALPHA", 2, 21, 0}, {0, u"ALPHA", 1, 11, 0}};
    auto event = admit(f.live, data(inputs));
    const auto bytes = event.residentBytes();
    auto work = turn();
    auto result = f.history.stage(std::move(event), work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == Error::RESOURCE_LIMIT);
    CHECK(event.residentBytes() == bytes);
    CHECK(f.history.state() == s::HistoryState::STALE);
    CHECK(f.history.viewState(slow) == s::HistoryViewState::ACTIVE);
    CHECK_FALSE(f.history.viewFailure(slow));
    CHECK(f.history.pendingCount(slow) == 1);
    CHECK(f.history.pendingCount(healthy) == 0);
    CHECK(f.history.pointCount() == 2);
}

TEST_CASE("HISTORY explicit empty-metadata forgetting permits sustained churn without a healthy reset") {
    auto bounds = limits();
    bounds.partitions = 2;
    auto f = fixture(false, bounds);
    auto healthy = open(f);
    auto churn = open(f);
    add(f, healthy);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, healthy);
    const auto epoch = range(f, healthy).epoch;
    std::uint64_t generation = 0, activation = 0;

    for (std::int32_t n = 0; n < 12; ++n) {
        std::u16string symbol = u"K";
        symbol.push_back(static_cast<char16_t>(u'A' + n));
        add(f, churn, 0, 0, symbol);
        const auto current = range(f, churn, symbol);
        CHECK(current.partitionActivation > activation);
        activation = current.partitionActivation;
        send(f, {{0, symbol, 0, 0, 14}});
        (void)drain(f, churn);
        removeKey(f, churn, symbol);
        CHECK(f.history.partitionCount() == 2);
        auto work = turn();
        auto forgotten = f.history.forgetUnobserved(work);
        REQUIRE(forgotten);
        CHECK(forgotten->forgottenPartitions == 1);
        CHECK(forgotten->continuityForgotten);
        CHECK(forgotten->generation > generation);
        generation = forgotten->generation;
        CHECK(f.history.partitionCount() == 1);
        const auto status = range(f, healthy);
        CHECK(status.epoch.matches(epoch));
        CHECK(status.completeRequestedRange);
        CHECK_FALSE(f.history.resetNotice());
        send(f, {{0, u"ALPHA", 3, 100 + n, 0}});
        (void)drain(f, healthy);
        CHECK(f.history.pointCount() == 2);
    }
}

TEST_CASE("HISTORY empty-only forgetting cannot evict store-everything values") {
    auto f = fixture(true);
    send(f, {{0, u"ALPHA", 2, 20, 4}, {0, u"ALPHA", 0, 0, 10}});
    const auto points = f.history.pointCount();
    const auto frames = f.history.frameCount();
    auto work = turn();
    auto result = f.history.forgetUnobserved(work);
    REQUIRE(result);
    CHECK(result->forgottenPartitions == 0);
    CHECK_FALSE(result->continuityForgotten);
    CHECK(f.history.pointCount() == points);
    CHECK(f.history.frameCount() == frames);
    auto view = open(f);
    add(f, view);
    auto values = drain(f, view);
    REQUIRE(values.size() == 2);
    CHECK(values[0].value == 20);
}

TEST_CASE("HISTORY epoch pin survives drain and reset interval coalesces without claiming lossless completion") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 5}});
    (void)drain(f, view);
    const auto old = range(f, view);
    auto foreign = session(f.schema);
    const Input values[]{{0, u"ALPHA", 0, 0, 14}};
    auto event = admit(foreign, data(values));
    const auto bytes = event.residentBytes();
    auto work = turn();
    auto rejected = f.history.stage(std::move(event), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == Error::INVALID_EPOCH);
    CHECK(event.residentBytes() == bytes);
    work = turn();
    auto first = f.history.reset(work);
    REQUIRE(first);
    CHECK(first->unknownLoss);
    work = turn();
    auto second = f.history.reset(work);
    REQUIRE(second);
    CHECK(second->skippedResets == 1);
    CHECK(second->from.matches(first->from));
    CHECK(second->from.matches(old.epoch));
    CHECK_FALSE(f.history.acknowledgeReset(first->sequence));
    CHECK(f.history.state() == s::HistoryState::RESET_PENDING);
    REQUIRE(f.history.acknowledgeReset(second->sequence));
    work = turn();
    REQUIRE(f.history.stage(std::move(event), work));
    CHECK(event.residentBytes() == 0);
    auto fresh = drain(f, view);
    REQUIRE(fresh.size() == 1);
    CHECK(fresh[0].flags == 14);
    CHECK_FALSE(range(f, view).epoch.matches(old.epoch));
}

TEST_CASE("HISTORY opaque views reject foreign and reused origins and moves invalidate prepared offers") {
    auto f = fixture();
    auto other = fixture();
    auto one = open(f);
    auto foreign = open(other);
    auto work = turn();
    auto rejected = f.history.front(foreign, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == Error::INVALID_VIEW);
    add(f, one);
    send(f, {{0, u"ALPHA", 0, 0, 14}});
    work = turn();
    auto offer = f.history.front(one, work);
    REQUIRE(offer);
    REQUIRE(*offer);
    auto moved = std::move(f.history);
    work = turn();
    auto stale = moved.pop(one, work);
    REQUIRE_FALSE(stale);
    CHECK(stale.error().error == Error::NO_OFFER);
    CHECK(f.history.schema() == nullptr);
    work = turn();
    REQUIRE(moved.closeView(one, work));
    work = turn();
    auto reused = moved.openView({true}, work);
    REQUIRE(reused);
    CHECK_FALSE(reused->matches(one));
    CHECK(moved.viewState(one) == s::HistoryViewState::INVALID);
    CHECK(moved.viewState(*reused) == s::HistoryViewState::ACTIVE);
}

TEST_CASE("HISTORY MAX-index policy is source-backed native validation rather than runtime oracle parity") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    const Input input[]{{0, u"ALPHA", std::numeric_limits<std::int64_t>::max(), 1, 0}};
    auto event = admit(f.live, data(input));
    auto work = turn();
    auto result = f.history.stage(std::move(event), work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == Error::INVALID_INDEX);
    CHECK(event.residentBytes() != 0);
    CHECK(f.history.state() == s::HistoryState::STALE);
    work = turn();
    auto reset = f.history.reset(work);
    REQUIRE(reset);
    REQUIRE(f.history.acknowledgeReset(reset->sequence));
    send(f, {{0, u"ALPHA", std::numeric_limits<std::int64_t>::max(), 0, 3}});
    CHECK(range(f, view).sourceTransactionPending);
    send(f, {{0, u"ALPHA", std::numeric_limits<std::int64_t>::max(), 0, 2}});
    auto boundary = drain(f, view);
    REQUIRE(boundary.size() == 1);
    CHECK(boundary[0].time == std::numeric_limits<std::int64_t>::max());
    CHECK(boundary[0].flags == 2);
    CHECK(f.history.pointCount() == 0);
}

TEST_CASE("HISTORY factory and steady stage front pop obey allocation ownership boundaries") {
    auto local = schema();
    const auto context = local.recordContext(0);
    bool succeeded = false;

    for (std::size_t allowed = 0; allowed < 40 && !succeeded; ++allowed) {
        auto work = turn();
        auto created = [&]() {
            AllocationFault fault(allowed);

            return s::HistoryCollector::create(std::move(local), limits(), {false, 100, 10000}, work);
        }();

        if (created) {
            succeeded = true;
            CHECK(created->schema()->recordContext(0).matches(context));
        } else {
            CHECK(created.error().error == Error::ALLOCATION_FAILURE);
            CHECK(local.recordContext(0).matches(context));
        }
    }

    REQUIRE(succeeded);
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    const Input inputs[]{{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}};
    auto event = admit(f.live, data(inputs));
    auto work = turn();
    auto staged = [&]() {
        AllocationFault fault(0);

        return f.history.stage(std::move(event), work);
    }();
    REQUIRE(staged);
    work = turn();
    auto offer = [&]() {
        AllocationFault fault(0);

        return f.history.front(view, work);
    }();
    REQUIRE(offer);
    REQUIRE(*offer);
    work = turn();
    auto consumed = [&]() {
        AllocationFault fault(0);

        return f.history.pop(view, work);
    }();
    REQUIRE(consumed);
}

TEST_CASE("HISTORY default raw fields and synthetic provenance are distinct from actual admitted sources") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    send(f, {{0, u"ALPHA", 5, 50, 4}, {0, u"ALPHA", 2, 22, 16}});
    auto first = drain(f, view, 1);
    REQUIRE(first.size() == 1);
    CHECK(first[0].time == 5);
    auto work = turn();
    auto removal = f.history.front(view, work);
    REQUIRE(removal);
    REQUIRE(*removal);
    CHECK((**removal).record.rawTime == 3);
    CHECK((**removal).record.fields[2].raw == 0);
    CHECK_FALSE((**removal).sourceRecord);
    REQUIRE((**removal).trigger);
    CHECK((**removal).trigger->rawTime == 2);
    CHECK((**removal).trigger->flags == 16);
    const auto *prior = (**removal).record.fields.data();
    work = turn();
    work.visits = 0;
    CHECK_FALSE(f.history.front(view, work));
    CHECK(prior[2].raw == 0);
    work = turn();
    REQUIRE(f.history.pop(view, work));
    auto last = drain(f, view);
    REQUIRE(last.size() == 1);
    CHECK(last[0].flags == 16);
}

namespace {
/** Create a real source for arbitrary complete validated local fields.
 * @param local Immutable caller schema.
 * @return Ready source; caller separately supplies exact descriptions.
 */
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
std::vector<c::FieldValue> copyProjected(std::span<const s::HistoryFieldValueView> fields) {
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

TEST_CASE("HISTORY rejects complete ordinary standard Quote and Profile schemas without inventing indexing") {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};

    for (const bool profile : {false, true}) {
        auto defaults = profile ? q::defaultProfileSchema(schemaBounds) : q::defaultQuoteSchema(schemaBounds);
        REQUIRE(defaults);
        REQUIRE(defaults->records()[0].fields.size() == (profile ? 16 : 8));
        auto local = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        auto copy = *local;
        auto work = turn();
        auto collector = s::HistoryCollector::create(std::move(copy), limits(), {false, 100, 10000}, work);
        REQUIRE(collector);
        auto view = collector->openView({true}, work);
        REQUIRE(view);
        const c::SubscriptionRequest request{{local->recordContext(0), u"IBM"}, 0};
        auto coreWork = command();
        auto result = collector->add(*view, std::span(&request, 1), coreWork, work);
        REQUIRE_FALSE(result);
        CHECK(result.error().subscriptionError == c::SubscriptionError::INVALID_HISTORY);
        CHECK(collector->desired(*view)->entries().empty());
        CHECK(collector->partitionCount() == 0);
        const c::SubscriptionRequest wildcard{{local->recordContext(0), u"*"}, 0};
        coreWork = command();
        work = turn();
        CHECK_FALSE(collector->add(*view, std::span(&wildcard, 1), coreWork, work));
    }

    auto f = fixture();
    auto view = open(f);
    const c::SubscriptionRequest wildcard{{f.schema.recordContext(0), u"*"}, 0};
    auto coreWork = command();
    auto work = turn();
    auto result = f.history.add(view, std::span(&wildcard, 1), coreWork, work);
    REQUIRE_FALSE(result);
    CHECK(result.error().subscriptionError == c::SubscriptionError::INVALID_SELECTOR);
}

TEST_CASE(
    "HISTORY preserves complete standard field bodies in explicitly caller-indexed schemas and prepared mappings") {
    const c::SchemaLimits schemaBounds{16, 128, 256, 128, 16384};

    for (const bool profile : {false, true}) {
        auto defaults = profile ? q::defaultProfileSchema(schemaBounds) : q::defaultQuoteSchema(schemaBounds);
        REQUIRE(defaults);
        const auto count = profile ? std::size_t{16} : std::size_t{8};
        REQUIRE(defaults->records()[0].fields.size() == count);
        auto ordinary = c::LocalSchema::create(defaults->records(), schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(ordinary);
        std::vector<std::byte> oldDescription, fieldBody;
        std::vector<c::FieldValue> expected;
        {
            auto source = genericSession(*ordinary);
            const auto relative = profile ? "tests/fixtures/encoding/field-types/expected/profile-stream.qtp"
                                          : "tests/fixtures/protocol/quote/expected/time-1.qtp";
            std::ifstream input(std::filesystem::path(ORG_TTLDTOR_QDCXX_HISTORY_SOURCE_ROOT) / relative,
                                std::ios::binary);
            REQUIRE(input);
            const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
            const auto capture = std::as_bytes(std::span(bytes));
            p::FrameReader reader(liveLimits().frame);
            std::size_t offset = 0;

            while (offset < capture.size()) {
                auto decoded = reader.feed(capture.subspan(offset));
                REQUIRE(decoded.frame);
                offset += decoded.accepted;

                if (decoded.frame->type == 2) {
                    oldDescription = decoded.frame->payload;
                    (void)admit(source, frame(2, oldDescription));
                } else if (decoded.frame->type == 15) {
                    fieldBody = decoded.frame->payload;
                    auto event = admit(source, frame(15, fieldBody));
                    REQUIRE(event.records().size() == 1);
                    expected = copyFields(event.records()[0].typedValues);
                }
            }
        }
        REQUIRE(expected.size() == count);
        REQUIRE_FALSE(fieldBody.empty());
        std::vector<c::LocalRecord> records(defaults->records().begin(), defaults->records().end());
        records[0].hasTime = true;
        records[0].indexedTimeLayout = c::IndexedTimePairInt32{count, count + 1};
        records[0].fields.push_back({8});
        records[0].fields.push_back({8});
        records[0].lookups.push_back({u"History.IndexHigh", count});
        records[0].lookups.push_back({u"History.IndexLow", count + 1});
        auto local = c::LocalSchema::create(records, schemaBounds, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        auto schemaCopy = *local;
        auto work = turn();
        auto collector = s::HistoryCollector::create(std::move(schemaCopy), limits(), {true, 100, 10000}, work);
        REQUIRE(collector);
        {
            auto live = genericSession(*local);
            const auto sender = e::readCompactInt(oldDescription);
            REQUIRE(sender.status == e::Status::OK);
            auto rest = std::span<const std::byte>(oldDescription).subspan(sender.consumed);
            const auto name = e::readUtfString(rest, {8192, 1024});
            REQUIRE(name.status == e::Status::OK);
            REQUIRE(name.value);
            rest = rest.subspan(name.consumed);
            const auto fieldCount = e::readCompactInt(rest);
            REQUIRE(fieldCount.status == e::Status::OK);
            REQUIRE(fieldCount.value > 0);
            REQUIRE(fieldCount.value <= static_cast<std::int32_t>(count));
            rest = rest.subspan(fieldCount.consumed);
            std::vector<std::byte> descriptionBody;
            number(descriptionBody, sender.value);
            text(descriptionBody, *name.value);
            number(descriptionBody, fieldCount.value + 2);
            descriptionBody.insert(descriptionBody.end(), rest.begin(), rest.end());
            text(descriptionBody, u"History.IndexHigh");
            number(descriptionBody, 8);
            text(descriptionBody, u"History.IndexLow");
            number(descriptionBody, 8);
            (void)admit(live, frame(2, descriptionBody));
            // Exact Java STREAM field bytes plus explicit native index/flags form a nonstandard caller HISTORY profile.
            std::vector<std::byte> historyBody{std::byte{0xf9}};
            number(historyBody, 4);
            historyBody.insert(historyBody.end(), fieldBody.begin(), fieldBody.end());
            number(historyBody, 0);
            number(historyBody, 7);
            auto event = admit(live, frame(20, historyBody));
            REQUIRE(event.records().size() == 1);
            REQUIRE(event.records()[0].typedValues.size() == count + 2);
            REQUIRE(collector->stage(std::move(event), work));
            std::vector<std::byte> closing;
            prefix(closing, 10, u"IBM", sender.value);
            number(closing, 0);
            auto end = admit(live, frame(20, closing));
            REQUIRE(collector->stage(std::move(end), work));
            CHECK(live.finish() == p::LiveStatus::CLOSED);
        }
        auto view = collector->openView({true}, work);
        REQUIRE(view);
        const c::SubscriptionRequest request{{local->recordContext(0), u"IBM"}, 0};
        auto coreWork = command();
        REQUIRE(collector->add(*view, std::span(&request, 1), coreWork, work));
        auto offered = collector->front(*view, work);
        REQUIRE(offered);
        REQUIRE(*offered);
        const auto &record = (**offered).record;
        CHECK(record.rawTime == 7);
        CHECK(record.eventFlags == 4);
        const auto values = copyProjected(record.fields);
        REQUIRE(values.size() == count + 2);

        for (std::size_t i = 0; i < count; ++i) {
            CHECK(values[i].type == expected[i].type);
            CHECK(values[i].raw == expected[i].raw);
            CHECK(values[i].units == expected[i].units);
            CHECK(values[i].bytes == expected[i].bytes);
        }

        const q::MappingLimits mappingBounds{128, 256, 32, 64, 128, 128, 16384, 1000000};

        if (profile) {
            auto mapping = q::PreparedProfile::prepare(*local, 0, mappingBounds);
            REQUIRE(mapping);
            auto mapped = mapping->map(u"IBM", {record.context, values}, {128, 1024, 8192}, 128);
            REQUIRE(mapped);
            CHECK(mapped->flags == 224);
            CHECK(mapped->statusReason == expected[15].units);
        } else {
            auto mapping = q::PreparedQuote::prepare(*local, 0, mappingBounds);
            REQUIRE(mapping);
            auto mapped = mapping->map(u"IBM", {record.context, values}, 128);
            REQUIRE(mapped);
            CHECK(mapped->timeMs == 1000);
        }

        REQUIRE(collector->pop(*view, work));
        auto end = collector->front(*view, work);
        REQUIRE(end);
        REQUIRE(*end);
        CHECK((**end).record.rawTime == 0);
        CHECK((**end).record.eventFlags == 10);
        REQUIRE(collector->pop(*view, work));
        auto status = collector->rangeStatus(*view, request.key, work);
        REQUIRE(status);
        CHECK(status->completeRequestedRange);
    }
}

TEST_CASE("HISTORY generic single64 layout preserves nullable UTF16 and opaque payload after source destruction") {
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
    auto copy = *local;
    auto work = turn();
    auto collector = s::HistoryCollector::create(std::move(copy), limits(), {true, 100, 10000}, work);
    REQUIRE(collector);
    const std::u16string symbol{u'S', char16_t{0}, char16_t{0xd800}};
    const std::u16string payload{char16_t{0}, u'A', char16_t{0xd800}};
    {
        auto live = genericSession(*local);
        (void)admit(live, genericDescription(*local));
        std::vector<std::byte> body;
        prefix(body, 4, symbol, 17);
        number(body, 7);
        std::array<std::byte, 128> encoded{};
        const auto chars = e::writeCharArray(payload, encoded, {128, 32});
        REQUIRE(chars.status == e::Status::OK);
        body.insert(body.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(chars.written));
        number(body, -1); // Exact null UTFString.
        number(body, 0);  // Engaged empty byte array.
        number(body, 3);
        body.insert(body.end(), {std::byte{0}, std::byte{0xff}, std::byte{1}});
        number(body, -1); // Exact null marshalled payload; no deserialization.
        number(body, -5);
        auto event = admit(live, frame(20, body));
        REQUIRE(collector->stage(std::move(event), work));
        std::vector<std::byte> closing;
        prefix(closing, 10, symbol, 17);
        number(closing, 0);
        auto end = admit(live, frame(20, closing));
        REQUIRE(collector->stage(std::move(end), work));
    }
    auto view = collector->openView({true}, work);
    REQUIRE(view);
    const c::SubscriptionRequest request{{local->recordContext(0), symbol}, 0};
    auto coreWork = command();
    REQUIRE(collector->add(*view, std::span(&request, 1), coreWork, work));
    auto offered = collector->front(*view, work);
    REQUIRE(offered);
    REQUIRE(*offered);
    const auto &r = (**offered).record;
    REQUIRE(r.fields.size() == 7);
    CHECK(r.symbol == symbol);
    CHECK(r.rawTime == 7);
    REQUIRE(r.fields[1].units);
    CHECK(*r.fields[1].units == payload);
    CHECK_FALSE(r.fields[2].units);
    REQUIRE(r.fields[3].bytes);
    CHECK(r.fields[3].bytes->empty());
    REQUIRE(r.fields[4].bytes);
    CHECK(r.fields[4].bytes->size() == 3);
    CHECK((*r.fields[4].bytes)[1] == std::byte{0xff});
    CHECK_FALSE(r.fields[5].bytes);
    CHECK(r.fields[6].raw == -5);
    REQUIRE((**offered).sourceRecord);
    CHECK((**offered).sourceRecord->context.matches(local->recordContext(0)));
    REQUIRE(collector->pop(*view, work));
    auto end = collector->front(*view, work);
    REQUIRE(end);
    REQUIRE(*end);
    CHECK((**end).record.eventFlags == 10);
    CHECK_FALSE((**end).record.fields[1].units);
    CHECK_FALSE((**end).record.fields[4].bytes);
}

TEST_CASE("HISTORY detached marker copies and final releases cross threads during owner retirement") {
    std::optional<Fixture> owner(fixture());
    auto view = open(*owner);
    add(*owner, view);
    send(*owner, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    auto epoch = range(*owner, view).epoch;
    std::atomic<bool> start{false};
    std::atomic<unsigned> failures{0};
    std::vector<std::thread> workers;

    for (unsigned n = 0; n < 4; ++n) {
        workers.emplace_back([token = view, epochToken = epoch, &start, &failures]() {
            while (!start.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (unsigned i = 0; i < 10000; ++i) {
                auto a = token;
                auto b = a;
                s::HistoryToken moved(std::move(b));
                b = epochToken;

                if (!a.matches(token) || !moved.matches(token) || !b.matches(epochToken) || !a.valid()) {
                    failures.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    start.store(true, std::memory_order_release);
    owner.reset(); // All collector, desired and protocol owners retire only on this thread.
    view = {};
    epoch = {};

    for (auto &worker : workers) {
        worker.join();
    }

    CHECK(failures.load() == 0);
}

TEST_CASE("HISTORY zero capacity and exact fixed resident limits reject before allocation or source transfer") {
    auto local = schema();
    auto bounds = limits();
    bounds.partitions = 0;
    bounds.points = 0;
    bounds.symbolUnitsPerPartition = 0;
    bounds.cacheFrames = 0;
    bounds.cacheBytes = 0;
    bounds.viewSlots = 0;
    bounds.pendingItems = 0;
    bounds.pendingFrames = 0;
    bounds.pendingBytes = 0;
    auto copy = local;
    auto work = turn();
    auto probe = s::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    REQUIRE(probe);
    const auto fixed = probe->residentBytes();
    REQUIRE(fixed > 0);
    bounds.residentBytes = fixed;
    copy = local;
    work = turn();
    auto exact = s::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    REQUIRE(exact);
    CHECK(exact->residentBytes() == fixed);
    bounds.residentBytes = fixed - 1;
    copy = local;
    work = turn();
    auto rejected = [&] {
        AllocationFault fault(0);

        return s::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    }();
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == Error::RESOURCE_LIMIT);
    auto live = session(local);
    auto event = admit(live, ::data(std::array{Input{0, u"ALPHA", 1, 10, 4}}));
    work = turn();
    const auto eventBytes = event.residentBytes();
    REQUIRE(eventBytes > 0);
    auto result = exact->stage(std::move(event), work);
    REQUIRE_FALSE(result);
    CHECK(result.error().error == Error::RESOURCE_LIMIT);
    CHECK(event.residentBytes() == eventBytes);
    CHECK(exact->state() == s::HistoryState::STALE);
    CHECK(exact->frameCount() == 0);
    CHECK(exact->pointCount() == 0);
    bounds.cacheFrames = std::numeric_limits<std::size_t>::max();
    bounds.residentBytes = std::numeric_limits<std::size_t>::max();
    copy = local;
    work = turn();
    auto overflow = [&] {
        AllocationFault fault(0);

        return s::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    }();
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error().error == Error::INVALID_CONFIGURATION);
}

TEST_CASE("HISTORY retained event peak charges full old and incoming backing even when replacing one point") {
    auto local = schema();
    auto live = session(local);
    auto first = admit(live, ::data(std::array{Input{0, u"ALPHA", 3, 30, 4}}));
    const auto bytes = first.residentBytes();
    auto bounds = limits();
    bounds.cacheBytes = bytes;
    auto copy = local;
    auto work = turn();
    auto collector = s::HistoryCollector::create(std::move(copy), bounds, {true, 100, 10000}, work);
    REQUIRE(collector);
    REQUIRE(collector->stage(std::move(first), work));
    CHECK(collector->eventBytes() == bytes);
    auto second = admit(live, ::data(std::array{Input{0, u"ALPHA", 3, 31, 0}}));
    const auto incoming = second.residentBytes();
    work = turn();
    auto rejected = collector->stage(std::move(second), work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == Error::RESOURCE_LIMIT);
    CHECK(second.residentBytes() == incoming);
    CHECK(collector->state() == s::HistoryState::STALE);
    CHECK(collector->frameCount() == 1);
    CHECK(collector->pointCount() == 1);
    CHECK(collector->eventBytes() == bytes);
}

TEST_CASE("HISTORY rejects failed frames entirely and explicit reset labels a transaction interruption") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    send(f, {{0, u"ALPHA", 3, 31, 1}});
    auto pending = range(f, view);
    CHECK(pending.sourceTransactionPending);
    CHECK_FALSE(pending.completeRequestedRange);
    const auto before = f.history.pointCount();
    // Rewrite the valid envelope around a truncated record, so failure occurs inside the actual decoder.
    std::vector<std::byte> body;
    prefix(body, 1, u"ALPHA", 41);
    number(body, 0);
    number(body, 2);
    auto broken = frame(20, body);
    auto liveWork = liveTurn();
    auto result = f.live.feed(broken, liveWork);
    CHECK(result.status == p::LiveStatus::FAILED);
    CHECK_FALSE(result.event);
    CHECK(f.history.pointCount() == before);
    CHECK(range(f, view).sourceTransactionPending);
    auto work = turn();
    auto reset = f.history.reset(work);
    REQUIRE(reset);
    CHECK(f.history.state() == s::HistoryState::RESET_PENDING);
    CHECK_FALSE(range(f, view).completeRequestedRange);
    CHECK(reset->unknownLoss);
    CHECK(reset->discardedPending == 1);
    REQUIRE(f.history.acknowledgeReset(reset->sequence));
    CHECK(f.history.state() == s::HistoryState::EMPTY);
    CHECK_FALSE(range(f, view).completeRequestedRange);
}

TEST_CASE("HISTORY alias-borrowed replacement retires membership and isolates independent schema contexts") {
    auto f = fixture();
    auto other = fixture();
    auto view = open(f);
    add(f, view, 0);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 1, 10, 0}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    const auto *desired = f.history.desired(view);
    REQUIRE(desired);
    REQUIRE(desired->entries().size() == 1);
    const c::SubscriptionRequest borrowed{desired->entries()[0].key, 2};
    auto coreWork = command();
    auto work = turn();
    auto replacement = f.history.replace(view, std::span(&borrowed, 1), coreWork, work);
    REQUIRE(replacement);
    CHECK(replacement->changed);
    auto current = range(f, view);
    CHECK(current.requestedFrom == 2);
    CHECK_FALSE(current.completeRequestedRange);
    auto initial = drain(f, view);
    REQUIRE(initial.size() == 2);
    CHECK(initial[0].time == 3);
    CHECK(initial[1].time == 2);
    CHECK(initial[1].flags == 10);
    CHECK(range(f, view).completeRequestedRange);
    const c::SubscriptionRequest foreign{{other.schema.recordContext(0), u"ALPHA"}, 0};
    coreWork = command();
    work = turn();
    auto rejected = f.history.add(view, std::span(&foreign, 1), coreWork, work);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().subscriptionError == c::SubscriptionError::INVALID_CONTEXT);
    CHECK(range(f, view).requestedFrom == 2);
    CHECK(other.history.partitionCount() == 0);
}

TEST_CASE("HISTORY sustained fixed-key updates use live ownership quotas rather than cumulative tape limits") {
    auto bounds = limits();
    bounds.cacheFrames = 2;
    bounds.pendingFrames = 2;
    bounds.pendingItems = 2;
    auto f = fixture(false, bounds);
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    (void)drain(f, view);
    const auto stable = f.history.eventBytes();

    for (std::int32_t n = 0; n < 1500; ++n) {
        send(f, {{0, u"ALPHA", 3, 100 + n, 0}});
        auto rows = drain(f, view);
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].value == 100 + n);
        CHECK(f.history.frameCount() == 1);
        CHECK(f.history.pointCount() == 1);
        CHECK(f.history.eventBytes() <= stable);
    }

    CHECK(range(f, view).completeRequestedRange);
}

TEST_CASE("HISTORY each turn axis rejects atomically without consuming source or replacing an offer") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 0, 0, 10}});
    auto work = turn();
    auto first = f.history.front(view, work);
    REQUIRE(first);
    REQUIRE(*first);
    const auto fields = (**first).record.fields.data();
    const Input input[]{{0, u"ALPHA", 3, 31, 0}};
    auto event = admit(f.live, data(input));
    const auto bytes = event.residentBytes();

    for (unsigned axis = 0; axis < 4; ++axis) {
        work = turn();

        if (axis == 0)
            work.operations = 0;

        if (axis == 1)
            work.visits = 0;

        if (axis == 2)
            work.textUnits = 0;

        if (axis == 3)
            work.bytes = 0;

        auto rejected = f.history.stage(std::move(event), work);
        REQUIRE_FALSE(rejected);
        CHECK(rejected.error().error == Error::WORK_LIMIT);
        CHECK(event.residentBytes() == bytes);
        CHECK(f.history.state() == s::HistoryState::CURRENT);
        CHECK(fields[2].raw == 30);
        CHECK(f.history.pointCount() == 1);
        CHECK(f.history.viewState(view) == s::HistoryViewState::ACTIVE);
    }

    work = turn();
    REQUIRE(f.history.stage(std::move(event), work));
    auto rows = drain(f, view);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].value == 31);
}

TEST_CASE("HISTORY retains original binding generations while later descriptions share the same epoch") {
    auto f = fixture();
    auto view = open(f);
    add(f, view);
    const auto old = f.live.peer().identity().generation();
    send(f, {{0, u"ALPHA", 3, 30, 4}});
    (void)admit(f.live, description(41, 0));
    const auto later = f.live.peer().identity().generation();
    REQUIRE(later > old);
    send(f, {{0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    auto work = turn();
    auto top = f.history.front(view, work);
    REQUIRE(top);
    REQUIRE(*top);
    REQUIRE((**top).trigger);
    CHECK((**top).record.rawTime == 3);
    CHECK((**top).trigger->bindingGeneration == old);
    const auto epoch = (**top).epoch;
    REQUIRE(f.history.pop(view, work));
    auto next = f.history.front(view, work);
    REQUIRE(next);
    REQUIRE(*next);
    REQUIRE((**next).trigger);
    CHECK((**next).record.rawTime == 2);
    CHECK((**next).trigger->bindingGeneration == later);
    CHECK((**next).epoch.matches(epoch));
}

TEST_CASE("HISTORY physical VOID high slot preserves full removal identity separately from logical low index") {
    const c::LocalRecord record{
        0, u"Order#NTV", true, {{8}, {8}}, {{u"Index", 0}, {u"Value", 1}}, c::IndexedTimeLowInt32{0}};
    const std::array declarations{record};
    auto local = c::LocalSchema::create(declarations, {1, 2, 2, 32, 128}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(local);
    auto live = genericSession(*local);
    std::vector<std::byte> describe;
    number(describe, 17);
    text(describe, u"Order#NTV");
    number(describe, 3);

    for (const auto &[name, type] :
         std::array<std::pair<std::u16string_view, std::int32_t>, 3>{{{u"Void", 0}, {u"Index", 8}, {u"Value", 8}}}) {
        text(describe, name);
        number(describe, type);
    }

    (void)admit(live, frame(2, describe));
    auto copied = *local;
    auto work = turn();
    auto history = s::HistoryCollector::create(std::move(copied), limits(), {false, 100, 10000}, work);
    REQUIRE(history);
    auto view = history->openView({false}, work);
    REQUIRE(view);
    const c::SubscriptionRequest request{{local->recordContext(0), u"AAPL"}, std::numeric_limits<std::int64_t>::min()};
    auto coreWork = command();
    REQUIRE(history->add(*view, std::span(&request, 1), coreWork, work));
    const auto stage = [&](std::int64_t index, std::int32_t flags) {
        std::vector<std::byte> body;
        prefix(body, flags, u"AAPL", 17);
        number(body, index);

        if (!(flags & 2)) {
            number(body, 123);
        }

        auto event = admit(live, frame(20, body));
        REQUIRE(event.records().size() == 1);

        if (flags & 2) {
            REQUIRE(event.records()[0].removalTime);
            CHECK(*event.records()[0].removalTime == index);
        }

        auto credits = turn();
        REQUIRE(history->stage(std::move(event), credits));
    };
    const auto take = [&](std::int64_t raw, std::int32_t low) {
        INFO("expected collector key=", raw);
        auto credits = turn();
        auto front = history->front(*view, credits);
        REQUIRE(front);
        REQUIRE(*front);
        CHECK((**front).record.rawTime == raw);
        CHECK((**front).record.fields[0].raw == low);

        if ((**front).trigger) {
            CHECK((**front).trigger->rawTime == raw);
        }

        REQUIRE(history->pop(*view, credits));
    };
    stage(5, 0);
    take(5, 5);
    CHECK(history->pointCount() == 1);
    constexpr std::int64_t HIGH_REMOVAL = (std::int64_t{1} << 32) | 5;
    stage(HIGH_REMOVAL, 2);
    // An unchanged unknown removal is suppressed by the production collector projection.
    work = turn();
    auto unchanged = history->front(*view, work);
    REQUIRE(unchanged);
    CHECK_FALSE(*unchanged);
    CHECK(history->pointCount() == 1);
    stage(0, 0);
    take(0, 0);
    CHECK(history->pointCount() == 2);
    stage(0, 2);
    take(0, 0);
    CHECK(history->pointCount() == 1);
    stage(5, 2);
    take(5, 5);
    CHECK(history->pointCount() == 0);
    stage(-1, 0);
    take(4294967295LL, -1);
    CHECK(history->pointCount() == 1);
    stage(std::numeric_limits<std::int64_t>::max(), 2);
    CHECK(history->pointCount() == 1);
    work = turn();
    auto sentinel = history->front(*view, work);
    REQUIRE(sentinel);

    if (*sentinel) {
        CHECK((**sentinel).record.rawTime == std::numeric_limits<std::int64_t>::max());
        CHECK((**sentinel).record.fields[0].raw == -1);
        REQUIRE(history->pop(*view, work));
    }
}

TEST_CASE("HISTORY private replay prepays exact work and preserves sibling view epoch and failed cuts") {
    auto f = fixture();
    auto view = open(f);
    auto sibling = open(f);
    add(f, view);
    add(f, sibling);
    send(f, {{0, u"ALPHA", 3, 30, 4}, {0, u"ALPHA", 2, 20, 0}, {0, u"ALPHA", 0, 0, 10}});
    REQUIRE(drain(f, view).size() == 3);
    REQUIRE(drain(f, sibling).size() == 3);
    const c::SubscriptionKey key{f.schema.recordContext(0), u"ALPHA"};
    const c::SubscriptionRequest request{key, 0};
    auto status = [&](const auto &token) {
        auto work = turn();
        auto result = f.history.rangeStatus(token, key, work);
        REQUIRE(result);

        return *result;
    };
    const auto before = status(view), healthy = status(sibling);
    REQUIRE(before.completeRequestedRange);
    CHECK_FALSE(before.initialPending);
    auto credits = turn();
    const auto supplied = credits;
    auto coreWork = command();
    REQUIRE(s::detail::HistoryReplayAccess::replace(f.history, view, {&request, 1}, coreWork, credits));
    const s::HistoryWork exact{supplied.operations - credits.operations, supplied.visits - credits.visits,
                               supplied.textUnits - credits.textUnits, supplied.bytes - credits.bytes};
    const auto replayed = status(view);
    CHECK(replayed.observationSerial == before.observationSerial + 1);
    CHECK(replayed.initialPending);
    CHECK(replayed.epoch.matches(before.epoch));
    CHECK_FALSE(status(sibling).initialPending);
    CHECK(status(sibling).epoch.matches(healthy.epoch));
    CHECK(drain(f, sibling).empty());

    for (unsigned kind = 0; kind < 2; ++kind) {
        auto shortWork = exact;

        if (kind == 0)
            shortWork = {};
        else {
            REQUIRE(shortWork.visits > 0);
            --shortWork.visits;
        }

        coreWork = command();
        const auto result =
            s::detail::HistoryReplayAccess::replace(f.history, view, {&request, 1}, coreWork, shortWork);
        REQUIRE_FALSE(result);
        CHECK(result.error().error == Error::WORK_LIMIT);
        CHECK(status(view).observationSerial == replayed.observationSerial);
        CHECK(status(view).initialPending);
    }

    credits = exact;
    coreWork = command();
    REQUIRE(s::detail::HistoryReplayAccess::replace(f.history, view, {&request, 1}, coreWork, credits));
    CHECK(credits.operations == 0);
    CHECK(credits.visits == 0);
    CHECK(credits.textUnits == 0);
    CHECK(credits.bytes == 0);
    CHECK(status(view).observationSerial == replayed.observationSerial + 1);
    REQUIRE(drain(f, view).size() == 3);
    CHECK(drain(f, sibling).empty());
    credits = turn();
    coreWork = command();
    const s::HistoryViewToken invalid;
    const auto refused = s::detail::HistoryReplayAccess::replace(f.history, invalid, {&request, 1}, coreWork, credits);
    REQUIRE_FALSE(refused);
    CHECK(refused.error().error == Error::INVALID_VIEW);
    CHECK(status(view).epoch.matches(before.epoch));
    credits = turn();
    auto reset = f.history.reset(credits);
    REQUIRE(reset);
    REQUIRE(f.history.acknowledgeReset(reset->sequence));
    credits = turn();
    coreWork = command();
    REQUIRE(s::detail::HistoryReplayAccess::replace(f.history, view, {&request, 1}, coreWork, credits));
    CHECK_FALSE(status(view).completeRequestedRange);
    CHECK_FALSE(status(view).epoch.matches(before.epoch));
}

TEST_CASE("MarketMaker HISTORY keeps full removal keys independently of narrowed public exchange identity") {
    auto local = q::defaultMarketMakerSchema({4, 128, 256, 128, 16384});
    REQUIRE(local);
    auto live = genericSession(*local);
    std::vector<std::byte> describe;
    number(describe, 17);
    text(describe, u"MarketMaker");
    number(describe, 10);
    const auto &record = local->records()[0];

    for (std::size_t field = 0; field < record.fields.size(); ++field) {
        const auto key = std::ranges::find_if(record.lookups, [&](const auto &lookup) {
            return lookup.fieldId == field;
        });
        REQUIRE(key != record.lookups.end());
        text(describe, key->name);
        number(describe, record.fields[field].serialTypeId);
    }

    (void)admit(live, frame(2, describe));
    auto copy = *local;
    auto work = turn();
    auto history = s::HistoryCollector::create(std::move(copy), limits(), {false, 100, 10000}, work);
    REQUIRE(history);
    auto view = history->openView({false}, work);
    REQUIRE(view);
    const c::SubscriptionRequest request{{local->recordContext(0), u"AAPL"}, std::numeric_limits<std::int64_t>::min()};
    auto desired = command();
    REQUIRE(history->add(*view, std::span(&request, 1), desired, work));
    auto stage = [&](std::int64_t time, std::int32_t flags) {
        std::vector<std::byte> body;
        prefix(body, flags, u"AAPL", 17);

        if (flags & 2) {
            number(body, time);
        } else {
            constexpr std::array<std::int64_t, 10> fields{0, 0, 0, 384, 384, 384, 0, 384, 384, 384};

            for (const auto value : fields) {
                number(body, value);
            }
        }

        auto event = admit(live, frame(20, body));
        REQUIRE(event.records().size() == 1);

        if (flags & 2) {
            CHECK(event.records()[0].removalTime == time);
            const auto bits = static_cast<std::uint64_t>(time);
            CHECK(event.records()[0].typedValues[0].raw ==
                  std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits >> 32)));
            CHECK(event.records()[0].typedValues[1].raw ==
                  std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(bits)));
        }

        auto credits = turn();
        REQUIRE(history->stage(std::move(event), credits));
    };
    stage(0, 0);
    work = turn();
    auto first = history->front(*view, work);
    REQUIRE(first);
    REQUIRE(*first);
    CHECK((**first).record.rawTime == 0);
    REQUIRE(history->pop(*view, work));
    CHECK(history->pointCount() == 1);

    // These are the same exact full removal words captured from the actual Java composer/parser.
    for (const auto time : std::array<std::int64_t, 4>{std::numeric_limits<std::int64_t>::min(), -1,
                                                       std::numeric_limits<std::int64_t>::max(), 4294967301LL}) {
        stage(time, 2);
        CHECK(history->pointCount() == 1);
        work = turn();
        auto offered = history->front(*view, work);
        REQUIRE(offered);

        if (*offered) {
            CHECK((**offered).record.rawTime == time);
            REQUIRE(history->pop(*view, work));
        }
    }

    stage(0, 2);
    CHECK(history->pointCount() == 0);
    work = turn();
    auto removed = history->front(*view, work);
    REQUIRE(removed);
    REQUIRE(*removed);
    CHECK((**removed).record.rawTime == 0);
    CHECK((**removed).record.fields[0].raw == 0);
    REQUIRE(history->pop(*view, work));
}
