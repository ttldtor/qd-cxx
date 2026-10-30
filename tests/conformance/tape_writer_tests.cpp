// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0

#define DOCTEST_CONFIG_IMPLEMENT
#include "qd/events/profile.hpp"
#include "qd/events/quote.hpp"
#include "qd/protocol/tape_writer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <new>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace allocation_test {
thread_local bool enabled = false;      ///< Fail only allocations in an explicitly armed library call.
thread_local std::size_t remaining = 0; ///< Successful allocation attempts permitted before the failure.

/** Allocate ordinary owning storage, with narrowly armed call-scoped failure injection.
 * @param size Requested bytes; zero still obtains a unique allocation.
 * @return Owned suitably aligned ordinary storage, or throw bad_alloc.
 */
void *allocate(std::size_t size) {
    if (enabled) {
        if (remaining == 0) {
            throw std::bad_alloc();
        }

        --remaining;
    }

    if (auto *result = std::malloc(size == 0 ? 1 : size)) {
        return result;
    }

    throw std::bad_alloc();
}
} // namespace allocation_test

/** Replace ordinary allocation solely to exercise actual library owning boundaries.
 * @param size Requested bytes.
 * @return Owned ordinary storage or bad_alloc.
 */
void *operator new(std::size_t size) {
    return allocation_test::allocate(size);
}

/** Replace ordinary array allocation for the same scoped fault boundary.
 * @param size Requested bytes.
 * @return Owned ordinary storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return allocation_test::allocate(size);
}

/** Release ordinary storage paired with this test executable's allocation replacement.
 * @param value Previously allocated ordinary storage or null.
 */
void operator delete(void *value) noexcept {
    std::free(value);
}

/** Release ordinary array storage paired with the test allocation replacement.
 * @param value Previously allocated ordinary array storage or null.
 */
void operator delete[](void *value) noexcept {
    std::free(value);
}

/** Release sized ordinary storage; size does not affect the owning allocation.
 * @param value Previously allocated storage or null.
 * @param size Supplied allocation size.
 */
void operator delete(void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

/** Release sized ordinary array storage.
 * @param value Previously allocated array storage or null.
 * @param size Supplied allocation size.
 */
void operator delete[](void *value, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(value);
}

namespace {
namespace p = org::ttldtor::qdcxx::protocol;
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace events = org::ttldtor::qdcxx::events;
std::filesystem::path fixtures;                ///< Explicit immutable contract fixture directory from CTest.
std::filesystem::path emitDirectory;           ///< Optional caller-selected interoperability output directory.
constexpr std::size_t FIXTURE_BYTES = 65536;   ///< Finite test-only JSON/TSV document allowance.
constexpr std::size_t FIXTURE_ELEMENTS = 4096; ///< Finite test-only JSON value count.
constexpr std::size_t FIXTURE_DEPTH = 16;      ///< Finite test-only JSON recursion allowance.

/** Strict test-only ASCII JSON value; no production loader or third-party dependency. */
struct Json {
    using Array = std::vector<Json>;            ///< Ordered owning fixture elements.
    using Object = std::map<std::string, Json>; ///< Unique owning ASCII fixture keys.
    std::variant<std::monostate, bool, std::int64_t, std::string, Array, Object> value; ///< Exact owning fixture value.

    /** Inspect one required fixture key.
     * @param key Exact ASCII key.
     * @return Borrowed value, or throw for malformed/missing fixture content.
     */
    const Json &at(std::string_view key) const {
        return std::get<Object>(value).at(std::string(key));
    }

    /** Inspect fixture text.
     * @return Borrowed exact ASCII text or a fixture type error.
     */
    const std::string &text() const {
        return std::get<std::string>(value);
    }

    /** Inspect a JSON integer, without floating-point conversion.
     * @return Exact signed fixture integer.
     */
    std::int64_t number() const {
        return std::get<std::int64_t>(value);
    }

    /** Inspect a JSON array.
     * @return Borrowed ordered fixture elements.
     */
    const Array &array() const {
        return std::get<Array>(value);
    }

    /** Inspect null distinctly from empty text/array.
     * @return Whether the fixture value is JSON null.
     */
    bool null() const {
        return std::holds_alternative<std::monostate>(value);
    }
};

/** Parse only the reviewed fixture grammar with finite depth/bytes/elements. */
class JsonParser {
    std::string_view input_;
    std::size_t position_ = 0;
    std::size_t elements_ = 0;

    /** Skip ASCII JSON whitespace without reading beyond input. */
    void whitespace() {
        while (position_ < input_.size() && (input_[position_] == ' ' || input_[position_] == '\n' ||
                                             input_[position_] == '\r' || input_[position_] == '\t')) {
            ++position_;
        }
    }

    /** Consume one exact punctuation byte.
     * @param expected Required byte.
     */
    void punctuation(char expected) {
        whitespace();

        if (position_ == input_.size() || input_[position_++] != expected) {
            throw std::runtime_error("fixture JSON punctuation");
        }
    }

    /** Read exact ASCII fixture text with ordinary escaped punctuation/control bytes.
     * @return Owning text; unsupported escape/non-ASCII input is a fixture error.
     */
    std::string string() {
        punctuation('"');
        std::string result;

        while (position_ < input_.size()) {
            char unit = input_[position_++];

            if (unit == '"') {
                return result;
            }

            if (unit == '\\') {
                if (position_ == input_.size()) {
                    throw std::runtime_error("fixture JSON escape");
                }

                unit = input_[position_++];

                if (unit == 'n') {
                    unit = '\n';
                } else if (unit == 'r') {
                    unit = '\r';
                } else if (unit == 't') {
                    unit = '\t';
                } else if (unit != '"' && unit != '\\' && unit != '/') {
                    throw std::runtime_error("fixture JSON unsupported escape");
                }
            } else if (static_cast<unsigned char>(unit) < 0x20 || static_cast<unsigned char>(unit) >= 0x80) {
                throw std::runtime_error("fixture JSON non-ASCII string");
            }

            result.push_back(unit);
        }

        throw std::runtime_error("fixture JSON unterminated text");
    }

    /** Parse one finite fixture value.
     * @param depth Current nesting depth.
     * @return Owning exact value; malformed input is never silently skipped.
     */
    Json value(std::size_t depth) {
        whitespace();

        if (depth > FIXTURE_DEPTH || ++elements_ > FIXTURE_ELEMENTS || position_ == input_.size()) {
            throw std::runtime_error("fixture JSON limit/truncation");
        }

        const char first = input_[position_];

        if (first == '"') {
            return {string()};
        }

        if (first == '[' || first == '{') {
            ++position_;
            Json::Array array;
            Json::Object object;
            whitespace();
            const char last = first == '[' ? ']' : '}';

            if (position_ < input_.size() && input_[position_] != last) {
                while (true) {
                    if (first == '[') {
                        array.push_back(value(depth + 1));
                    } else {
                        auto key = string();
                        punctuation(':');

                        if (!object.emplace(std::move(key), value(depth + 1)).second) {
                            throw std::runtime_error("fixture JSON duplicate key");
                        }
                    }

                    whitespace();

                    if (position_ == input_.size() || input_[position_] != ',') {
                        break;
                    }

                    ++position_;
                }
            }

            punctuation(last);

            return first == '[' ? Json{std::move(array)} : Json{std::move(object)};
        }

        for (const auto literal : {std::string_view("null"), std::string_view("true"), std::string_view("false")}) {
            if (input_.substr(position_, literal.size()) == literal) {
                position_ += literal.size();

                return literal == "null" ? Json{} : Json{literal == "true"};
            }
        }

        const auto start = position_;

        if (first == '-') {
            ++position_;
        }

        while (position_ < input_.size() && input_[position_] >= '0' && input_[position_] <= '9') {
            ++position_;
        }

        std::int64_t number = 0;
        const auto result = std::from_chars(input_.data() + start, input_.data() + position_, number);

        if (result.ec != std::errc{} || result.ptr != input_.data() + position_) {
            throw std::runtime_error("fixture JSON integer");
        }

        return {number};
    }

    public:
    /** Borrow one bounded fixture document.
     * @param input Complete caller-owned text.
     */
    explicit JsonParser(std::string_view input) : input_(input) {
        if (input.size() > FIXTURE_BYTES) {
            throw std::runtime_error("fixture JSON document limit");
        }
    }

    /** Parse exactly one JSON value, refusing trailing content.
     * @return Complete owning fixture tree.
     */
    Json parse() {
        auto result = value(0);
        whitespace();

        if (position_ != input_.size()) {
            throw std::runtime_error("fixture JSON trailing bytes");
        }

        return result;
    }
};

/** Read a finite immutable UTF-8/ASCII fixture.
 * @param name Relative fixture filename.
 * @return Owning complete text, refusing failed/open/oversized reads.
 */
std::string fixtureText(std::string_view name) {
    std::ifstream input(fixtures / name, std::ios::binary);

    if (!input) {
        throw std::runtime_error("fixture open failure");
    }

    std::string result;
    char unit = 0;

    while (input.get(unit)) {
        if (result.size() == FIXTURE_BYTES) {
            throw std::runtime_error("fixture byte limit");
        }

        result.push_back(unit);
    }

    if (!input.eof()) {
        throw std::runtime_error("fixture read failure");
    }

    return result;
}

/** Read one reviewed JSON contract document.
 * @param name Relative filename.
 * @return Complete strictly parsed owning tree.
 */
Json fixtureJson(std::string_view name) {
    const auto text = fixtureText(name);

    return JsonParser(text).parse();
}

/** Parse exact signed decimal TSV/raw text.
 * @param text Complete decimal cell.
 * @return Signed64 value or fixture error.
 */
std::int64_t decimal(std::string_view text) {
    std::int64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);

    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::runtime_error("fixture decimal");
    }

    return value;
}

