// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#ifdef QDCXX_WILDCARD_CLIENT
#    include "qd/client/stream_dispatcher.hpp"
#    include "qd/mux/queue.hpp"
#endif
#include <doctest/doctest.h>

#include "qd/collectors/history.hpp"
#include "qd/collectors/stream.hpp"
#include "qd/collectors/ticker.hpp"
#include "qd/core/subscription.hpp"
#include "qd/encoding/char_array.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/penta_symbol.hpp"
#include "qd/encoding/symbol_reader.hpp"
#include "qd/encoding/utf_char.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/mux/interest.hpp"
#include "qd/protocol/live_session.hpp"
#include "qd/protocol/subscription_writer.hpp"
#include <algorithm>
#include <iterator>

#include <array>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <latch>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace m = org::ttldtor::qdcxx::mux;
namespace p = org::ttldtor::qdcxx::protocol;
namespace col = org::ttldtor::qdcxx::collectors;

namespace {
/** Construct complete timed synthetic schema, not an untimed business-event substitute.
 * @return Independently owned valid schema; fixture construction failure aborts the case.
 */
c::LocalSchema schema() {
    const c::LocalRecord records[]{{0,
                                    u"G6#ONE",
                                    true,
                                    {{8}, {8}, {8}},
                                    {{u"TimeHi", 0}, {u"TimeLo", 1}, {u"Value", 2}},
                                    c::IndexedTimePairInt32{0, 1}},
                                   {1,
                                    u"G6#TWO",
                                    true,
                                    {{8}, {8}, {8}},
                                    {{u"TimeHi", 0}, {u"TimeLo", 1}, {u"Value", 2}},
                                    c::IndexedTimePairInt32{0, 1}}};
    auto result = c::LocalSchema::create(records, {4, 16, 16, 64, 1024}, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Fresh finite independent caller turn.
 * @return Explicit desired-state work, never internally replenished.
 */
c::SubscriptionWork work() {
    return {1000, 1000000, 1000000};
}

/** Fresh finite pure MUX turn.
 * @return Explicit metadata/text/byte credits.
 */
m::MuxWork muxWork() {
    return {1000, 1000000000, 1000000000, 1000000000};
}

constexpr c::SubscriptionLimits DESIRED{4, 16, 64, 1024, 32, 2048, 16}; ///< Fixed desired bounds.

/** Canonical UTF-16 observation, preserving NUL and isolated surrogate units.
 * @param value Actual input/decoded units.
 * @return Uppercase four-hex-unit string, including empty.
 */
std::string units(std::u16string_view value) {
    std::ostringstream out;

    for (const auto unit : value) {
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(unit);
    }

    return out.str();
}

/** Observe actual encoded bytes without interpreting symbol semantics.
 * @param value Actual byte span.
 * @return Uppercase hex bytes.
 */
std::string hex(std::span<const std::byte> value) {
    std::ostringstream out;

    for (const auto byte : value) {
        out << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << std::to_integer<unsigned>(byte);
    }

    return out.str();
}

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
    text(payload, local == 0 ? u"G6#ONE" : u"G6#TWO");
    number(payload, 3);

    for (const auto name : {u"TimeHi", u"TimeLo", u"Value"}) {
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
    bool canonical = true;      ///< False explicitly sends raw string domain.
};

/** Compose selected source tuples through actual symbol/compact codecs.
 * @param inputs Complete ordered records.
 * @param type Exact data family, STREAM_DATA by default.
 * @return Whole native admission frame.
 */
std::vector<std::byte> sourceFrame(std::span<const Input> inputs, std::int32_t type = 15) {
    std::vector<std::byte> payload;

    for (const auto &input : inputs) {
        payload.push_back(std::byte{0xf9});
        number(payload, input.flags);
        const auto encoded =
            input.canonical ? e::encodePentaSymbol(input.symbol) : std::optional<e::PentaSymbolEncoding>{};

        if (encoded) {
            payload.insert(payload.end(), encoded->bytes.begin(),
                           encoded->bytes.begin() + static_cast<std::ptrdiff_t>(encoded->size));
        } else {
            payload.push_back(std::byte{0xfd});
            number(payload, static_cast<std::int64_t>(input.symbol.size()));

            for (const auto unit : input.symbol) {
                std::array<std::byte, 4> bytes{};
                const auto written = e::writeUtfChar(unit, bytes).written;
                payload.insert(payload.end(), bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(written));
            }
        }

        number(payload, input.record);

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
    const p::OutgoingRecord declarations[]{{0, 0, u"G6#ONE", {{u"TimeHi", 8, 0}, {u"TimeLo", 8, 1}, {u"Value", 8, 2}}},
                                           {1, 1, u"G6#TWO", {{u"TimeHi", 8, 0}, {u"TimeLo", 8, 1}, {u"Value", 8, 2}}}};
    p::SubscriptionWriterWork writerWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(source, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, writerWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto result = p::LiveSession::create(std::move(copied), std::move(*writer), bounds, work);
    REQUIRE(result);
    (void)admit(*result, ready());
    (void)admit(*result, description(0, 0));
    (void)admit(*result, description(1, 1));

    return std::move(*result);
}

} // namespace

namespace {
/** Create a real arbitrary-schema session without synthetic sender bindings.
 * @param local Complete owned immutable catalog.
 * @return Ready serialized source owner, with no DATA decoded yet.
 */
p::LiveSession genericSession(const c::LocalSchema &local) {
    std::vector<p::OutgoingRecord> declarations;

    for (const auto &record : local.records()) {
        p::OutgoingRecord output{17 + record.id, record.id, record.name, {}};

        for (std::size_t field = 0; field < record.fields.size(); ++field) {
            const auto lookup = std::ranges::find_if(record.lookups, [field](const auto &entry) {
                return entry.fieldId == field;
            });
            REQUIRE(lookup != record.lookups.end());
            output.fields.push_back(
                {lookup->name, record.fields[field].serialTypeId, static_cast<std::int32_t>(field)});
        }

        declarations.push_back(std::move(output));
    }

    auto copy = local;
    p::SubscriptionWriterWork outputWork{10000, 1000000, 1000000, 1000000};
    auto writer = p::SubscriptionWriter::create(local, declarations, {{16, 128, 256, 128, 16384}, 16, 128, 128, 16384},
                                                {8192, 8201, 512, 1024, {8192, 1024}, 65536, 65536, 128}, outputWork);
    REQUIRE(writer);
    auto work = liveTurn();
    auto live = p::LiveSession::create(std::move(copy), std::move(*writer), liveLimits(), work);
    REQUIRE(live);
    (void)admit(*live, ready());

    return std::move(*live);
}

/** Preserve all typed/null/text/opaque payload information independently of the source lifetime.
 * @param fields Complete admitted materialized values.
 * @return Test-owned deep copies, never borrowed protocol spans.
 */
std::vector<c::FieldValue> copyFields(std::span<const p::LiveFieldValueView> fields) {
    std::vector<c::FieldValue> result;

    for (const auto &field : fields) {
        c::FieldValue value{field.type, field.raw};

        if (field.units)
            value.units = std::u16string(*field.units);

        if (field.bytes)
            value.bytes = std::vector<std::byte>(field.bytes->begin(), field.bytes->end());

        result.push_back(std::move(value));
    }

    return result;
}

/** Emit actual postdecoder identity/payload without inventing physical stored-symbol metadata.
 * @param out Task-private trace stream.
 * @param role Fixed source phase.
 * @param record Actual admitted source record.
 * @param local Actual matching local schema.
 * @param index Actual zero-based source/delivery index within the named phase.
 */
void traceInput(std::ostream &out, const char *role, const p::LiveRecordView &record, const c::LocalSchema &local,
                std::size_t index = 0) {
    auto definition = std::ranges::find_if(local.records(), [&](const auto &r) {
        return r.id == record.localId;
    });
    REQUIRE(definition != local.records().end());
    REQUIRE(record.context.matches(local.recordContext(definition->id)));
    REQUIRE(record.typedValues.size() == 3);
    const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(record.typedValues[0].raw)) << 32) |
                      static_cast<std::uint32_t>(record.typedValues[1].raw);
    out << role << '\t' << index << '\t' << units(definition->name) << '\t' << record.cipher << '\t'
        << units(record.symbol) << '\t' << record.removalTime.value_or(std::bit_cast<std::int64_t>(bits)) << '\t'
        << record.typedValues[2].raw << '\t' << record.eventFlags << '\n';
}
} // namespace

TEST_CASE("wildcard All preserves complete standard Quote and Profile materialized bodies") {
    namespace events = org::ttldtor::qdcxx::events;
    const c::SchemaLimits bounds{16, 128, 256, 128, 16384};

    for (const bool profile : {false, true}) {
        auto defaults = profile ? events::defaultProfileSchema(bounds) : events::defaultQuoteSchema(bounds);
        REQUIRE(defaults);
        auto local = c::LocalSchema::create(defaults->records(), bounds, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(local);
        const auto fields = profile ? 16 : 8;
        REQUIRE(local->records()[0].fields.size() == fields);
        auto live = genericSession(*local);
        const auto relative = profile ? "tests/fixtures/encoding/field-types/expected/profile-stream.qtp"
                                      : "tests/fixtures/protocol/quote/expected/time-1.qtp";
        std::ifstream in(std::filesystem::path(QDCXX_WILDCARD_SOURCE_ROOT) / relative, std::ios::binary);
        REQUIRE(in);
        const std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
        const auto input = std::as_bytes(std::span(bytes));
        p::FrameReader reader(liveLimits().frame);
        std::size_t offset = 0;
        std::vector<std::byte> canonical;
        std::vector<c::FieldValue> expected;

        while (offset < input.size()) {
            auto parsed = reader.feed(input.subspan(offset));
            REQUIRE(parsed.frame);
            offset += parsed.accepted;

            if (parsed.frame->type == 2)
                (void)admit(live, frame(2, parsed.frame->payload));
            else if (parsed.frame->type == 15) {
                auto event = admit(live, frame(15, parsed.frame->payload));
                REQUIRE(event.records().size() == 1);
                expected = copyFields(event.records()[0].typedValues);
                e::SymbolReader symbols({8192, 128, 128}, 0);
                auto symbol = symbols.read(parsed.frame->payload);
                REQUIRE(symbol.status == e::SymbolStatus::OK);
                REQUIRE(symbol.value);
                CHECK(symbol.value->eventFlags == 0);
                auto encoded = e::encodePentaSymbol(symbol.value->units);
                REQUIRE(encoded);
                canonical.assign(encoded->bytes.begin(),
                                 encoded->bytes.begin() + static_cast<std::ptrdiff_t>(encoded->size));
                canonical.insert(canonical.end(),
                                 parsed.frame->payload.begin() + static_cast<std::ptrdiff_t>(symbol.consumed),
                                 parsed.frame->payload.end());
            }
        }

        REQUIRE(expected.size() == fields);
        auto desired = c::SubscriptionState::create(*local, c::SubscriptionFamily::STREAM, DESIRED,
                                                    c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
        REQUIRE(desired);
        const c::SubscriptionRequest all{c::allSymbols(local->recordContext(0)), {}};
        auto desiredWork = work();
        REQUIRE(desired->add(std::span(&all, 1), desiredWork));
        col::StreamWork turn{100000, 1000000000, 1000000000, 1000000000};
        auto collector =
            col::StreamCollector::create(std::move(*desired), {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, turn);
        REQUIRE(collector);
        auto event = admit(live, frame(15, canonical));
        const auto cipher = event.records()[0].cipher;
        CHECK(cipher != 0);
        REQUIRE(collector->stage(std::move(event), turn));
        auto front = collector->front(turn);
        REQUIRE(front);
        REQUIRE(*front);
        const auto &record = *(**front).record;
        CHECK(record.cipher == cipher);
        CHECK(record.context.matches(local->recordContext(0)));
        const auto actual = copyFields(record.typedValues);
        REQUIRE(actual.size() == expected.size());

        for (std::size_t field = 0; field < actual.size(); ++field) {
            CHECK(actual[field].type == expected[field].type);
            CHECK(actual[field].raw == expected[field].raw);
            CHECK(actual[field].units == expected[field].units);
            CHECK(actual[field].bytes == expected[field].bytes);
        }

        REQUIRE(collector->pop(turn));
    }
}

TEST_CASE("wildcard stateless encoder emits actual boundary vectors for independent production comparison") {
    std::vector<std::u16string> values;

    for (char16_t unit = 0; unit < 128; ++unit) {
        values.emplace_back(1, unit);
    }

    for (const auto text : {u"",        u"A",        u"AB",      u"ABC",     u"ABCD",     u"ABCDE",  u"ABCDEF",
                            u"ABCDEFG", u"ABCDEFGH", u"/AAAAAA", u"$AAAAAA", u"aAAAAAA",  u"......", u".......",
                            u"AB*",     u"A?",       u"ALPHA",   u"BETA",    u"FUTURE42", u"\u0416", u"\U0001F680"}) {
        values.emplace_back(text);
    }

    values.emplace_back(1, static_cast<char16_t>(0xd800));
    values.emplace_back(1, static_cast<char16_t>(0xdc00));
    values.emplace_back(u"A\0B", 3);
    values.emplace_back(u"'");
    values.emplace_back(u"`");
    const std::filesystem::path directory{QDCXX_WILDCARD_OUTPUT};
    std::filesystem::create_directories(directory);
    std::ofstream out(directory / "penta-vectors.tsv", std::ios::binary);
    REQUIRE(out);
    out << "index\tunits\tcipher\tpenta-bytes\n";
    std::size_t index = 0;

    for (const auto &value : values) {
        const auto encoded = e::encodePentaSymbol(value);
        out << index++ << '\t' << units(value) << '\t' << (encoded ? encoded->cipher : 0) << '\t';

        if (encoded) {
            CHECK(encoded->size >= 1);
            CHECK(encoded->size <= encoded->bytes.size());
            const auto bytes = std::span(encoded->bytes).first(encoded->size);
            out << hex(bytes);
            e::SymbolReader reader({64, 16, 16}, 0);
            auto decoded = reader.read(bytes);
            REQUIRE(decoded.status == e::SymbolStatus::OK);
            REQUIRE(decoded.value);
            CHECK(decoded.value->units == value);
            CHECK(decoded.value->cipher == encoded->cipher);
            CHECK(decoded.consumed == bytes.size());
        } else {
            out << '-';
        }

        out << '\n';
    }

    out.close();
    REQUIRE(out);
    CHECK(index == 154);
}

TEST_CASE("wildcard selector defaults stay exact and unsupported families refuse explicit All") {
    auto local = schema();

    for (auto family : {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto state = c::SubscriptionState::create(local, family, DESIRED);
        REQUIRE(state);
        const c::SubscriptionRequest request{c::allSymbols(local.recordContext(0)), {}};
        auto turn = work();
        auto result = state->add(std::span(&request, 1), turn);
        REQUIRE_FALSE(result);
        CHECK(result.error() == c::SubscriptionError::UNSUPPORTED_SELECTOR);
        CHECK(state->entries().empty());

        if (family != c::SubscriptionFamily::STREAM) {
            auto forbidden = c::SubscriptionState::create(local, family, DESIRED,
                                                          c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
            REQUIRE_FALSE(forbidden);
            CHECK(forbidden.error() == c::SubscriptionError::UNSUPPORTED_SELECTOR);
        }
    }
}

TEST_CASE("wildcard captured singleton choice preserves either original activation without resurrection") {
    auto local = schema();
    auto result = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                               c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(result);
    auto &state = *result;
    const std::array<c::SubscriptionRequest, 2> requests{
        {{c::allSymbols(local.recordContext(0)), {}}, {{local.recordContext(0), u"ALPHA"}, {}}}};
    auto turn = work();
    REQUIRE(state.add(requests, turn));
    const auto all = state.entries()[0].membership;
    const auto exact = state.entries()[1].membership;
    const auto captured = c::SubscriptionMembership::anyOf(all, exact);
    CHECK(state.isCurrent(captured));
    CHECK(state.isCurrent(c::SubscriptionMembership::anyOf(all, all)));
    CHECK_FALSE(state.isCurrent(c::SubscriptionMembership::anyOf(captured, captured)));
    CHECK_FALSE(state.isCurrent(c::SubscriptionMembership::anyOf({}, all)));
    const auto allKey = c::allSymbols(local.recordContext(0));
    turn = work();
    REQUIRE(state.remove(std::span(&allKey, 1), turn));
    CHECK(state.isCurrent(captured));
    turn = work();
    REQUIRE(state.add(std::span(requests.data(), 1), turn));
    turn = work();
    REQUIRE(state.remove(std::span(&requests[1].key, 1), turn));
    CHECK_FALSE(state.isCurrent(captured));
    CHECK_FALSE(state.isCurrent(all));
    CHECK_FALSE(state.isCurrent(exact));
}

TEST_CASE("wildcard MUX exact and All aggregate separately and capture either original match") {
    auto local = schema();
    auto turn = muxWork();
    auto state =
        m::MuxState::create(local, c::SubscriptionFamily::STREAM, {4, 4, 16, 16, 64, 1024, 16, 4 * 1024 * 1024}, turn,
                            c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(state);
    auto a = state->createClient(turn);
    auto b = state->createClient(turn);
    REQUIRE(a);
    REQUIRE(b);
    const std::array<c::SubscriptionRequest, 2> requests{
        {{c::allSymbols(local.recordContext(0)), {}}, {{local.recordContext(0), u"ALPHA"}, {}}}};
    auto shortTurn = muxWork();
    shortTurn.symbolUnits = 1000000;
    const auto revision = state->revision();
    const auto refused = state->add(*a, requests, shortTurn);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == m::MuxError::WORK_LIMIT);
    CHECK(state->revision() == revision);
    CHECK(state->aggregateEntries().empty());
    REQUIRE(state->add(*a, requests, turn));
    REQUIRE(state->add(*b, std::span(requests.data(), 1), turn));
    CHECK(state->aggregateEntries().size() == 2);
    auto selected = state->select(*a, requests[1].key, {}, turn);
    REQUIRE(selected);
    REQUIRE(*selected);
    REQUIRE(state->remove(*a, std::span(&requests[0].key, 1), turn));
    CHECK(state->aggregateEntries().size() == 2);
    auto current = state->isCurrent(*a, **selected, turn);
    REQUIRE(current);
    CHECK(*current);
    REQUIRE(state->remove(*b, std::span(&requests[0].key, 1), turn));
    CHECK(state->aggregateEntries().size() == 1);
    REQUIRE(state->remove(*a, std::span(&requests[1].key, 1), turn));
    current = state->isCurrent(*a, **selected, turn);
    REQUIRE(current);
    CHECK_FALSE(*current);
}

TEST_CASE("wildcard canonical Exact policy supports three families without granting All") {
    auto local = schema();

    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        auto state =
            c::SubscriptionState::create(local, family, DESIRED, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1);
        REQUIRE(state);
        const c::SubscriptionRequest exact{{local.recordContext(0), u"ALPHA"},
                                           family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0}
                                                                                    : std::nullopt};
        auto turn = work();
        REQUIRE(state->add(std::span(&exact, 1), turn));
        const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), exact.historyBound};
        turn = work();
        const auto refused = state->add(std::span(&all, 1), turn);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == c::SubscriptionError::UNSUPPORTED_SELECTOR);
        CHECK(state->entries().size() == 1);
    }
}

TEST_CASE("wildcard writer emits actual whole six-family canonical and unchanged legacy envelopes") {
    auto local = schema();
    const p::OutgoingRecord layout{0, 0, u"G6#ONE", {{u"TimeHi", 8, 0}, {u"TimeLo", 8, 1}, {u"Value", 8, 2}}};
    const std::array<std::u16string_view, 7> symbols{u"ALPHA", u"ALPHA",           u"", u"FUTURE42", u"AB*",
                                                     u"A?",    u"\u0416\U0001F680"};
    const std::filesystem::path output{std::filesystem::path(QDCXX_WILDCARD_OUTPUT) / "wire"};
    std::filesystem::create_directories(output);

    for (const auto profile :
         {c::SubscriptionSelectorPolicy::EXACT_ONLY, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1,
          c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1}) {
        p::SubscriptionWriterWork turn{100000, 10000000, 10000000, 10000000};
        auto writer = p::SubscriptionWriter::create(
            local, std::span(&layout, 1), {{4, 16, 16, 64, 1024}, 4, 64, 64, 8192},
            {8192, 8201, 64, 1024, {8192, 1024}, 1024 * 1024, 1024 * 1024, 128}, turn, profile);
        REQUIRE(writer);
        const std::int32_t id = 0;
        auto description = writer->describe(std::span(&id, 1), turn);
        REQUIRE(description);

        for (const auto message : {p::SubscriptionMessage::TICKER_ADD, p::SubscriptionMessage::TICKER_REMOVE,
                                   p::SubscriptionMessage::STREAM_ADD, p::SubscriptionMessage::STREAM_REMOVE,
                                   p::SubscriptionMessage::HISTORY_ADD, p::SubscriptionMessage::HISTORY_REMOVE}) {
            std::vector<c::SubscriptionRequest> requests;

            for (std::size_t i = 0; i < symbols.size(); ++i) {
                requests.push_back({{local.recordContext(0), symbols[i]},
                                    message == p::SubscriptionMessage::HISTORY_ADD
                                        ? std::optional<std::int64_t>{static_cast<std::int64_t>(i) - 2}
                                        : std::nullopt});
            }

            auto bytes = writer->compose(message, requests, turn);
            REQUIRE(bytes);

            if (profile != c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1) {
                const auto name = (profile == c::SubscriptionSelectorPolicy::EXACT_ONLY ? "legacy-" : "canonical-") +
                                  std::to_string(static_cast<std::int32_t>(message)) + ".qtp";
                std::ofstream file(output / name, std::ios::binary);
                const auto metadata = description->bytes();
                const auto content = bytes->bytes();
                file.write(reinterpret_cast<const char *>(metadata.data()),
                           static_cast<std::streamsize>(metadata.size()));
                file.write(reinterpret_cast<const char *>(content.data()),
                           static_cast<std::streamsize>(content.size()));
                REQUIRE(file);
            }

            const c::SubscriptionRequest all{
                c::allSymbols(local.recordContext(0)),
                message == p::SubscriptionMessage::HISTORY_ADD ? std::optional<std::int64_t>{0} : std::nullopt};
            auto selected = writer->compose(message, std::span(&all, 1), turn);
            const bool supported =
                profile == c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1 &&
                (message == p::SubscriptionMessage::STREAM_ADD || message == p::SubscriptionMessage::STREAM_REMOVE);
            CHECK(selected.has_value() == supported);

            if (!supported)
                CHECK(selected.error() == p::SubscriptionWriterError::UNSUPPORTED_SELECTOR);
            else {
                const std::array<c::SubscriptionRequest, 6> mixed{{{c::allSymbols(local.recordContext(0)), {}},
                                                                   {c::allSymbols(local.recordContext(0)), {}},
                                                                   {{local.recordContext(0), u""}, {}},
                                                                   {{local.recordContext(0), u"ALPHA"}, {}},
                                                                   {{local.recordContext(0), u"ALPHA"}, {}},
                                                                   {c::allSymbols(local.recordContext(0)), {}}}};
                auto actual = writer->compose(message, mixed, turn);
                REQUIRE(actual);
                std::ofstream file(output / ("all-" + std::to_string(static_cast<std::int32_t>(message)) + ".qtp"),
                                   std::ios::binary);
                const auto metadata = description->bytes();
                const auto content = actual->bytes();
                file.write(reinterpret_cast<const char *>(metadata.data()),
                           static_cast<std::streamsize>(metadata.size()));
                file.write(reinterpret_cast<const char *>(content.data()),
                           static_cast<std::streamsize>(content.size()));
                REQUIRE(file);
            }
        }
    }
}

TEST_CASE("wildcard real STREAM stage preserves duplicate events and either original overlapping eligibility") {
    auto local = schema();
    auto live = session(local);
    auto desired = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(desired);
    col::StreamWork turn{1000, 1000000000, 1000000000, 1000000000};
    auto collector = col::StreamCollector::create(std::move(*desired), {8, 64, 4 * 1024 * 1024, 8 * 1024 * 1024}, turn);
    REQUIRE(collector);
    const std::array<c::SubscriptionRequest, 2> requests{
        {{c::allSymbols(local.recordContext(0)), {}}, {{local.recordContext(0), u"ALPHA"}, {}}}};
    auto command = work();
    REQUIRE(collector->add(requests, command));
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}, {0, u"BETA", 1, 20}, {1, u"ALPHA", 1, 99}};
    auto event = admit(live, sourceFrame(inputs, 15));
    auto staged = collector->stage(std::move(event), turn);
    REQUIRE(staged);
    CHECK(staged->selected == 3);
    CHECK(staged->ignored == 1);
    auto offered = collector->front(turn);
    REQUIRE(offered);
    REQUIRE(*offered);
    const auto *retained = (**offered).record;
    const auto before = collector->counts();
    const Input invalid[]{{0, u"FUTURE42", 2, 21}, {0, u"ALPHA", 2, 11, 0, false}};
    auto bad = admit(live, sourceFrame(invalid, 15));
    const auto badBacking = bad.residentBytes();
    auto noMetadata = turn;
    noMetadata.visits = 0;
    auto metadataRefused = collector->stage(std::move(bad), noMetadata);
    REQUIRE_FALSE(metadataRefused);
    CHECK(metadataRefused.error() == col::StreamError::WORK_LIMIT);
    auto noText = turn;
    noText.textUnits = 0;
    auto textRefused = collector->stage(std::move(bad), noText);
    REQUIRE_FALSE(textRefused);
    CHECK(textRefused.error() == col::StreamError::WORK_LIMIT);
    CHECK(bad.residentBytes() == badBacking);
    CHECK(retained->typedValues[2].raw == 10);
    auto refused = collector->stage(std::move(bad), turn);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == col::StreamError::UNSUPPORTED_SYMBOL_DOMAIN);
    CHECK(bad.kind() == p::LiveEventKind::DATA);
    CHECK(bad.residentBytes() == badBacking);
    CHECK(collector->counts().staged == before.staged);
    CHECK(retained->typedValues[2].raw == 10);
    command = work();
    REQUIRE(collector->remove(std::span(&requests[0].key, 1), command));
    std::vector<std::int64_t> values;

    for (;;) {
        offered = collector->front(turn);
        REQUIRE(offered);

        if (!*offered)
            break;

        values.push_back((**offered).record->typedValues[2].raw);
        REQUIRE(collector->pop(turn));
    }

    CHECK(values == std::vector<std::int64_t>{10, 10});
}