/** Convert ASCII fixture names into exact UTF-16.
 * @param text Borrowed ASCII name.
 * @return Owned units.
 */
std::u16string ascii(std::string_view text) {
    return {text.begin(), text.end()};
}

/** Decode fixture hexadecimal UTF-16, preserving every unit.
 * @param text Exact four-digit units; empty/- means empty.
 * @return Owning units or fixture error.
 */
std::u16string units(std::string_view text) {
    if (text == "-" || text == "empty") {
        return {};
    }

    if (text.size() % 4 != 0) {
        throw std::runtime_error("fixture units width");
    }

    std::u16string result;

    for (std::size_t i = 0; i < text.size(); i += 4) {
        unsigned value = 0;
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 4, value, 16);

        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 4) {
            throw std::runtime_error("fixture units hex");
        }

        result.push_back(static_cast<char16_t>(value));
    }

    return result;
}

/** Encode exact UTF-16 into the interoperability row grammar.
 * @param value Borrowed exact units.
 * @return Lowercase hex, or '-' for empty.
 */
std::string hex(std::u16string_view value) {
    constexpr std::string_view DIGITS = "0123456789abcdef";
    std::string result;

    for (const auto unit : value) {
        for (int shift = 12; shift >= 0; shift -= 4) {
            result.push_back(DIGITS[(unit >> shift) & 15]);
        }
    }

    return result.empty() ? "-" : result;
}

/** Read finite reviewed TSV rows, preserving empty cells.
 * @param name Relative fixture filename.
 * @return Ordered rows after one required header.
 */
std::vector<std::vector<std::string>> rows(std::string_view name) {
    std::istringstream input(fixtureText(name));
    std::vector<std::vector<std::string>> result;
    std::string line;
    REQUIRE(static_cast<bool>(std::getline(input, line)));

    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::vector<std::string> cells;
        std::size_t start = 0;

        while (true) {
            const auto end = line.find('\t', start);
            cells.push_back(line.substr(start, end == std::string::npos ? end : end - start));

            if (end == std::string::npos) {
                break;
            }

            start = end + 1;
        }

        result.push_back(std::move(cells));
    }

    return result;
}

/** Construct the exact local declarations from the schema contract fixture.
 * @param input Parsed schema.json tree.
 * @return Validated owned extended-field schema.
 */
c::LocalSchema schema(const Json &input) {
    REQUIRE(input.at("formatVersion").number() == 1);
    std::vector<c::LocalRecord> records;

    for (const auto &entry : input.at("records").array()) {
        c::LocalRecord record{static_cast<std::int32_t>(entry.at("id").number()),
                              ascii(entry.at("name").text()),
                              std::get<bool>(entry.at("hasTime").value),
                              {},
                              {}};

        for (const auto &field : entry.at("fields").array()) {
            record.fields.push_back({static_cast<std::int32_t>(field.at("serialTypeId").number())});
        }

        for (const auto &key : entry.at("lookups").array()) {
            record.lookups.push_back(
                {ascii(key.at("name").text()), static_cast<std::size_t>(key.at("fieldId").number())});
        }

        records.push_back(std::move(record));
    }

    auto result = c::LocalSchema::create(records, {100, 1000, 1000, 1000, 10000}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Extract explicit layouts rather than inferring the first lookup alias.
 * @param input Parsed writer-cases fixture.
 * @return Owning supplied declarations.
 */
std::vector<p::OutgoingRecord> layouts(const Json &input) {
    std::vector<p::OutgoingRecord> result;

    for (const auto &entry : input.at("layouts").array()) {
        p::OutgoingRecord record{static_cast<std::int32_t>(entry.at("senderId").number()),
                                 static_cast<std::int32_t>(entry.at("localRecordId").number()),
                                 ascii(entry.at("wireName").text()),
                                 {}};

        for (const auto &field : entry.at("fields").array()) {
            record.fields.push_back({ascii(field.at("wireName").text()),
                                     static_cast<std::int32_t>(field.at("serialTypeId").number()),
                                     static_cast<std::int32_t>(field.at("localFieldId").number())});
        }

        result.push_back(std::move(record));
    }

    return result;
}

/** Supply finite generous construction allowances for ordinary witnesses.
 * @return Explicit layout/schema budgets.
 */
p::LayoutLimits layoutLimits() {
    return {{100, 1000, 1000, 1000, 10000}, 100, 1000, 1000, 10000};
}

/** Supply finite generous operation allowances for concrete test witnesses.
 * @return Explicit writer budgets, never an unlimited sentinel.
 */
p::WriterLimits writerLimits() {
    return {65536,   1000000, 1000,  10000,   100000, {10000, 10000}, 1000000, {10000, 10000},
            1000000, 1000000, 10000, 1000000, 65545,  10000,          10000000};
}

/** Decode exact represented outgoing descriptor text.
 * @param input Parsed descriptor fixture object.
 * @return Owned descriptor with absent/empty distinctions preserved.
 */
p::OutgoingDescriptor descriptor(const Json &input) {
    p::OutgoingDescriptor result{
        {}, {}, ascii(input.at("time").text()), std::get<bool>(input.at("explicitRawSend").value)};

    if (!input.at("version").null()) {
        result.version = ascii(input.at("version").text());
    }

    if (!input.at("options").null()) {
        result.options = ascii(input.at("options").text());
    }

    return result;
}

/** Owned test inputs keep all borrowed record spans/views stable. */
struct Record {
    std::int32_t senderId;             ///< Exact wire identity.
    std::u16string symbol;             ///< Owned exact units for the call.
    std::vector<c::FieldValue> values; ///< Owned local-order tagged values.
    std::int32_t flags = 0;            ///< Raw caller flag input.

    /** Borrow one record without extending its storage lifetime.
     * @return Call-scoped view.
     */
    p::OutgoingRecordView view() const {
        return {senderId, symbol, values, flags};
    }
};

/** Extract exact owned values from the JSON witness.
 * @param input Complete writer-cases fixture.
 * @return Stable owning records in fixture order.
 */
std::vector<Record> records(const Json &input) {
    std::vector<Record> result;

    for (const auto &entry : input.at("records").array()) {
        Record record{static_cast<std::int32_t>(entry.at("senderId").number()),
                      units(entry.at("symbolUnits").text()),
                      {},
                      static_cast<std::int32_t>(entry.at("eventFlags").number())};

        for (const auto &value : entry.at("values").array()) {
            record.values.push_back(
                {static_cast<c::SerialFieldType>(value.at("type").number()), decimal(value.at("raw").text()),
                 value.at("units").null() ? std::nullopt : std::optional(units(value.at("units").text()))});
        }

        result.push_back(std::move(record));
    }

    return result;
}

/** One successful case's explicit envelope actions and expected data semantics. */
struct Capture {
    std::vector<std::byte> bytes;                        ///< Joined independent owned chunks.
    std::vector<std::int32_t> frames;                    ///< Message IDs, with -1 for markers.
    std::vector<Record> expected;                        ///< Input records in actual action order.
    std::vector<std::optional<std::int64_t>> timestamps; ///< Exact last explicit heartbeat per output record.
    p::WriterCounters counters;                          ///< Final logical accounting.
};

/** Compare enum fixture names without guessing an unknown outcome.
 * @param name Exact expected contract spelling.
 * @return Corresponding failure category, or fixture error.
 */
p::WriterError error(std::string_view name) {
    if (name == "INVALID_LAYOUT") {
        return p::WriterError::INVALID_LAYOUT;
    }

    if (name == "INVALID_VALUE") {
        return p::WriterError::INVALID_VALUE;
    }

    if (name == "UNSUPPORTED") {
        return p::WriterError::UNSUPPORTED;
    }

    if (name == "INVALID_STATE") {
        return p::WriterError::INVALID_STATE;
    }

    throw std::runtime_error("fixture expected writer error");
}

/** Execute exact JSON actions through public methods.
 * @param writer Independent owned writer.
 * @param input Parsed complete writer contract.
 * @param owned Stable borrowed-input owners.
 * @param capture Optional successful action/output collector.
 * @return First diagnostic on failure, or absent after complete actions.
 */
std::optional<p::WriterDiagnostic> actions(p::TapeWriter &writer, const Json &input, const std::vector<Record> &owned,
                                           Capture *capture = nullptr) {
    std::optional<std::int64_t> timestamp;

    for (const auto &action : input.at("actions").array()) {
        const auto &name = action.at("op").text();
        std::expected<p::WriterChunk, p::WriterDiagnostic> result;
        std::int32_t type = -2;

        if (name == "begin") {
            result = writer.begin();
            type = 1;
        } else if (name == "describe") {
            std::vector<std::int32_t> ids;

            for (const auto &id : action.at("senderIds").array()) {
                ids.push_back(static_cast<std::int32_t>(id.number()));
            }

            result = writer.describe(ids);
            type = 2;
        } else if (name == "heartbeat") {
            p::TapeHeartbeat heartbeat;

            if (!action.at("content").null()) {
                heartbeat.content = static_cast<std::int32_t>(action.at("content").number());
            }

            if (!action.at("timestampMs").null()) {
                heartbeat.timestampMs = decimal(action.at("timestampMs").text());
            }

            result = writer.heartbeat(heartbeat);
            type = 0;

            if (result && heartbeat.timestampMs) {
                timestamp = heartbeat.timestampMs;
            }
        } else if (name == "data") {
            std::vector<p::OutgoingRecordView> views;

            for (const auto &index : action.at("recordIndices").array()) {
                views.push_back(owned.at(static_cast<std::size_t>(index.number())).view());
            }

            result = writer.data(views);
            type = 5;

            if (capture && result) {
                for (const auto &index : action.at("recordIndices").array()) {
                    capture->expected.push_back(owned.at(static_cast<std::size_t>(index.number())));
                    capture->timestamps.push_back(timestamp);
                }
            }
        } else if (name == "marker") {
            result = writer.marker();
            type = -1;
        } else if (name == "finish") {
            result = writer.finish();
        } else {
            throw std::runtime_error("fixture action");
        }

        if (!result) {
            return result.error();
        }

        if (capture) {
            capture->bytes.insert(capture->bytes.end(), result->bytes.begin(), result->bytes.end());

            if (type != -2) {
                capture->frames.push_back(type);
            }
        }
    }

    if (capture) {
        capture->counters = writer.counters();
    }

    return {};
}

/** Pack the independently specified Java cursor timing with floor division and saturation.
 * @param timestamp Exact optional signed wire milliseconds.
 * @return Signed packed sequence, zero before an explicit timestamp.
 */
std::int64_t sequence(std::optional<std::int64_t> timestamp) {
    if (!timestamp) {
        return 0;
    }

    auto seconds = *timestamp / 1000;
    auto millis = *timestamp % 1000;

    if (millis < 0) {
        --seconds;
        millis += 1000;
    }

    seconds = std::clamp(seconds, static_cast<std::int64_t>(INT32_MIN), static_cast<std::int64_t>(INT32_MAX));
    const auto packed = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(seconds)) << 32) |
                        (static_cast<std::uint64_t>(millis) << 22);

    return std::bit_cast<std::int64_t>(packed);
}