TEST_CASE("wildcard TICKER canonical domain failure preserves actual pending offer and input") {
    auto local = schema();
    auto live = session(local);
    auto copied = local;
    col::TickerWork turn{10000, 1000000000, 1000000000, 1000000000};
    auto collector =
        col::TickerCollector::create(std::move(copied), {2, 16, 16, 4 * 1024 * 1024, 16 * 1024 * 1024, DESIRED},
                                     {false, 100, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1}, turn);
    REQUIRE(collector);
    auto view = collector->openView(turn);
    REQUIRE(view);
    const c::SubscriptionRequest request{{local.recordContext(0), u"ALPHA"}, {}};
    auto command = work();
    REQUIRE(collector->add(*view, std::span(&request, 1), command, turn));
    const Input one[]{{0, u"ALPHA", 3, 30}};
    auto event = admit(live, sourceFrame(one, 10));
    std::filesystem::create_directories(QDCXX_WILDCARD_OUTPUT);
    std::ofstream trace(std::filesystem::path(QDCXX_WILDCARD_OUTPUT) / "ticker-profile.tsv", std::ios::binary);
    REQUIRE(trace);
    trace << "phase\tindex\trecord\tcipher\tdecoded-symbol\traw-time\tvalue\tflags\n";
    traceInput(trace, "live.data-parsed", event.records()[0], local);
    REQUIRE(collector->stage(std::move(event), turn));
    auto offered = collector->front(*view, turn);
    REQUIRE(offered);
    REQUIRE(*offered);
    const auto *old = (**offered).value.sourceRecord;
    const auto before = collector->counts();
    const Input invalid[]{{0, u"BETA", 4, 40}, {0, u"ALPHA", 4, 41, 0, false}};
    auto bad = admit(live, sourceFrame(invalid, 10));
    const auto backing = bad.residentBytes();
    auto noMetadata = turn;
    noMetadata.visits = 0;
    auto metadataRefused = collector->stage(std::move(bad), noMetadata);
    REQUIRE_FALSE(metadataRefused);
    CHECK(metadataRefused.error() == col::TickerError::WORK_LIMIT);
    auto noText = turn;
    noText.textUnits = 0;
    auto textRefused = collector->stage(std::move(bad), noText);
    REQUIRE_FALSE(textRefused);
    CHECK(textRefused.error() == col::TickerError::WORK_LIMIT);
    CHECK(bad.residentBytes() == backing);
    CHECK(old->typedValues[2].raw == 30);
    auto rejected = collector->stage(std::move(bad), turn);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == col::TickerError::UNSUPPORTED_SYMBOL_DOMAIN);
    CHECK(bad.residentBytes() == backing);
    CHECK(collector->counts().changed == before.changed);
    CHECK(old->typedValues[2].raw == 30);
    std::size_t output = 0;

    for (;;) {
        offered = collector->front(*view, turn);
        REQUIRE(offered);

        if (!*offered)
            break;

        traceInput(trace, "delivery", *(**offered).value.sourceRecord, local, output++);
        REQUIRE(collector->pop(*view, turn));
    }

    trace << "delivery\t-1\tCOUNT\t" << output << "\t-\t-\t-\t-\n";
    trace.close();
    REQUIRE(trace);
    CHECK(output == 1);
    auto empty = collector->front(*view, turn);
    REQUIRE(empty);
    CHECK_FALSE(*empty);
}