/** Read each emitted action through the existing public RAW reader and compare input semantics.
 * @param capture Whole tape and expected action/record sequence.
 * @param outgoing Explicit immutable mapping used for writing.
 * @param expectedDescriptor Exact represented header.
 * @return Delivered timing sequence per record, for the independent Java verifier.
 */
std::vector<std::int64_t> roundTrip(const Capture &capture, const p::OutgoingLayouts &outgoing,
                                    const p::OutgoingDescriptor &expectedDescriptor) {
    p::TapeLimits limits{{65536, 65545},
                         1000000,
                         1000,
                         {100, 10, {10000, 10000}, 1000000},
                         {{100, 1000, 1000, 1000, 10000}, 100, {1000, 1000, 10000}},
                         1000000,
                         {65536,
                          10000,
                          100000,
                          10000,
                          100000,
                          1000000,
                          {65536, 10000, 10000},
                          {10000, 10000},
                          1000000,
                          1000000,
                          1000000},
                         1000};
    p::TapeReader reader(outgoing.schema(), limits, p::TapeMode::RAW_MESSAGE_V1);
    std::size_t position = 0;
    std::size_t frame = 0;
    std::size_t record = 0;
    std::vector<std::int64_t> sequences;

    while (position < capture.bytes.size()) {
        auto result = reader.feed(std::span(capture.bytes).subspan(position));
        REQUIRE(result.accepted > 0);
        position += result.accepted;
        REQUIRE(result.status != p::TapeStatus::FAILED);

        if (!result.frame) {
            continue;
        }

        REQUIRE(frame < capture.frames.size());
        const auto id = result.frame->type.value_or(-1);
        CHECK(id == capture.frames[frame++]);

        for (const auto &decoded : result.records) {
            REQUIRE(record < capture.expected.size());
            const auto &expected = capture.expected[record];
            const auto *layout = outgoing.find(expected.senderId);
            REQUIRE(layout);
            CHECK(decoded.localId == layout->localRecordId);
            CHECK(decoded.symbol.units == expected.symbol);
            CHECK(decoded.typedValues == expected.values);
            REQUIRE(result.timing);
            CHECK(result.timing->timestampMs == capture.timestamps[record]);
            CHECK(result.timing->eventTimeSequence == sequence(capture.timestamps[record]));
            sequences.push_back(result.timing->eventTimeSequence);
            ++record;
        }
    }

    CHECK(frame == capture.frames.size());
    CHECK(record == capture.expected.size());
    CHECK(reader.finish() == p::TapeStatus::FINISHED);
    REQUIRE(reader.descriptor());
    CHECK(reader.descriptor()->version == expectedDescriptor.version);
    CHECK(reader.descriptor()->options == expectedDescriptor.options);
    CHECK(reader.descriptor()->time == expectedDescriptor.time);
    CHECK(reader.descriptor()->explicitRawSend == expectedDescriptor.explicitRawSend);

    return sequences;
}

/** Emit input-derived schema/semantics beside already asserted reader round-trip bytes.
 * @param name Explicit case directory name.
 * @param capture Complete successful tape and ordered inputs.
 * @param outgoing Exact explicit layout/schema owners.
 * @param sequences Independently asserted RAW reader timing observations.
 */
void emit(std::string_view name, const Capture &capture, const p::OutgoingLayouts &outgoing,
          const std::vector<std::int64_t> &sequences) {
    if (emitDirectory.empty()) {
        return;
    }

    const auto directory = emitDirectory / name;
    std::filesystem::create_directories(directory);
    std::ofstream tape(directory / "tape.qtp", std::ios::binary);
    tape.write(reinterpret_cast<const char *>(capture.bytes.data()),
               static_cast<std::streamsize>(capture.bytes.size()));
    REQUIRE(tape.good());
    std::ofstream schemaOutput(directory / "schema.tsv", std::ios::binary);

    for (const auto &record : outgoing.records()) {
        schemaOutput << "record\t" << hex(record.wireName) << "\t-\t-\t-\t-\n";
        const auto &local = outgoing.schema().records()[static_cast<std::size_t>(record.localRecordId)];

        for (std::size_t i = 0; i < local.fields.size(); ++i) {
            const auto field = std::find_if(record.fields.begin(), record.fields.end(), [&](const auto &entry) {
                return entry.localFieldId == static_cast<std::int32_t>(i);
            });
            REQUIRE(field != record.fields.end());
            const auto key = std::find_if(local.lookups.begin(), local.lookups.end(), [&](const auto &entry) {
                return entry.fieldId == i;
            });
            REQUIRE(key != local.lookups.end());
            schemaOutput << "field\t" << hex(record.wireName) << '\t' << i << '\t' << hex(key->name) << '\t'
                         << hex(field->wireName) << '\t' << field->serialTypeId << '\n';
        }
    }

    REQUIRE(schemaOutput.good());
    std::ofstream expected(directory / "expected.tsv", std::ios::binary);

    for (std::size_t i = 0; i < capture.expected.size(); ++i) {
        const auto &record = capture.expected[i];
        const auto *layout = outgoing.find(record.senderId);
        REQUIRE(layout);
        expected << "record\t" << i << '\t' << hex(layout->wireName) << '\t' << hex(record.symbol) << '\t'
                 << record.flags << '\t' << sequences.at(i) << '\n';

        for (std::size_t j = 0; j < record.values.size(); ++j) {
            const auto &value = record.values[j];
            expected << "value\t" << i << '\t' << j << '\t' << static_cast<std::int32_t>(value.type) << '\t'
                     << value.raw << '\t' << (value.units ? hex(*value.units) : "null") << '\n';
        }
    }

    REQUIRE(expected.good());
}