TEST_CASE("wildcard HISTORY canonical domain guard preserves real complete snapshot offer and range status") {
    auto local = schema();
    auto live = session(local);
    auto copied = local;
    col::HistoryWork turn{100000, 1000000000000ULL, 1000000000000ULL, 1000000000000ULL};
    auto collector = col::HistoryCollector::create(
        std::move(copied), {8, 32, 64, 16, 4 * 1024 * 1024, 2, 32, 16, 4 * 1024 * 1024, 32 * 1024 * 1024, DESIRED},
        {false, 100, 10000, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1}, turn);
    REQUIRE(collector);
    auto view = collector->openView({true}, turn);
    REQUIRE(view);
    const c::SubscriptionRequest request{{local.recordContext(0), u"ALPHA"}, 0};
    auto command = work();
    REQUIRE(collector->add(*view, std::span(&request, 1), command, turn));
    std::filesystem::create_directories(QDCXX_WILDCARD_OUTPUT);
    std::ofstream trace(std::filesystem::path(QDCXX_WILDCARD_OUTPUT) / "history-profile.tsv", std::ios::binary);
    REQUIRE(trace);
    trace << "phase\tindex\trecord\tcipher\tdecoded-symbol\traw-time\tvalue\tflags\n";
    const char *phases[]{"begin.data-parsed", "middle.data-parsed", "end.data-parsed"};
    std::size_t inputIndex = 0;

    for (const Input input :
         {Input{0, u"ALPHA", 3, 30, 4}, Input{0, u"ALPHA", 2, 20, 0}, Input{0, u"ALPHA", 0, 0, 10}}) {
        auto event = admit(live, sourceFrame(std::span(&input, 1), 20));
        traceInput(trace, phases[inputIndex++], event.records()[0], local);
        REQUIRE(collector->stage(std::move(event), turn));
    }

    auto offered = collector->front(*view, turn);
    REQUIRE(offered);
    REQUIRE(*offered);
    const auto value = (**offered).record.fields[2].raw;
    const auto status = collector->rangeStatus(*view, request.key, turn);
    REQUIRE(status);
    const Input invalid[]{{0, u"ALPHA", 4, 40, 4}, {0, u"ALPHA", 2, 21, 0, false}};
    auto bad = admit(live, sourceFrame(invalid, 20));
    const auto backing = bad.residentBytes();
    auto noMetadata = turn;
    noMetadata.visits = 0;
    auto metadataRefused = collector->stage(std::move(bad), noMetadata);
    REQUIRE_FALSE(metadataRefused);
    CHECK(metadataRefused.error().error == col::HistoryError::WORK_LIMIT);
    auto noText = turn;
    noText.textUnits = 0;
    auto textRefused = collector->stage(std::move(bad), noText);
    REQUIRE_FALSE(textRefused);
    CHECK(textRefused.error().error == col::HistoryError::WORK_LIMIT);
    CHECK(bad.residentBytes() == backing);
    CHECK((**offered).record.fields[2].raw == value);
    auto rejected = collector->stage(std::move(bad), turn);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error().error == col::HistoryError::UNSUPPORTED_SYMBOL_DOMAIN);
    CHECK(bad.residentBytes() == backing);
    CHECK((**offered).record.fields[2].raw == value);
    auto after = collector->rangeStatus(*view, request.key, turn);
    REQUIRE(after);
    CHECK(after->observationSerial == status->observationSerial);
    std::size_t output = 0;

    for (;;) {
        offered = collector->front(*view, turn);
        REQUIRE(offered);

        if (!*offered)
            break;

        const auto &delivery = **offered;
        CHECK(delivery.record.context.matches(local.recordContext(0)));
        trace << "delivery\t" << output++ << '\t' << units(local.records()[0].name) << '\t';

        if (delivery.sourceRecord)
            trace << delivery.sourceRecord->cipher;
        else
            trace << '-';

        trace << '\t' << units(delivery.record.symbol) << '\t' << delivery.record.rawTime << '\t'
              << delivery.record.fields[2].raw << '\t' << delivery.record.eventFlags << '\n';
        REQUIRE(collector->pop(*view, turn));
    }

    trace << "delivery\t-1\tCOUNT\t" << output << "\t-\t-\t-\t-\n";
    trace.close();
    REQUIRE(trace);
    CHECK(output == 3);
    auto completed = collector->rangeStatus(*view, request.key, turn);
    REQUIRE(completed);
    CHECK(completed->completeRequestedRange);
}