/** Apply only the reviewed mutation path grammar to an independent fixture tree.
 * @param root Owning JSON tree to modify.
 * @param path Dot-separated keys with optional numeric array selections.
 * @param replacement Complete replacement value from the fixture.
 */
void mutate(Json &root, std::string_view path, const Json &replacement) {
    Json *current = &root;

    while (!path.empty()) {
        const auto end = path.find('.');
        const auto part = path.substr(0, end);
        const auto bracket = part.find('[');
        const auto key = part.substr(0, bracket);
        current = &std::get<Json::Object>(current->value).at(std::string(key));

        if (bracket != std::string_view::npos) {
            if (part.back() != ']') {
                throw std::runtime_error("fixture mutation path");
            }

            const auto index = decimal(part.substr(bracket + 1, part.size() - bracket - 2));
            current = &std::get<Json::Array>(current->value).at(static_cast<std::size_t>(index));
        }

        if (end == std::string_view::npos) {
            break;
        }

        path.remove_prefix(end + 1);
    }

    *current = replacement;
}

/** Build owned explicit contract layouts through the public construction boundary.
 * @param input Parsed exact contract case.
 * @param limits Finite custom bounds.
 * @return Owned immutable declarations or first diagnostic.
 */
std::expected<p::OutgoingLayouts, p::WriterDiagnostic> contractLayouts(const Json &input,
                                                                       p::LayoutLimits limits = layoutLimits()) {
    auto local = schema(fixtureJson("schema.json"));
    auto declarations = layouts(input);

    return p::OutgoingLayouts::create(local, declarations, limits);
}

/** Execute a complete baseline contract with custom limits.
 * @param limits Required finite writer budgets.
 * @param capture Optional successful-operation evidence.
 * @return First construction/terminal error, or no failure.
 */
std::optional<p::WriterDiagnostic> contractRun(p::WriterLimits limits, Capture *capture = nullptr) {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), limits);

    if (!writer) {
        return writer.error();
    }

    return actions(*writer, input, records(input), capture);
}

TEST_CASE("read-only binary field profile refuses outgoing layouts before allocation or traversal") {
    for (const auto type : {-1, 0, 1, 8, 104}) {
        std::vector<c::LocalRecord> records;

        if (type >= 0) {
            records.push_back({0, u"R", false, {{type}}, {{u"Value", 0}}});
        }

        auto schema = c::LocalSchema::create(records, {1, 1, 1, 5, 6}, c::LocalProfile::BINARY_FIELDS_V1);
        REQUIRE(schema);
        const std::vector<p::OutgoingRecord> outgoing{{-1, -1, u"invalid", {}}};

        for (const bool empty : {false, true}) {
            const auto declarations = empty ? std::span<const p::OutgoingRecord>{} : std::span(outgoing);
            allocation_test::remaining = 0;
            allocation_test::enabled = true;
            const auto result = p::OutgoingLayouts::create(*schema, declarations, {});
            allocation_test::enabled = false;
            REQUIRE_FALSE(result);
            CHECK(result.error().error == p::WriterError::UNSUPPORTED);
            CHECK(result.error().operation == p::WriterOperation::LAYOUT);
            CHECK(result.error().stage == "profile");
            CHECK_FALSE(result.error().record);
            CHECK_FALSE(result.error().field);
        }
    }
}

TEST_CASE("writer executes the immutable JSON action contract and semantic reader round trip") {
    const auto input = fixtureJson("writer-cases.json");
    CHECK(input.at("format").text() == "qdcxx-tape-writer-contract");
    CHECK(input.at("version").number() == 1);
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
    REQUIRE(writer);
    auto owned = records(input);
    Capture capture;
    REQUIRE(!actions(*writer, input, owned, &capture));
    CHECK(writer->state() == p::WriterState::FINISHED);
    CHECK(capture.frames == std::vector<std::int32_t>{1, 2, 0, 0, 0, 5, -1, 2, 5, 5, 2, 0});
    CHECK(writer->counters().outputBytes == capture.bytes.size());
    CHECK(writer->counters().frames == capture.frames.size());
    const auto sequences = roundTrip(capture, *outgoing, descriptor(input.at("descriptor")));
    emit("contract", capture, *outgoing, sequences);
    const auto before = writer->counters().workUnits;
    auto again = writer->finish();
    REQUIRE(again);
    CHECK(again->bytes.empty());
    CHECK(writer->counters().workUnits == before);
}

TEST_CASE("every reviewed JSON mutation fails with the declared first diagnostic") {
    const auto original = fixtureJson("writer-cases.json");
    std::size_t count = 0;

    for (const auto &mutation : original.at("mutations").array()) {
        INFO(mutation.at("id").text());
        auto input = original;
        mutate(input, mutation.at("path").text(), mutation.at("value"));
        const auto &object = std::get<Json::Object>(mutation.value);

        if (object.contains("also")) {
            const auto &also = mutation.at("also");
            mutate(input, also.at("path").text(), also.at("value"));
        }

        auto outgoing = contractLayouts(input);
        std::optional<p::WriterDiagnostic> failure;

        if (!outgoing) {
            failure = outgoing.error();
        } else {
            auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());

            if (!writer) {
                failure = writer.error();
            } else {
                failure = actions(*writer, input, records(input));
                REQUIRE(failure);
                CHECK(writer->state() == p::WriterState::FAILED);
                const auto counters = writer->counters();
                const auto later = writer->data({});
                REQUIRE(!later);
                CHECK(later.error() == *failure);
                CHECK(writer->counters().workUnits == counters.workUnits);
                CHECK(writer->counters().frames == counters.frames);
                CHECK(writer->counters().outputBytes == counters.outputBytes);
            }
        }

        REQUIRE(failure);
        CHECK(failure->error == error(mutation.at("expected").text()));

        if (object.contains("stage")) {
            CHECK(failure->stage == mutation.at("stage").text());
        }

        ++count;
    }

    CHECK(count == original.at("mutations").array().size());
}

/** Select one fixture-named writer allowance without a general configuration layer.
 * @param limits Finite test limits to modify.
 * @param name Exact budgets.tsv writer spelling.
 * @return Borrowed selected allowance; unknown names are fixture errors.
 */
std::size_t &writerBudget(p::WriterLimits &limits, std::string_view name) {
    if (name == "frameBytes") {
        return limits.frameBytes;
    }

    if (name == "outputBytes") {
        return limits.outputBytes;
    }

    if (name == "frames") {
        return limits.frames;
    }

    if (name == "records") {
        return limits.records;
    }

    if (name == "fields") {
        return limits.fields;
    }

    if (name == "text.bodyBytes") {
        return limits.text.bodyBytes;
    }

    if (name == "text.codeUnits") {
        return limits.text.codeUnits;
    }

    if (name == "textUnits") {
        return limits.textUnits;
    }

    if (name == "fieldString.encodedBytes") {
        return limits.fieldString.encodedBytes;
    }

    if (name == "fieldString.codeUnits") {
        return limits.fieldString.codeUnits;
    }

    if (name == "fieldBytes") {
        return limits.fieldBytes;
    }

    if (name == "fieldUnits") {
        return limits.fieldUnits;
    }

    if (name == "symbolUnits") {
        return limits.symbolUnits;
    }

    if (name == "totalSymbolUnits") {
        return limits.totalSymbolUnits;
    }

    if (name == "scratchBytes") {
        return limits.scratchBytes;
    }

    if (name == "scratchUnits") {
        return limits.scratchUnits;
    }

    if (name == "workUnits") {
        return limits.workUnits;
    }

    throw std::runtime_error("fixture writer budget name");
}