#ifdef QDCXX_WILDCARD_CLIENT
namespace {
/** Fresh host-supplied finite turn; the queue never increases these credits.
 * @return Independently budgeted queue and actual component work.
 */
m::QueueWork queueTurn() {
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

/** Create the actual family composition with immutable canonical profile.
 * @param local Valid owned schema, also retained by the actual components.
 * @param family Fixed source family.
 * @return Fresh owner; construction failure aborts the case.
 */
m::MuxQueue queue(const c::LocalSchema &local, c::SubscriptionFamily family) {
    const c::SubscriptionLimits desired{16, 32, 128, 4096, 64, 8192, 64};
    const m::MuxLimits interests{16, 4, 32, 32, 128, 4096, 64, 4 * 1024 * 1024};
    const m::QueueLimits limits{4, 128, 16, 4, 16, 256, 4 * 1024 * 1024, 64, 1024 * 1024, 32, 64, 8, 16 * 1024 * 1024};
    auto turn = queueTurn();
    auto result =
        family == c::SubscriptionFamily::STREAM
            ? m::MuxQueue::createStream(local, interests, desired, {8, 256, 4 * 1024 * 1024, 8 * 1024 * 1024}, limits,
                                        turn, c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1)
        : family == c::SubscriptionFamily::TICKER
            ? m::MuxQueue::createTicker(local, interests, {4, 32, 64, 4 * 1024 * 1024, 16 * 1024 * 1024, desired},
                                        {false, 100, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1}, limits, turn)
            : m::MuxQueue::createHistory(
                  local, interests,
                  {32, 256, 128, 64, 4 * 1024 * 1024, 4, 128, 64, 4 * 1024 * 1024, 32 * 1024 * 1024, desired},
                  {false, 100, 10000, c::SubscriptionSelectorPolicy::CANONICAL_EXACT_V1}, limits, turn);
    INFO("factory error ", result ? -1 : static_cast<int>(result.error().error));

    if (!result && result.error().source)
        INFO("nested ", std::visit(
                            [](auto e) {
                                return static_cast<int>(e);
                            },
                            *result.error().source));

    REQUIRE(result);

    return std::move(*result);
}

/** Callback observations are application-owned copies; no protocol owner escapes. */
struct QueueRecorder final : m::MuxConsumer {
    std::vector<std::int64_t> values;    ///< Exact ordered scalar observations.
    std::vector<std::u16string> symbols; ///< Actual concrete symbols, including Penta DATA star.

    /** Copy the complete borrowed batch before returning.
     * @param batch Immutable callback borrow, expiring on return.
     * @return Continue future callbacks.
     */
    m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
        for (const auto &record : batch.records()) {
            REQUIRE(record.typedValues.size() == 3);
            values.push_back(record.typedValues[2].raw);
            symbols.emplace_back(record.symbol);
        }

        return m::BatchAction::CONTINUE;
    }
};

/** Run a bounded deterministic host loop, never waiting on wall time.
 * @param owner Serialized composition.
 * @param ready Observable condition.
 */
void pump(m::MuxQueue &owner, const std::function<bool()> &ready) {
    for (unsigned i = 0; i < 20000; ++i) {
        if (ready())
            return;

        auto turn = queueTurn();
        auto result = owner.progress(turn);
        INFO("queue progress error ", result ? -1 : static_cast<int>(result.error().error));
        REQUIRE(result);
    }

    FAIL("bounded owner progress did not reach the condition");
}

/** Open a genuinely empty registration before ordinary command submission.
 * @param owner Actual queue owner.
 * @param recorder Application callback.
 * @return Producer-safe public handle.
 */
m::MuxHandle open(m::MuxQueue &owner, const std::shared_ptr<m::MuxConsumer> &recorder) {
    auto turn = queueTurn();
    auto result =
        owner.open({32, 512 * 1024, 8, 128 * 1024, 4, 128, 128, 1024, 4096, 8192, 4, 8, 64, 4096}, recorder, turn);
    REQUIRE(result);

    return std::move(*result);
}

/** Apply explicit desired selectors and wait for actual synchronization.
 * @param owner Actual owner.
 * @param handle Registration handle.
 * @param requests Borrowed command payload.
 */
void add(m::MuxQueue &owner, m::MuxHandle &handle, std::span<const c::SubscriptionRequest> requests) {
    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto ticket = handle.add(requests, work);
    REQUIRE(ticket);
    pump(owner, [&] {
        return ticket->result().has_value();
    });
    CHECK(ticket->result()->status == m::CommandStatus::APPLIED);
    pump(owner, [&] {
        return handle.state() == m::QueueClientState::ACTIVE;
    });
}

/** Apply one explicit removal barrier.
 * @param owner Actual owner.
 * @param handle Registration handle.
 * @param key Original selector, never a decoded source text surrogate.
 */
void remove(m::MuxQueue &owner, m::MuxHandle &handle, const c::SubscriptionKey &key) {
    m::QueueSubmitWork work{10000, 1000000, 1000000, 1000000};
    auto ticket = handle.remove(std::span(&key, 1), work);
    REQUIRE(ticket);
    pump(owner, [&] {
        return ticket->result().has_value();
    });
    CHECK(ticket->result()->status == m::CommandStatus::APPLIED);
    pump(owner, [&] {
        return handle.state() == m::QueueClientState::ACTIVE;
    });
}

/** Transfer an actual complete decoder owner only when the composition is ready.
 * @param owner Actual queue.
 * @param live Serialized source session.
 * @param inputs Exact materialized source operands.
 * @param type DATA family.
 */
void send(m::MuxQueue &owner, p::LiveSession &live, std::span<const Input> inputs, std::int32_t type = 15) {
    auto event = admit(live, sourceFrame(inputs, type));

    for (unsigned i = 0; i < 20000; ++i) {
        auto turn = queueTurn();
        auto result = owner.admit(std::move(event), turn);

        if (result) {
            CHECK(result->kind == m::AdmissionKind::DATA);

            return;
        }

        REQUIRE(result.error().error == m::QueueError::BUSY);
        auto progressed = owner.progress(turn);
        REQUIRE(progressed);
    }

    FAIL("bounded source handoff remained busy");
}
} // namespace

TEST_CASE("wildcard actual queue preserves Stream duplicates and one projection for Exact plus All across owners") {
    auto local = schema();
    auto live = session(local);
    auto owner = queue(local, c::SubscriptionFamily::STREAM);
    auto a = std::make_shared<QueueRecorder>(), b = std::make_shared<QueueRecorder>();
    auto first = open(owner, a), second = open(owner, b);
    const auto all = c::allSymbols(local.recordContext(0));
    const c::SubscriptionRequest requests[]{{all, {}}, {{local.recordContext(0), u"ALPHA"}, {}}};
    add(owner, first, requests);
    add(owner, second, std::span(requests, 1));
    CHECK(owner.aggregateEntries().size() == 2);
    const Input initial[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}, {0, u"BETA", 2, 20}, {1, u"ALPHA", 3, 99}};
    send(owner, live, initial);
    pump(owner, [&] {
        return a->values.size() == 3 && b->values.size() == 3;
    });
    CHECK(a->values == std::vector<std::int64_t>{10, 10, 20});
    CHECK(b->values == a->values);
    const Input future[]{{0, u"FUTURE42", 4, 40}, {0, u"*", 5, 50}};
    send(owner, live, future);
    pump(owner, [&] {
        return a->values.size() == 5 && b->values.size() == 5;
    });
    CHECK(a->symbols.back() == u"*");
    CHECK(b->symbols.back() == u"*");
    remove(owner, first, all);
    CHECK(owner.aggregateEntries().size() == 2);
    const Input afterFirst[]{{0, u"ALPHA", 6, 60}, {0, u"BETA", 6, 61}};
    send(owner, live, afterFirst);
    pump(owner, [&] {
        return a->values.size() == 6 && b->values.size() == 7;
    });
    CHECK(a->values.back() == 60);
    CHECK(b->values.back() == 61);
    remove(owner, second, all);
    CHECK(owner.aggregateEntries().size() == 1);
    const Input afterLast[]{{0, u"ALPHA", 7, 70}, {0, u"BETA", 7, 71}};
    send(owner, live, afterLast);
    pump(owner, [&] {
        return a->values.size() == 7;
    });
    CHECK(a->values.back() == 70);
    CHECK(b->values.size() == 7);
}

TEST_CASE("wildcard canonical bad source domain is accepted Unstaged then truthfully fails the shared composition") {
    for (const auto family :
         {c::SubscriptionFamily::STREAM, c::SubscriptionFamily::TICKER, c::SubscriptionFamily::HISTORY}) {
        CAPTURE(static_cast<int>(family));
        auto local = schema();
        auto live = session(local);
        auto owner = queue(local, family);
        auto recorder = std::make_shared<QueueRecorder>();
        auto handle = open(owner, recorder);
        const c::SubscriptionRequest request{{local.recordContext(0), u"ALPHA"},
                                             family == c::SubscriptionFamily::HISTORY ? std::optional<std::int64_t>{0}
                                                                                      : std::nullopt};
        add(owner, handle, std::span(&request, 1));
        const Input invalid[]{{0, u"ALPHA", 3, 30, family == c::SubscriptionFamily::HISTORY ? 4 : 0, false}};
        send(owner, live, invalid,
             family == c::SubscriptionFamily::STREAM   ? 15
             : family == c::SubscriptionFamily::TICKER ? 10
                                                       : 20);
        CHECK_FALSE(owner.terminal());
        pump(owner, [&] {
            return owner.terminal().has_value();
        });
        const auto terminal = *owner.terminal();
        CHECK(terminal.outcome == m::TerminalOutcome::FAILED);
        REQUIRE(terminal.primary);
        CHECK(terminal.primary->error == m::QueueError::SOURCE_FAILURE);
        REQUIRE(terminal.primary->source);
        CHECK(terminal.counts.acceptedRecords == 1);
        CHECK(terminal.counts.sourceRecordsDiscarded == 1);
        CHECK(terminal.hadGap);
        CHECK(recorder->values.empty());
        const bool actualDomain = std::visit(
            [](const auto error) {
                using T = std::remove_cv_t<decltype(error)>;

                if constexpr (std::is_same_v<T, col::StreamError>)
                    return error == col::StreamError::UNSUPPORTED_SYMBOL_DOMAIN;
                else if constexpr (std::is_same_v<T, col::TickerError>)
                    return error == col::TickerError::UNSUPPORTED_SYMBOL_DOMAIN;
                else if constexpr (std::is_same_v<T, col::HistoryError>)
                    return error == col::HistoryError::UNSUPPORTED_SYMBOL_DOMAIN;
                else
                    return false;
            },
            *terminal.primary->source);
        CHECK(actualDomain);
    }
}