TEST_CASE("every writer TSV allowance has fixed-input zero exact below and above witnesses") {
    Capture baseline;
    REQUIRE(!contractRun(writerLimits(), &baseline));
    // Reviewed input has two description IDs, four records and eight attempted logical fields.
    CHECK(baseline.counters.records == 6);
    CHECK(baseline.counters.fields == 8);
    CHECK(baseline.counters.frames == 12);
    // Descriptor validation/copy is fourteen units, policy comparison visits eight more units;
    // descriptor/description writing adds 177 visits.
    CHECK(baseline.counters.textUnits == 199);
    // Three type-10 inputs are null, empty and three units/eight bytes, traversed four times.
    CHECK(baseline.counters.fieldBytes == 40);
    CHECK(baseline.counters.fieldUnits == 12);
    // New three-unit and one-unit symbols each have sizing, two primitive passes and an owned copy.
    CHECK(baseline.counters.symbolUnits == 16);
    CHECK(baseline.counters.peakScratchBytes == 48);
    CHECK(baseline.counters.peakScratchUnits == 3);
    // Schema/layout/configuration visits: 180; descriptor: 90; description groups: 82 and 24;
    // data batches: 54 and 10. Empty actions and fixed scalar metadata do not visit elements/units.
    CHECK(baseline.counters.workUnits == 440);
    const std::map<std::string, std::size_t> required{{"frameBytes", 47},
                                                      {"outputBytes", baseline.bytes.size()},
                                                      {"frames", 12},
                                                      {"records", 6},
                                                      {"fields", 8},
                                                      {"text.bodyBytes", 13},
                                                      {"text.codeUnits", 13},
                                                      {"textUnits", 199},
                                                      {"fieldString.encodedBytes", 8},
                                                      {"fieldString.codeUnits", 3},
                                                      {"fieldBytes", 40},
                                                      {"fieldUnits", 12},
                                                      {"symbolUnits", 3},
                                                      {"totalSymbolUnits", 16},
                                                      {"scratchBytes", 48},
                                                      {"scratchUnits", 3},
                                                      {"workUnits", 440}};
    std::size_t exercised = 0;

    for (const auto &row : rows("budgets.tsv")) {
        REQUIRE(row.size() == 5);

        if (row[1] != "writer") {
            continue;
        }

        INFO(row[0], " ", row[2]);
        const auto exact = required.at(row[2]);

        for (const auto allowance : {std::size_t{0}, exact - 1, exact, exact + 1}) {
            auto limits = writerLimits();
            writerBudget(limits, row[2]) = allowance;
            Capture capture;
            const auto failure = contractRun(limits, &capture);

            if (allowance < exact) {
                REQUIRE(failure);
                CHECK(failure->error == p::WriterError::RESOURCE_LIMIT);
            } else {
                REQUIRE(!failure);
                CHECK(capture.bytes == baseline.bytes);
            }
        }

        ++exercised;
    }

    CHECK(exercised == 17);
}

TEST_CASE("layout TSV allowances include schema bounds and exact complete declaration copies") {
    const auto input = fixtureJson("writer-cases.json");
    std::size_t exercised = 0;

    for (const auto &row : rows("budgets.tsv")) {
        REQUIRE(row.size() == 5);

        if (row[1] != "layout") {
            continue;
        }

        INFO(row[0]);
        const auto exact = row[2] == "records" || row[2] == "fields" ? 2U : row[2] == "nameUnits" ? 13U : 29U;

        for (const auto allowance : {0U, exact - 1, exact, exact + 1}) {
            auto limits = layoutLimits();

            if (row[2] == "records") {
                limits.records = allowance;
            } else if (row[2] == "fields") {
                limits.fields = allowance;
            } else if (row[2] == "nameUnits") {
                limits.nameUnits = allowance;
            } else if (row[2] == "totalNameUnits") {
                limits.totalNameUnits = allowance;
            } else {
                throw std::runtime_error("fixture layout budget name");
            }

            const auto result = contractLayouts(input, limits);

            if (allowance < exact) {
                REQUIRE(!result);
                CHECK(result.error().error == p::WriterError::RESOURCE_LIMIT);
            } else {
                REQUIRE(result);
                CHECK(result->records().size() == 2);
            }
        }

        ++exercised;
    }

    CHECK(exercised == 4);
    // Complete schema copying separately bounds records, fields, aliases and all owned names.
    const std::array<c::SchemaLimits, 5> exactSchema{c::SchemaLimits{2, 100, 100, 100, 100},
                                                     {100, 2, 100, 100, 100},
                                                     {100, 100, 3, 100, 100},
                                                     {100, 100, 100, 13, 100},
                                                     {100, 100, 100, 100, 34}};

    for (std::size_t selected = 0; selected < exactSchema.size(); ++selected) {
        auto limits = layoutLimits();
        limits.schema = exactSchema[selected];
        REQUIRE(contractLayouts(input, limits));
        std::size_t *value = selected == 0   ? &limits.schema.records
                             : selected == 1 ? &limits.schema.fields
                             : selected == 2 ? &limits.schema.lookups
                             : selected == 3 ? &limits.schema.nameUnits
                                             : &limits.schema.totalNameUnits;
        --*value;
        CHECK(!contractLayouts(input, limits));
        *value = 0;
        CHECK(!contractLayouts(input, limits));
    }
}

TEST_CASE("native sparse sender tables accept MAX minus one and refuse the Java ID overflow endpoint") {
    auto input = fixtureJson("writer-cases.json");
    mutate(input, "layouts[0].senderId", Json{static_cast<std::int64_t>(INT32_MAX - 1)});
    auto sparse = contractLayouts(input);
    REQUIRE(sparse);
    REQUIRE(sparse->find(INT32_MAX - 1));
    CHECK(sparse->records().size() == 2);
    mutate(input, "layouts[0].senderId", Json{static_cast<std::int64_t>(INT32_MAX)});
    const auto overflow = contractLayouts(input);
    REQUIRE(!overflow);
    CHECK(overflow.error().error == p::WriterError::INVALID_LAYOUT);
    CHECK(overflow.error().stage == "record-id");
}

TEST_CASE("actual owning allocation failures preserve atomic chunks announcements and first errors") {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    const auto metadata = descriptor(input.at("descriptor"));
    auto local = schema(fixtureJson("schema.json"));
    const auto supplied = layouts(input);
    std::size_t layoutFailures = 0;

    for (std::size_t allowance = 0; allowance < 100; ++allowance) {
        allocation_test::remaining = allowance;
        allocation_test::enabled = true;
        auto candidate = p::OutgoingLayouts::create(local, supplied, layoutLimits());
        allocation_test::enabled = false;

        if (candidate) {
            break;
        }

        CHECK(candidate.error().error == p::WriterError::ALLOCATION_FAILURE);
        ++layoutFailures;
    }

    CHECK(layoutFailures >= 6);
    std::size_t constructionFailures = 0;

    for (std::size_t allowance = 0; allowance < 100; ++allowance) {
        allocation_test::remaining = allowance;
        allocation_test::enabled = true;
        auto writer = p::TapeWriter::create(*outgoing, metadata, writerLimits());
        allocation_test::enabled = false;

        if (writer) {
            break;
        }

        CHECK(writer.error().error == p::WriterError::ALLOCATION_FAILURE);
        ++constructionFailures;
    }

    CHECK(constructionFailures >= 6);
    auto writer = p::TapeWriter::create(*outgoing, metadata, writerLimits());
    REQUIRE(writer);
    REQUIRE(writer->begin());
    const auto before = writer->counters();
    const std::array<std::int32_t, 1> ids{37};
    allocation_test::remaining = 0;
    allocation_test::enabled = true;
    auto description = writer->describe(ids);
    allocation_test::enabled = false;
    REQUIRE(!description);
    CHECK(description.error().error == p::WriterError::ALLOCATION_FAILURE);
    CHECK(writer->counters().frames == before.frames);
    CHECK(writer->counters().outputBytes == before.outputBytes);
    CHECK(writer->counters().records > before.records);
    CHECK(writer->counters().workUnits > before.workUnits);
    const auto charged = writer->counters().workUnits;
    CHECK(writer->begin().error() == description.error());
    CHECK(writer->counters().workUnits == charged);

    std::size_t dataFailures = 0;
    auto owned = records(input);
    owned[0].symbol.assign(100, u'x');
    const std::array<p::OutgoingRecordView, 1> views{owned[0].view()};

    for (std::size_t allowance = 0; allowance < 10; ++allowance) {
        auto instance = p::TapeWriter::create(*outgoing, metadata, writerLimits());
        REQUIRE(instance);
        REQUIRE(instance->begin());
        REQUIRE(instance->describe(ids));
        const auto prior = instance->counters();
        allocation_test::remaining = allowance;
        allocation_test::enabled = true;
        auto data = instance->data(views);
        allocation_test::enabled = false;

        if (data) {
            break;
        }

        CHECK(data.error().error == p::WriterError::ALLOCATION_FAILURE);
        CHECK(instance->counters().frames == prior.frames);
        CHECK(instance->counters().outputBytes == prior.outputBytes);
        CHECK(instance->state() == p::WriterState::FAILED);
        ++dataFailures;
    }

    CHECK(dataFailures == 2); // One whole envelope and one actual retained long-symbol allocation.
}