TEST_CASE("wildcard compound detached batch retains full nullable UTF16 opaque and indexed payload after owners die") {
    /** Caller retains only already detached immutable backing. */
    struct Snapshot final : m::MuxConsumer {
        m::OwnedMuxBatch saved; ///< No protocol owner or borrowed field span.

        /** Retain prepared backing without allocating.
         * @param batch Callback-only immutable borrow.
         * @return Continue future callbacks.
         */
        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            saved = batch.retain();

            return m::BatchAction::CONTINUE;
        }
    };

    auto snapshot = std::make_shared<Snapshot>();
    const std::u16string symbol{u'S', char16_t{0}, char16_t{0xd800}};
    const std::u16string chars{char16_t{0}, u'A', char16_t{0xd800}};
    {
        const c::LocalRecord definition{0,
                                        u"Generic#All",
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
        auto live = genericSession(*local);
        auto owner = queue(*local, c::SubscriptionFamily::STREAM);
        auto handle = open(owner, snapshot);
        const c::SubscriptionRequest requests[]{{c::allSymbols(local->recordContext(0)), {}},
                                                {{local->recordContext(0), symbol}, {}}};
        add(owner, handle, requests);
        std::vector<std::byte> descriptor;
        number(descriptor, 17);
        text(descriptor, definition.name);
        number(descriptor, 7);

        for (std::size_t i = 0; i < 7; ++i) {
            text(descriptor, definition.lookups[i].name);
            number(descriptor, definition.fields[i].serialTypeId);
        }

        (void)admit(live, frame(2, descriptor));
        std::vector<std::byte> body{std::byte{0xfd}};
        number(body, static_cast<std::int64_t>(symbol.size()));

        for (const auto unit : symbol) {
            std::array<std::byte, 4> encoded{};
            auto r = e::writeUtfChar(unit, encoded);
            REQUIRE(r.status == e::Status::OK);
            body.insert(body.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(r.written));
        }

        number(body, 17);
        number(body, 7);
        std::array<std::byte, 128> encoded{};
        auto r = e::writeCharArray(chars, encoded, {128, 32});
        REQUIRE(r.status == e::Status::OK);
        body.insert(body.end(), encoded.begin(), encoded.begin() + static_cast<std::ptrdiff_t>(r.written));
        number(body, -1);
        number(body, 0);
        number(body, 3);
        body.insert(body.end(), {std::byte{0}, std::byte{0xff}, std::byte{1}});
        number(body, -1);
        number(body, -5);
        auto event = admit(live, frame(15, body));
        REQUIRE(event.records().size() == 1);
        auto turn = queueTurn();
        REQUIRE(owner.admit(std::move(event), turn));
        pump(owner, [&] {
            return !snapshot->saved.records().empty();
        });
        CHECK(snapshot->saved.records().size() == 1);
    }
    auto saved = std::move(snapshot->saved);
    snapshot.reset();
    bool exact = false;
    std::thread reader([batch = std::move(saved), &exact, &symbol, &chars]() mutable {
        const auto &record = batch.records()[0];
        const auto &fields = record.typedValues;
        exact = record.symbol == symbol && fields.size() == 7 && fields[0].raw == 7 && fields[1].units &&
                *fields[1].units == chars && !fields[2].units && fields[3].bytes && fields[3].bytes->empty() &&
                fields[4].bytes && fields[4].bytes->size() == 3 && (*fields[4].bytes)[1] == std::byte{0xff} &&
                !fields[5].bytes && fields[6].raw == -5;
        batch = {};
    });
    reader.join();
    CHECK(exact);
}

TEST_CASE(
    "wildcard actual canonical owner trace emits decoded inputs and projected outputs for production comparison") {
    auto local = schema();
    auto live = session(local);
    auto owner = queue(local, c::SubscriptionFamily::STREAM);

    /** Copy only actual public data, preserving the complete selected semantic tuple. */
    struct Observer final : m::MuxConsumer {
        const c::LocalSchema &schema;  ///< Caller keeps this schema alive through all callbacks.
        std::vector<std::string> rows; ///< Detached scalar/text observations.

        /** Retain no protocol or callback data beyond the call.
         * @param local Caller-owned immutable schema.
         */
        explicit Observer(const c::LocalSchema &local) : schema(local) {
        }

        /** Observe actual queue output without supplying expected values.
         * @param batch Whole immutable callback borrow.
         * @return Continue future callbacks.
         */
        m::BatchAction onBatch(const m::BorrowedMuxBatch &batch) override {
            for (const auto &record : batch.records()) {
                REQUIRE(record.source);
                REQUIRE(record.typedValues.size() == 3);
                auto name = std::u16string_view{};

                for (const auto &definition : schema.records())
                    if (record.context.matches(schema.recordContext(definition.id)))
                        name = definition.name;

                REQUIRE_FALSE(name.empty());
                const auto bits =
                    (static_cast<std::uint64_t>(static_cast<std::uint32_t>(record.typedValues[0].raw)) << 32) |
                    static_cast<std::uint32_t>(record.typedValues[1].raw);
                std::ostringstream row;
                row << units(name) << '\t' << record.source->cipher << '\t' << units(record.symbol) << '\t'
                    << std::bit_cast<std::int64_t>(bits) << '\t' << record.typedValues[2].raw << '\t'
                    << record.eventFlags;
                rows.push_back(row.str());
            }

            return m::BatchAction::CONTINUE;
        }
    };

    auto a = std::make_shared<Observer>(local), b = std::make_shared<Observer>(local);
    auto first = open(owner, a), second = open(owner, b);
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}},
        exact{{local.recordContext(0), u"ALPHA"}, {}};
    add(owner, first, std::span(&all, 1));
    add(owner, second, std::span(&all, 1));
    add(owner, first, std::span(&exact, 1));
    const auto directory = std::filesystem::path(QDCXX_WILDCARD_OUTPUT);
    std::filesystem::create_directories(directory);
    std::ofstream trace(directory / "canonical-owners.tsv", std::ios::binary);
    REQUIRE(trace);
    trace << "phase\tindex\trecord\tcipher\tdecoded-symbol\traw-time\tvalue\tflags\n";
    /** Record actual decoder output before transferring its complete owner.
     * @param phase Fixed named production input phase.
     * @param input Fixed source operand, encoded by the actual native writer primitives.
     */
    const auto sendObserved = [&](const char *phase, Input input) {
        auto event = admit(live, sourceFrame(std::span(&input, 1)));
        REQUIRE(event.records().size() == 1);
        const auto &record = event.records()[0];
        REQUIRE(record.typedValues.size() == 3);
        auto definition = std::ranges::find_if(local.records(), [&](const auto &r) {
            return r.id == record.localId;
        });
        REQUIRE(definition != local.records().end());
        const auto bits = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(record.typedValues[0].raw)) << 32) |
                          static_cast<std::uint32_t>(record.typedValues[1].raw);
        trace << phase << "\t0\t" << units(definition->name) << '\t' << record.cipher << '\t' << units(record.symbol)
              << '\t' << std::bit_cast<std::int64_t>(bits) << '\t' << record.typedValues[2].raw << '\t'
              << record.eventFlags << '\n';

        for (unsigned i = 0; i < 20000; ++i) {
            auto turn = queueTurn();
            auto admitted = owner.admit(std::move(event), turn);
            INFO("phase ", phase, " attempt ", i, " resident ", event.residentBytes(), " type ", event.messageType());

            if (admitted)
                return;

            REQUIRE(admitted.error().error == m::QueueError::BUSY);
            REQUIRE(owner.progress(turn));
        }

        FAIL("actual trace source handoff remained busy");
    };
    /** Emit complete actual phase rows/count, never expected fixture values.
     * @param phase Actual production output phase.
     * @param observer Caller-owned observations.
     * @param from Previous output cutoff for this observer.
     */
    const auto emit = [&](const char *phase, const Observer &observer, std::size_t from) {
        for (std::size_t i = from; i < observer.rows.size(); ++i)
            trace << phase << '\t' << (i - from) << '\t' << observer.rows[i] << '\n';

        trace << phase << "\t-1\tCOUNT\t" << (observer.rows.size() - from) << "\t-\t-\t-\t-\n";
    };
    sendObserved("identical-0.data-parsed", {0, u"ALPHA", 1, 20});
    sendObserved("identical-1.data-parsed", {0, u"ALPHA", 1, 20});
    sendObserved("beta.data-parsed", {0, u"BETA", 1, 21});
    sendObserved("other-record.data-parsed", {1, u"ALPHA", 1, 99});
    pump(owner, [&] {
        return a->rows.size() >= 3 && b->rows.size() >= 3;
    });
    emit("first.a", *a, 0);
    emit("first.b", *b, 0);
    auto fromA = a->rows.size(), fromB = b->rows.size();
    sendObserved("future-symbol.data-parsed", {0, u"FUTURE42", 1, 22});
    pump(owner, [&] {
        return a->rows.size() > fromA && b->rows.size() > fromB;
    });
    emit("future.a", *a, fromA);
    emit("future.b", *b, fromB);
    fromA = a->rows.size();
    fromB = b->rows.size();
    remove(owner, first, all.key);
    sendObserved("after-first-remove-alpha.data-parsed", {0, u"ALPHA", 1, 23});
    sendObserved("after-first-remove-beta.data-parsed", {0, u"BETA", 1, 24});
    pump(owner, [&] {
        return a->rows.size() > fromA && b->rows.size() >= fromB + 2;
    });
    emit("after-first.a", *a, fromA);
    emit("after-first.b", *b, fromB);
    fromA = a->rows.size();
    fromB = b->rows.size();
    remove(owner, second, all.key);
    sendObserved("after-last-remove-alpha.data-parsed", {0, u"ALPHA", 1, 25});
    sendObserved("after-last-remove-beta.data-parsed", {0, u"BETA", 1, 26});
    pump(owner, [&] {
        return a->rows.size() > fromA;
    });

    for (unsigned i = 0; i < 32; ++i) {
        auto turn = queueTurn();
        REQUIRE(owner.progress(turn));
    }

    emit("after-last.a", *a, fromA);
    emit("after-last.b", *b, fromB);
    trace.close();
    REQUIRE(trace);
}