/** Extract one complete public envelope without reproducing composition.
 * @param chunk Owned successful whole envelope.
 * @return Owned frame with the asserted RAW_DATA identity.
 */
p::Frame rawFrame(const p::WriterChunk &chunk) {
    p::FrameReader reader({65536, 65545});
    auto result = reader.feed(chunk.bytes);
    REQUIRE(result.accepted == chunk.bytes.size());
    REQUIRE(result.frame);
    REQUIRE(result.frame->type == 5);
    REQUIRE(reader.finish() == p::FrameStatus::FINISHED);

    return std::move(*result.frame);
}

/** Decode an exact fixture byte string without widening accepted text grammar.
 * @param text Pairs of lowercase hexadecimal digits.
 * @return Owning byte vector or fixture error.
 */
std::vector<std::byte> bytes(std::string_view text) {
    if (text.size() % 2 != 0) {
        throw std::runtime_error("fixture byte width");
    }

    std::vector<std::byte> result;

    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned value = 0;
        const auto parsed = std::from_chars(text.data() + i, text.data() + i + 2, value, 16);

        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + i + 2) {
            throw std::runtime_error("fixture byte hex");
        }

        result.push_back(static_cast<std::byte>(value));
    }

    return result;
}

TEST_CASE("symbol primitive TSV witnesses use FD first FF equality and reset each data frame") {
    auto local = c::LocalSchema::create(std::vector<c::LocalRecord>{{0, u"Empty", false, {}, {}}}, {1, 0, 0, 5, 5});
    REQUIRE(local);
    auto outgoing =
        p::OutgoingLayouts::create(*local, std::vector<p::OutgoingRecord>{{0, 0, u"Empty", {}}}, layoutLimits());
    REQUIRE(outgoing);
    std::size_t exercised = 0;

    for (const auto &row : rows("primitives.tsv")) {
        REQUIRE(row.size() == 8);

        if (!row[1].starts_with("symbol-")) {
            continue;
        }

        INFO(row[0]);
        auto writer = p::TapeWriter::create(*outgoing, {{}, {}, u"MESSAGE", false}, writerLimits());
        REQUIRE(writer);
        REQUIRE(writer->begin());
        REQUIRE(writer->describe(std::array<std::int32_t, 1>{0}));
        const auto symbol = units(row[2]);
        const p::OutgoingRecordView view{0, symbol, {}};
        std::vector<p::OutgoingRecordView> views{view};

        if (row[1] == "symbol-repeat") {
            views.push_back(view);
        } else if (row[1] == "symbol-new-frame") {
            REQUIRE(writer->data(views));
        }

        auto result = writer->data(views);
        REQUIRE(result);
        const auto frame = rawFrame(*result);
        const auto expected = bytes(row[7]);
        const std::size_t offset = row[1] == "symbol-repeat" ? 6 : 0;
        REQUIRE(frame.payload.size() == offset + expected.size() + 1);
        CHECK(
            std::equal(expected.begin(), expected.end(), frame.payload.begin() + static_cast<std::ptrdiff_t>(offset)));
        CHECK(frame.payload.back() == std::byte{0});
        CHECK(expected.size() == static_cast<std::size_t>(decimal(row[3])));
        CHECK(expected.size() == static_cast<std::size_t>(decimal(row[4])));
        CHECK(symbol.size() == static_cast<std::size_t>(decimal(row[5])));
        CHECK(row[6] == "OK");
        ++exercised;
    }

    CHECK(exercised == 5);
}

/** Append one asserted successful envelope and action ID to a capture.
 * @param capture Owning action sequence collector.
 * @param result Public writer result.
 * @param type Expected message ID, or -1 for an empty marker.
 */
void append(Capture &capture, std::expected<p::WriterChunk, p::WriterDiagnostic> result, std::int32_t type) {
    REQUIRE(result);
    capture.frames.push_back(type);
    capture.bytes.insert(capture.bytes.end(), result->bytes.begin(), result->bytes.end());
}