TEST_CASE("wildcard standalone ExactOnly dispatcher preserves kind through both command copy paths") {
    namespace d = org::ttldtor::qdcxx::client;

    /** No DATA is submitted; any callback would violate this command-only case. */
    struct NoData final : d::Consumer {
        /** Refuse an unexpected invocation.
         * @param batch Unused forbidden data borrow.
         */
        void onBatch(const d::BorrowedBatch &batch) override {
            (void)batch;
            FAIL("command-only dispatcher invoked a callback");
        }
    };

    /** Supply fresh explicit finite owner credits.
     * @return Independent desired/collector/client allowance.
     */
    const auto turn = []() {
        return d::DispatchWork{10000,
                               10000,
                               10000,
                               1000000,
                               1000000,
                               1000000,
                               100000000,
                               {10000, 1000000, 1000000},
                               {10000, 1000000, 1000000, 100000000}};
    };
    auto local = schema();
    auto consumer = std::make_shared<NoData>();
    auto work = turn();
    auto owner = d::StreamDispatcher::create(
        local, {16, 32, 128, 2048, 64, 4096, 32}, {16, 128, 2 * 1024 * 1024, 4 * 1024 * 1024},
        {4, 8, 8, 128, 2, 64, 128, 512, 1024, 16384, 4, 131072, 1024 * 1024}, consumer, work);
    REQUIRE(owner);
    auto handle = owner->handle();
    REQUIRE(handle);
    /** Wait for the actual once-published numeric completion.
     * @param ticket Already accepted copied command.
     * @return Actual immutable command result.
     */
    const auto complete = [&](const d::CommandTicket &ticket) {
        for (unsigned i = 0; i < 1000; ++i) {
            if (auto result = ticket.result())
                return *result;

            auto credits = turn();
            REQUIRE(owner->progress(credits));
        }

        FAIL("finite command did not complete");

        return d::CommandResult{};
    };
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
    d::SubmitWork submit{10000, 1000000, 1000000};
    auto addition = handle->add(std::span(&all, 1), submit);
    REQUIRE(addition);
    auto rejected = complete(*addition);
    CHECK(rejected.status == d::CommandStatus::REJECTED);
    REQUIRE(rejected.error);
    CHECK(*rejected.error == c::SubscriptionError::UNSUPPORTED_SELECTOR);
    REQUIRE(owner->desired());
    CHECK(owner->desired()->entries().empty());
    const c::SubscriptionRequest empty{{local.recordContext(0), u""}, {}};
    submit = {10000, 1000000, 1000000};
    auto exact = handle->add(std::span(&empty, 1), submit);
    REQUIRE(exact);
    CHECK(complete(*exact).status == d::CommandStatus::APPLIED);
    CHECK(owner->desired()->entries().size() == 1);
    submit = {10000, 1000000, 1000000};
    auto removal = handle->remove(std::span(&all.key, 1), submit);
    REQUIRE(removal);
    rejected = complete(*removal);
    CHECK(rejected.status == d::CommandStatus::REJECTED);
    REQUIRE(rejected.error);
    CHECK(*rejected.error == c::SubscriptionError::UNSUPPORTED_SELECTOR);
    CHECK(owner->desired()->entries().size() == 1);
}

TEST_CASE("wildcard cross-thread immutable singleton choice copy and release survives owner retirement") {
    auto local = schema();
    auto created = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(created);
    std::optional<c::SubscriptionState> owner(std::move(*created));
    const c::SubscriptionRequest requests[]{{c::allSymbols(local.recordContext(0)), {}},
                                            {{local.recordContext(0), u"ALPHA"}, {}}};
    auto turn = work();
    REQUIRE(owner->add(requests, turn));
    const auto all = owner->entries()[0].membership, exact = owner->entries()[1].membership;
    CHECK(owner->isCurrent(c::SubscriptionMembership::anyOf(all, exact)));
    std::latch arrived{2}, retired{1};
    std::atomic<unsigned> completed{0};
    /** Exercise only producer-safe immutable values; never touch the serialized owner.
     * @return No value; all copies are released on the worker.
     */
    const auto worker = [&, first = all, second = exact]() {
        std::array<c::SubscriptionMembership, 16> retained{};
        arrived.count_down();
        retired.wait();

        for (unsigned i = 0; i < 1000; ++i) {
            retained[i % retained.size()] = c::SubscriptionMembership::anyOf(first, second);
        }

        completed.fetch_add(1000);
    };
    std::thread a(worker), b(worker);
    arrived.wait();
    owner.reset();
    retired.count_down();
    a.join();
    b.join();
    CHECK(completed.load() == 2000);
}
#endif

TEST_CASE("wildcard All reserves no symbol universe and one exact entry slot obeys inclusive limits") {
    auto local = schema();
    auto limited = DESIRED;
    limited.entries = 1;
    limited.peakEntries = 2;
    limited.symbolUnits = 0;
    limited.totalSymbolUnits = 0;
    limited.peakSymbolUnits = 0;
    limited.commandEntries = 1;
    auto state = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, limited,
                                              c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(state);
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
    auto turn = work();
    REQUIRE(state->add(std::span(&all, 1), turn));
    CHECK(state->entries().size() == 1);
    const auto revision = state->revision();
    const auto token = state->entries()[0].membership;
    const c::SubscriptionRequest other{c::allSymbols(local.recordContext(1)), {}};
    turn = work();
    auto exceeded = state->add(std::span(&other, 1), turn);
    REQUIRE_FALSE(exceeded);
    CHECK(exceeded.error() == c::SubscriptionError::RESOURCE_LIMIT);
    CHECK(state->revision() == revision);
    CHECK(state->isCurrent(token));
    col::StreamWork stream{100000, 1000000000, 1000000000, 1000000000};
    auto collector = col::StreamCollector::create(std::move(*state), {2, 2, 4 * 1024 * 1024, 8 * 1024 * 1024}, stream);
    REQUIRE(collector);
    auto live = session(local);
    const Input initial[]{{0, u"FIRST_UNSEEN_SYMBOL", 1, 10}, {0, u"SECOND_UNSEEN_SYMBOL", 1, 11}};
    auto event = admit(live, sourceFrame(initial));
    auto stage = collector->stage(std::move(event), stream);
    REQUIRE(stage);
    CHECK(stage->selected == 2);
    CHECK(collector->desired()->entries().size() == 1);
    auto offered = collector->front(stream);
    REQUIRE(offered);
    REQUIRE(*offered);
    CHECK((**offered).record->typedValues[2].raw == 10);
    REQUIRE(collector->pop(stream));
    offered = collector->front(stream);
    REQUIRE(offered);
    REQUIRE(*offered);
    CHECK((**offered).record->typedValues[2].raw == 11);
    REQUIRE(collector->pop(stream));
    const Input literalStar[]{{0, u"*", 2, 12, 0, false}};
    auto bad = admit(live, sourceFrame(literalStar));
    const auto backing = bad.residentBytes();
    auto refused = collector->stage(std::move(bad), stream);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == col::StreamError::UNSUPPORTED_SYMBOL_DOMAIN);
    CHECK(bad.residentBytes() == backing);
}

TEST_CASE("wildcard explicit selectors reject malformed and unknown profiles without altering current demand") {
    auto local = schema();
    auto state = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                              c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(state);
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
    auto turn = work();
    REQUIRE(state->add(std::span(&all, 1), turn));
    const auto revision = state->revision();

    for (const c::SubscriptionKey key :
         {c::SubscriptionKey{local.recordContext(0), u"*"},
          c::SubscriptionKey{local.recordContext(0), u"NOT_EMPTY", c::SubscriptionSelectorKind::ALL},
          c::SubscriptionKey{local.recordContext(0), u"ALPHA", static_cast<c::SubscriptionSelectorKind>(99)}}) {
        const c::SubscriptionRequest bad{key, {}};
        turn = work();
        auto refused = state->add(std::span(&bad, 1), turn);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == c::SubscriptionError::INVALID_SELECTOR);
        CHECK(state->revision() == revision);
    }

    const auto unknown = static_cast<c::SubscriptionSelectorPolicy>(99);
    auto refused = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED, unknown);
    REQUIRE_FALSE(refused);
    CHECK(refused.error() == c::SubscriptionError::UNSUPPORTED_SELECTOR);
    auto foreign = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(foreign);
    turn = work();
    REQUIRE(foreign->add(std::span(&all, 1), turn));
    CHECK_FALSE(state->isCurrent(
        c::SubscriptionMembership::anyOf(state->entries()[0].membership, foreign->entries()[0].membership)));
}

TEST_CASE("wildcard queued admission survives either original then withdraws after both retire without resurrection") {
    for (const bool exactFirst : {false, true}) {
        auto local = schema();
        auto live = session(local);
        auto desired = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                    c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
        REQUIRE(desired);
        const c::SubscriptionRequest requests[]{{c::allSymbols(local.recordContext(0)), {}},
                                                {{local.recordContext(0), u"ALPHA"}, {}}};
        auto command = work();
        REQUIRE(desired->add(requests, command));
        col::StreamWork turn{100000, 1000000000, 1000000000, 1000000000};
        auto collector =
            col::StreamCollector::create(std::move(*desired), {2, 4, 4 * 1024 * 1024, 8 * 1024 * 1024}, turn);
        REQUIRE(collector);
        const Input input[]{{0, u"ALPHA", 1, 10}, {0, u"ALPHA", 1, 10}};
        auto event = admit(live, sourceFrame(input));
        REQUIRE(collector->stage(std::move(event), turn));
        const auto first = exactFirst ? 1 : 0, second = 1 - first;
        command = work();
        REQUIRE(collector->remove(std::span(&requests[first].key, 1), command));
        auto offered = collector->front(turn);
        REQUIRE(offered);
        REQUIRE(*offered);
        CHECK((**offered).record->typedValues[2].raw == 10);
        CHECK(collector->queuedRecords() == 2);
        command = work();
        REQUIRE(collector->remove(std::span(&requests[second].key, 1), command));
        command = work();
        REQUIRE(collector->add(std::span(&requests[first], 1), command));
        offered = collector->front(turn);
        REQUIRE(offered);
        CHECK_FALSE(*offered);
        CHECK(collector->counts().withdrawn == 2);
        CHECK(collector->queuedRecords() == 0);
        event = admit(live, sourceFrame(input));
        REQUIRE(collector->stage(std::move(event), turn));

        for (unsigned i = 0; i < 2; ++i) {
            offered = collector->front(turn);
            REQUIRE(offered);
            REQUIRE(*offered);
            REQUIRE(collector->pop(turn));
        }
    }
}

TEST_CASE("wildcard direct STREAM inclusive measured work boundary preserves whole input on one short then retries") {
    auto local = schema();
    /** Build a fresh identical actual state without moving the supplied source owner.
     * @return Fresh canonical All collector.
     */
    const auto make = [&]() {
        auto desired = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                    c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
        REQUIRE(desired);
        const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
        auto command = work();
        REQUIRE(desired->add(std::span(&all, 1), command));
        col::StreamWork work{100000, 1000000000, 1000000000, 1000000000};
        auto collector =
            col::StreamCollector::create(std::move(*desired), {2, 4, 4 * 1024 * 1024, 8 * 1024 * 1024}, work);
        REQUIRE(collector);

        return std::move(*collector);
    };
    const Input inputs[]{{0, u"ALPHA", 1, 10}, {0, u"FUTURE42", 2, 20}};
    auto baseline = make();
    auto live = session(local);
    auto event = admit(live, sourceFrame(inputs));
    const col::StreamWork before{100000, 1000000000, 1000000000, 1000000000};
    auto after = before;
    REQUIRE(baseline.stage(std::move(event), after));
    const col::StreamWork exact{before.operations - after.operations, before.visits - after.visits,
                                before.textUnits - after.textUnits, before.bytes - after.bytes};
    REQUIRE(exact.operations > 0);
    REQUIRE(exact.visits > 0);
    REQUIRE(exact.textUnits > 0);

    for (const unsigned axis : {0u, 1u, 2u}) {
        auto collector = make();
        auto source = session(local);
        auto incoming = admit(source, sourceFrame(inputs));
        const auto backing = incoming.residentBytes();
        auto shortWork = exact;

        if (axis == 0)
            --shortWork.operations;
        else if (axis == 1)
            --shortWork.visits;
        else
            --shortWork.textUnits;

        const auto refused = collector.stage(std::move(incoming), shortWork);
        REQUIRE_FALSE(refused);
        CHECK(refused.error() == col::StreamError::WORK_LIMIT);
        CHECK(incoming.residentBytes() == backing);
        CHECK(collector.queuedRecords() == 0);
        CHECK(collector.counts().staged == 0);
        auto retry = exact;
        auto admitted = collector.stage(std::move(incoming), retry);
        REQUIRE(admitted);
        CHECK(admitted->selected == 2);
    }
}

TEST_CASE("wildcard bounded unseen symbol workload reuses one cacheless Stream slot beyond per-frame quota") {
    auto local = schema();
    auto live = session(local);
    auto limits = DESIRED;
    limits.entries = 1;
    limits.peakEntries = 2;
    limits.symbolUnits = 0;
    limits.totalSymbolUnits = 0;
    limits.peakSymbolUnits = 0;
    auto desired = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, limits,
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(desired);
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
    auto command = work();
    REQUIRE(desired->add(std::span(&all, 1), command));
    col::StreamWork turn{100000, 1000000000, 1000000000, 1000000000};
    auto collector = col::StreamCollector::create(std::move(*desired), {1, 1, 4 * 1024 * 1024, 8 * 1024 * 1024}, turn);
    REQUIRE(collector);

    for (unsigned index = 0; index < 300; ++index) {
        const auto suffix = std::to_string(index);
        std::u16string symbol = u"UNSEEN-SYMBOL-";
        symbol.append(suffix.begin(), suffix.end());
        const Input input{0, symbol, index, static_cast<std::int32_t>(index)};
        auto event = admit(live, sourceFrame(std::span<const Input>(&input, 1)));
        auto admitted = collector->stage(std::move(event), turn);
        REQUIRE(admitted);
        CHECK(admitted->selected == 1);
        auto offered = collector->front(turn);
        REQUIRE(offered);
        REQUIRE(*offered);
        CHECK((**offered).record->symbol == symbol);
        CHECK((**offered).record->typedValues[2].raw == index);
        REQUIRE(collector->pop(turn));
        CHECK(collector->queuedRecords() == 0);
        CHECK(collector->stagedBytes() == 0);
        CHECK(collector->desired()->entries().size() == 1);
    }

    CHECK(collector->counts().staged == 300);
    CHECK_FALSE(collector->diagnostic().error);
}

TEST_CASE("wildcard actual Penta DATA star emits the identical production input output and count") {
    auto local = schema();
    auto live = session(local);
    auto desired = c::SubscriptionState::create(local, c::SubscriptionFamily::STREAM, DESIRED,
                                                c::SubscriptionSelectorPolicy::CANONICAL_STREAM_ALL_V1);
    REQUIRE(desired);
    const c::SubscriptionRequest all{c::allSymbols(local.recordContext(0)), {}};
    auto command = work();
    REQUIRE(desired->add(std::span(&all, 1), command));
    col::StreamWork turn{100000, 1000000000, 1000000000, 1000000000};
    auto collector = col::StreamCollector::create(std::move(*desired), {2, 4, 4 * 1024 * 1024, 8 * 1024 * 1024}, turn);
    REQUIRE(collector);
    const Input input{0, u"*", 1, 12, 0};
    auto event = admit(live, sourceFrame(std::span<const Input>(&input, 1), 15));
    REQUIRE(event.records().size() == 1);
    CHECK(event.records()[0].symbol == u"*");
    CHECK(event.records()[0].cipher != 0);
    std::filesystem::create_directories(QDCXX_WILDCARD_OUTPUT);
    std::ofstream trace(std::filesystem::path(QDCXX_WILDCARD_OUTPUT) / "penta-data-star.tsv", std::ios::binary);
    REQUIRE(trace);
    trace << "phase\tindex\trecord\tcipher\tdecoded-symbol\traw-time\tvalue\tflags\n";
    traceInput(trace, "penta-star-data.data-parsed", event.records()[0], local);
    auto staged = collector->stage(std::move(event), turn);
    REQUIRE(staged);
    CHECK(staged->selected == 1);
    std::size_t delivered = 0;

    for (;;) {
        auto front = collector->front(turn);
        REQUIRE(front);

        if (!*front)
            break;

        traceInput(trace, "penta-star.penta-all", *(**front).record, local, delivered++);
        REQUIRE(collector->pop(turn));
    }

    trace << "penta-star.penta-all\t-1\tCOUNT\t" << delivered << "\t-\t-\t-\t-\n";
    trace.close();
    REQUIRE(trace);
    CHECK(delivered == 1);
}