TEST_CASE("production corpus retains all exact types symbols and signed heartbeat boundaries") {
    constexpr std::array<std::int32_t, 8> TYPES{2, 8, 10, 40, 56, 72, 88, 120};
    const std::array<std::u16string, 8> names{u"Char",    u"Count",    u"Text", u"Short",
                                              u"Seconds", u"Sequence", u"Date", u"Wide"};
    c::LocalRecord declaration{0, u"AllTypes", false, {}, {}};
    p::OutgoingRecord outgoingDeclaration{19, 0, u"AllTypes", {}};

    for (std::size_t i = 0; i < TYPES.size(); ++i) {
        declaration.fields.push_back({TYPES[i]});
        declaration.lookups.push_back({names[i], i});
        declaration.lookups.push_back({u"Outgoing" + names[i], i});
    }

    for (std::size_t i = TYPES.size(); i > 0; --i) {
        outgoingDeclaration.fields.push_back(
            {u"Outgoing" + names[i - 1], TYPES[i - 1], static_cast<std::int32_t>(i - 1)});
    }

    auto local =
        c::LocalSchema::create(std::array{declaration}, {1, 8, 16, 100, 1000}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(local);
    auto outgoing = p::OutgoingLayouts::create(*local, std::array{outgoingDeclaration}, layoutLimits());
    REQUIRE(outgoing);
    const p::OutgoingDescriptor metadata{u"", u"hs", u"MESSAGE", true};
    auto writer = p::TapeWriter::create(*outgoing, metadata, writerLimits());
    REQUIRE(writer);
    Capture capture;
    append(capture, writer->begin(), 1);
    append(capture, writer->describe(std::array<std::int32_t, 1>{19}), 2);
    const std::vector<std::u16string> symbols{
        u"", u"*", u"IBM", u"IBM", std::u16string(1000, u'x'), units("0000d800dc00dfff"), units("d83dde00"), u"IBM"};
    const std::array<std::optional<std::int64_t>, 8> timestamps{std::nullopt, 0,         -1,        999,
                                                                1000,         INT64_MIN, INT64_MAX, -1001};

    for (std::size_t i = 0; i < symbols.size(); ++i) {
        if (i == 0) {
            append(capture, writer->heartbeat({{}, {}}), 0);
            append(capture, writer->heartbeat({0, {}}), 0);
        } else {
            append(capture, writer->heartbeat({1, timestamps[i]}), 0);
        }

        std::vector<c::FieldValue> values{{c::SerialFieldType::UTF_CHAR, i % 2 == 0 ? 0x10ffff : 0xd800},
                                          {c::SerialFieldType::COMPACT_INT, i % 2 == 0 ? INT32_MIN : INT32_MAX},
                                          {c::SerialFieldType::UTF_CHAR_ARRAY, 0,
                                           i == 0   ? std::nullopt
                                           : i == 1 ? std::optional(std::u16string{})
                                                    : std::optional(units("0000d800dc00ffff"))},
                                          {c::SerialFieldType::SHORT_STRING, i % 2 == 0 ? INT32_MAX : INT32_MIN},
                                          {c::SerialFieldType::TIME_SECONDS, i % 2 == 0 ? INT32_MIN : INT32_MAX},
                                          {c::SerialFieldType::SEQUENCE, i % 2 == 0 ? INT32_MAX : INT32_MIN},
                                          {c::SerialFieldType::DATE, i % 2 == 0 ? INT32_MIN : INT32_MAX},
                                          {c::SerialFieldType::WIDE_DECIMAL, i % 2 == 0 ? INT64_MIN : INT64_MAX}};
        Record record{19, symbols[i], std::move(values)};
        append(capture, writer->data(std::array{record.view()}), 5);
        capture.expected.push_back(record);
        capture.timestamps.push_back(timestamps[i]);

        if (i == 3) {
            append(capture, writer->data(std::array{record.view(), record.view()}), 5);
            capture.expected.push_back(record);
            capture.expected.push_back(record);
            capture.timestamps.push_back(timestamps[i]);
            capture.timestamps.push_back(timestamps[i]);
        }
    }

    append(capture, writer->data({}), 5);
    append(capture, writer->describe({}), 2);
    append(capture, writer->marker(), -1);
    append(capture, writer->heartbeat({1, -1001}), 0); // Explicit trailing heartbeat remains an action.
    auto finished = writer->finish();
    REQUIRE(finished);
    CHECK(finished->bytes.empty());
    capture.counters = writer->counters();
    const auto sequences = roundTrip(capture, *outgoing, metadata);
    emit("all-types", capture, *outgoing, sequences);
}

TEST_CASE("unchanged default Quote and Profile schemas retain every real property field") {
    for (const auto name : {std::string_view("quote"), std::string_view("profile")}) {
        INFO(name);
        auto local = name == "quote" ? events::defaultQuoteSchema({10, 100, 100, 100, 10000})
                                     : events::defaultProfileSchema({10, 100, 100, 100, 10000});
        REQUIRE(local);
        REQUIRE(local->records().size() == 1);
        const auto &record = local->records()[0];
        const auto expectedCount = name == "quote" ? 8U : 16U;
        REQUIRE(record.fields.size() == expectedCount);
        p::OutgoingRecord declaration{name == "quote" ? 0 : 5, 0, record.name, {}};

        for (const auto &row : rows("../expected/extras/" + std::string(name) + "-raw-wire.tsv")) {
            if (row[0] != "wire-field") {
                continue;
            }

            REQUIRE(row.size() == 5);
            const auto position = static_cast<std::size_t>(decimal(row[2]));
            REQUIRE(position == declaration.fields.size());
            REQUIRE(position < record.fields.size());
            const auto type = static_cast<std::int32_t>(decimal(row[4]));
            REQUIRE(type == record.fields[position].serialTypeId);
            declaration.fields.push_back({units(row[3]), type, static_cast<std::int32_t>(position)});
        }

        REQUIRE(declaration.fields.size() == expectedCount);
        auto outgoing = p::OutgoingLayouts::create(*local, std::array{declaration}, layoutLimits());
        REQUIRE(outgoing);
        const p::OutgoingDescriptor metadata{u"opaque", std::nullopt, u"MESSAGE", false};
        auto writer = p::TapeWriter::create(*outgoing, metadata, writerLimits());
        REQUIRE(writer);
        Capture capture;
        append(capture, writer->begin(), 1);
        append(capture, writer->describe(std::array{declaration.senderId}), 2);

        for (std::size_t i = 0; i < 2; ++i) {
            const auto timestamp = i == 0 ? -1 : 0;
            append(capture, writer->heartbeat({1, timestamp}), 0);
            Record input{declaration.senderId, i == 0 ? u"IBM" : u"*", {}};

            for (std::size_t j = 0; j < record.fields.size(); ++j) {
                const auto type = static_cast<c::SerialFieldType>(record.fields[j].serialTypeId);
                c::FieldValue value{type, 0};

                if (type == c::SerialFieldType::UTF_CHAR_ARRAY) {
                    value.units = i == 0 ? std::nullopt : std::optional(units("0000d800dc00ffff"));
                } else if (type == c::SerialFieldType::WIDE_DECIMAL) {
                    value.raw = i == 0 ? INT64_MIN : INT64_MAX;
                } else if (type == c::SerialFieldType::UTF_CHAR) {
                    value.raw = j == 1 ? 'Q' : 'A';
                } else {
                    value.raw = i == 0 ? INT32_MIN : INT32_MAX;
                }

                input.values.push_back(std::move(value));
            }

            append(capture, writer->data(std::array{input.view()}), 5);
            capture.expected.push_back(input);
            capture.timestamps.push_back(timestamp);
        }

        auto finished = writer->finish();
        REQUIRE(finished);
        CHECK(finished->bytes.empty());
        capture.counters = writer->counters();
        const auto sequences = roundTrip(capture, *outgoing, metadata);
        const auto directoryName = "standard-" + std::string(name);
        emit(directoryName, capture, *outgoing, sequences);

        if (!emitDirectory.empty()) {
            std::ofstream mode(emitDirectory / directoryName / "schema-mode.txt", std::ios::binary);
            mode << "standard\n";
            REQUIRE(mode.good());
        }
    }
}

TEST_CASE("descriptor absence empty opaque text spellings and send-list distinction are owned") {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);

    for (const auto version : {std::optional<std::u16string>{}, std::optional<std::u16string>{u""},
                               std::optional<std::u16string>{units("d8000000ffff")}}) {
        for (const auto options : {std::optional<std::u16string>{}, std::optional<std::u16string>{u""},
                                   std::optional<std::u16string>{u"hs"}}) {
            for (const auto spelling : {u"MESSAGE", u"message"}) {
                for (const bool raw : {false, true}) {
                    p::OutgoingDescriptor original{version, options, spelling, raw};
                    const auto expected = original;
                    auto writer = p::TapeWriter::create(*outgoing, original, writerLimits());
                    REQUIRE(writer);
                    original.version = u"changed after copying";
                    original.options = u"invalid after copying";
                    original.time = u"NONE";
                    original.explicitRawSend = !raw;
                    Capture capture;
                    append(capture, writer->begin(), 1);
                    REQUIRE(writer->finish());
                    const auto sequences = roundTrip(capture, *outgoing, expected);

                    if (version && !version->empty() && !options && !raw &&
                        std::u16string_view(spelling) == u"message") {
                        emit("descriptor-opaque", capture, *outgoing, sequences);
                    }
                }
            }
        }
    }
}

TEST_CASE("descriptor policy comparisons charge caller text units before work and reads") {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);

    for (const auto spelling : {std::u16string_view(u"MESSAGE"), std::u16string_view(u"message")}) {
        const p::OutgoingDescriptor metadata{{}, u"hs", std::u16string(spelling), false};
        // Two validation/copy passes visit 18 units; hs adds two comparison visits.
        // Uppercase visits seven time units; lowercase first visits the failed uppercase unit, then seven units.
        const std::size_t comparison = spelling == u"MESSAGE" ? 9 : 10;
        const auto exactText = 18 + comparison;
        const auto exactWork = spelling == u"MESSAGE" ? 186U : 188U;

        for (const auto allowance : {exactText - 1, exactText, exactText + 1}) {
            auto limits = writerLimits();
            limits.textUnits = allowance;
            limits.workUnits = exactWork;
            auto writer = p::TapeWriter::create(*outgoing, metadata, limits);

            if (allowance < exactText) {
                REQUIRE(!writer);
                CHECK(writer.error().error == p::WriterError::RESOURCE_LIMIT);
                CHECK(writer.error().stage == "descriptor-text-comparison");
            } else {
                REQUIRE(writer);
                CHECK(writer->counters().textUnits == exactText);
                CHECK(writer->counters().workUnits == exactWork);
            }
        }

        auto workLimits = writerLimits();
        workLimits.textUnits = exactText;
        workLimits.workUnits = exactWork - 1;
        const auto work = p::TapeWriter::create(*outgoing, metadata, workLimits);
        REQUIRE(!work);
        CHECK(work.error().error == p::WriterError::RESOURCE_LIMIT);
        CHECK(work.error().stage == "construction-work");
    }

    // hs is examined before time. If neither allowance admits its first comparison, text fails first.
    auto limits = writerLimits();
    limits.textUnits = 18;
    limits.workUnits = 168;
    const auto textFirst = p::TapeWriter::create(*outgoing, {{}, u"hs", u"MESSAGE", false}, limits);
    REQUIRE(!textFirst);
    CHECK(textFirst.error().stage == "descriptor-text-comparison");
    limits.textUnits = 19;
    const auto workSecond = p::TapeWriter::create(*outgoing, {{}, u"hs", u"MESSAGE", false}, limits);
    REQUIRE(!workSecond);
    CHECK(workSecond.error().stage == "construction-work");
}

TEST_CASE("failure precedence and completed output remain atomic across invalid later records") {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    auto owned = records(input);

    for (const auto action : {p::WriterOperation::BEGIN, p::WriterOperation::DESCRIBE, p::WriterOperation::HEARTBEAT,
                              p::WriterOperation::DATA, p::WriterOperation::MARKER, p::WriterOperation::FINISH}) {
        auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
        REQUIRE(writer);
        std::expected<p::WriterChunk, p::WriterDiagnostic> result;

        if (action == p::WriterOperation::BEGIN) {
            REQUIRE(writer->begin());
            result = writer->begin();
        } else if (action == p::WriterOperation::DESCRIBE) {
            result = writer->describe({});
        } else if (action == p::WriterOperation::HEARTBEAT) {
            result = writer->heartbeat({{}, {}});
        } else if (action == p::WriterOperation::DATA) {
            result = writer->data({});
        } else if (action == p::WriterOperation::MARKER) {
            result = writer->marker();
        } else {
            result = writer->finish();
        }

        REQUIRE(!result);
        CHECK(result.error().error == p::WriterError::INVALID_STATE);
        const auto before = writer->counters();
        CHECK(writer->begin().error() == result.error());
        CHECK(writer->describe({}).error() == result.error());
        CHECK(writer->heartbeat({2, INT64_MAX}).error() == result.error());
        CHECK(writer->data({}).error() == result.error());
        CHECK(writer->marker().error() == result.error());
        CHECK(writer->finish().error() == result.error());
        CHECK(writer->counters().workUnits == before.workUnits);
    }

    auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
    REQUIRE(writer);
    auto prefix = writer->begin();
    REQUIRE(prefix);
    auto description = writer->describe(std::array<std::int32_t, 1>{37});
    REQUIRE(description);
    const auto before = writer->counters();
    owned[1].flags = 1;
    owned[1].senderId = -1;
    const auto result = writer->data(std::array{owned[0].view(), owned[1].view()});
    REQUIRE(!result);
    CHECK(result.error().stage == "flags");
    CHECK(result.error().record == 1);
    CHECK(writer->counters().outputBytes == before.outputBytes);
    CHECK(writer->counters().frames == before.frames);
    CHECK(writer->counters().records == before.records + 2);
    CHECK(writer->counters().fields == before.fields + 2);

    auto tight = writerLimits();
    tight.frameBytes = 47;
    auto priority = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), tight);
    REQUIRE(priority);
    REQUIRE(priority->begin());
    REQUIRE(priority->describe(std::array<std::int32_t, 1>{37}));
    owned[0].symbol.assign(100, u'x');
    owned[0].flags = 1;
    owned[0].senderId = -1;
    const auto flags = priority->data(std::array{owned[0].view()});
    REQUIRE(!flags);
    CHECK(flags.error().error == p::WriterError::UNSUPPORTED);
    CHECK(flags.error().stage == "flags");
}

TEST_CASE("heartbeat invalid combinations fail before output and retain the first checkpoint") {
    const auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    const std::array<p::TapeHeartbeat, 5> invalid{p::TapeHeartbeat{{}, 0}, {0, 0}, {1, {}}, {-1, {}}, {2, {}}};

    for (const auto heartbeat : invalid) {
        auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
        REQUIRE(writer);
        REQUIRE(writer->begin());
        const auto before = writer->counters();
        const auto result = writer->heartbeat(heartbeat);
        REQUIRE(!result);
        CHECK(result.error().error == p::WriterError::INVALID_VALUE);
        CHECK(result.error().stage == "heartbeat");
        CHECK(writer->counters().outputBytes == before.outputBytes);
        CHECK(writer->counters().frames == before.frames);
        CHECK(writer->heartbeat({1, 0}).error() == result.error());
    }

    auto limits = writerLimits();
    limits.frames = 1;
    auto writer = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), limits);
    REQUIRE(writer);
    REQUIRE(writer->begin());
    const auto resource = writer->heartbeat(invalid[0]);
    REQUIRE(!resource);
    CHECK(resource.error().error == p::WriterError::RESOURCE_LIMIT);
    CHECK(resource.error().stage == "frames");
}

TEST_CASE("empty schema layouts zero fields and markers remain explicit finite operations") {
    auto schema = c::LocalSchema::create({}, {0, 0, 0, 0, 0}, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(schema);
    auto outgoing = p::OutgoingLayouts::create(*schema, {}, {{0, 0, 0, 0, 0}, 0, 0, 0, 0});
    REQUIRE(outgoing);
    auto limits = writerLimits();
    limits.records = 0;
    limits.fields = 0;
    limits.fieldString = {0, 0};
    limits.fieldBytes = 0;
    limits.fieldUnits = 0;
    limits.symbolUnits = 0;
    limits.totalSymbolUnits = 0;
    limits.scratchUnits = 0;
    const p::OutgoingDescriptor metadata{{}, {}, u"MESSAGE", false};
    auto writer = p::TapeWriter::create(*outgoing, metadata, limits);
    REQUIRE(writer);
    Capture capture;
    append(capture, writer->begin(), 1);
    append(capture, writer->describe({}), 2);
    append(capture, writer->data({}), 5);
    append(capture, writer->marker(), -1);
    REQUIRE(writer->finish());
    CHECK(writer->counters().records == 0);
    CHECK(writer->counters().fields == 0);
    CHECK(writer->counters().symbolUnits == 0);
    CHECK(writer->counters().peakScratchUnits == 0);
    static_cast<void>(roundTrip(capture, *outgoing, metadata));
}

TEST_CASE("raw-value TSV identities and range boundaries use the actual writer") {
    const auto witnesses = rows("raw-values.tsv");
    REQUIRE(witnesses.size() == 24);

    for (const auto &row : witnesses) {
        REQUIRE(row.size() == 5);
        INFO(row[0]);
        const auto type = static_cast<std::int32_t>(decimal(row[1]));
        auto local = c::LocalSchema::create(std::vector<c::LocalRecord>{{0, u"Raw", false, {{type}}, {{u"Value", 0}}}},
                                            {1, 1, 1, 10, 20}, c::LocalProfile::EXTENDED_FIELDS_V1);

        if (row[4] == "UNSUPPORTED") {
            REQUIRE(!local);
            CHECK(local.error() == c::SchemaError::UNSUPPORTED_PROFILE);

            continue;
        }

        REQUIRE(local);
        auto outgoing = p::OutgoingLayouts::create(
            *local, std::vector<p::OutgoingRecord>{{0, 0, u"Raw", {{u"Value", type, 0}}}}, layoutLimits());
        REQUIRE(outgoing);
        auto writer = p::TapeWriter::create(*outgoing, {{}, {}, u"MESSAGE", false}, writerLimits());
        REQUIRE(writer);
        REQUIRE(writer->begin());
        REQUIRE(writer->describe(std::array<std::int32_t, 1>{0}));
        c::FieldValue value{static_cast<c::SerialFieldType>(type), decimal(row[2]), {}};

        if (row[3] != "null" && row[3] != "absent") {
            value.units = units(row[3]);
        }

        const std::array<c::FieldValue, 1> values{value};
        const std::array<p::OutgoingRecordView, 1> record{{{0, u"", values}}};
        const auto result = writer->data(record);

        if (row[4] == "ACCEPT") {
            CHECK(result.has_value());
        } else {
            REQUIRE(!result);
            CHECK(result.error().error == error(row[4]));
        }
    }
}

TEST_CASE("writer owns immutable configuration, chunks and independent instance state") {
    auto input = fixtureJson("writer-cases.json");
    auto outgoing = contractLayouts(input);
    REQUIRE(outgoing);
    auto first = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
    auto second = p::TapeWriter::create(*outgoing, descriptor(input.at("descriptor")), writerLimits());
    REQUIRE(first);
    REQUIRE(second);
    auto chunk = first->begin();
    REQUIRE(chunk);
    const auto stable = chunk->bytes;
    REQUIRE(first->describe(std::array<std::int32_t, 1>{37}));
    auto secondBegin = second->begin();
    REQUIRE(secondBegin);
    CHECK(secondBegin->bytes == stable);
    auto owned = records(input);
    const std::array<p::OutgoingRecordView, 1> view{owned[0].view()};
    REQUIRE(first->data(view));
    const auto unannounced = second->data(view);
    REQUIRE(!unannounced);
    CHECK(unannounced.error().stage == "unannounced-id");
    CHECK(chunk->bytes == stable);
    auto moved = std::move(*first);
    CHECK(first->state() == p::WriterState::FAILED);
    REQUIRE(moved.finish());
    const auto afterFinish = moved.marker();
    REQUIRE(!afterFinish);
    CHECK(afterFinish.error().error == p::WriterError::INVALID_STATE);
    CHECK(moved.state() == p::WriterState::FAILED);
}

} // namespace

/** Run finite contract tests and optionally emit the already asserted interoperability corpus.
 * @param argc Argument count; requires fixture root and optional --emit-dir PATH.
 * @param argv Caller arguments borrowed for the process lifetime.
 * @return Doctest exit status, or 2 for an invalid test invocation.
 */
int main(int argc, char **argv) {
    if (argc != 2 && argc != 4) {
        return 2;
    }

    fixtures = argv[1];

    if (argc == 4) {
        if (std::string_view(argv[2]) != "--emit-dir") {
            return 2;
        }

        emitDirectory = argv[3];
    }

    doctest::Context context;

    return context.run();
}
