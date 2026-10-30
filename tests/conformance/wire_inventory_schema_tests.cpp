// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "fault_buffer.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include <array>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <new>
#include <sstream>

namespace {
std::ptrdiff_t allocationFailAfter = -1; ///< Single-threaded ordinary allocation injection; -1 disables it.
}

/** Replace ordinary allocation only in this focused test process.
 * @param size Requested bytes; zero still returns an independently deletable allocation.
 * @return Owned storage, or bad_alloc at the selected allocation checkpoint.
 */
void *operator new(std::size_t size) {
    if (allocationFailAfter == 0) {
        throw std::bad_alloc();
    }

    if (allocationFailAfter > 0) {
        --allocationFailAfter;
    }

    if (auto *storage = std::malloc(size == 0 ? 1 : size)) {
        return storage;
    }

    throw std::bad_alloc();
}

/** Allocate an ordinary array through the same test-only checkpoint.
 * @param size Requested bytes.
 * @return Owned storage or bad_alloc.
 */
void *operator new[](std::size_t size) {
    return ::operator new(size);
}

/** Release test-owned ordinary storage.
 * @param storage Previously allocated storage, possibly null.
 */
void operator delete(void *storage) noexcept {
    std::free(storage);
}

/** Release test-owned ordinary array storage.
 * @param storage Previously allocated storage, possibly null.
 */
void operator delete[](void *storage) noexcept {
    std::free(storage);
}

/** Release sized ordinary storage without depending on allocator size metadata.
 * @param storage Previously allocated storage.
 * @param size Compiler-provided size, intentionally unused.
 */
void operator delete(void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

/** Release sized array storage.
 * @param storage Previously allocated storage.
 * @param size Compiler-provided size, intentionally unused.
 */
void operator delete[](void *storage, std::size_t size) noexcept {
    static_cast<void>(size);
    std::free(storage);
}

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace display = org::ttldtor::qdcxx::inspect;
std::filesystem::path root; ///< Repository root explicitly supplied by CTest.

/** Read unchanged production Java evidence.
 * @param name Fixture filename under the mixed exact-field capture.
 * @return Owned source bytes.
 */
std::vector<std::byte> fixture(std::string_view name) {
    std::ifstream input(root / "tests/fixtures/encoding/field-types/expected" / name, std::ios::binary);
    REQUIRE(input.good());
    const std::vector<char> chars((std::istreambuf_iterator<char>(input)), {});
    const auto bytes = std::as_bytes(std::span(chars));

    return {bytes.begin(), bytes.end()};
}

/** Append a canonical signed compact primitive.
 * @param bytes Owning destination.
 * @param value Exact signed value.
 */
void compact(std::vector<std::byte> &bytes, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto result = e::writeCompactLong(value, buffer);
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Append a bounded description name.
 * @param bytes Owning destination.
 * @param value Exact UTF-16 name.
 */
void text(std::vector<std::byte> &bytes, std::u16string_view value) {
    std::array<std::byte, 1024> buffer{};
    const auto result = e::writeUtfString(value, buffer, {1024, 256});
    REQUIRE(result.status == e::Status::OK);
    bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(result.written));
}

/** Wrap a designed payload without changing its bytes.
 * @param tape Owning destination.
 * @param type Full message identity.
 * @param body Complete payload.
 */
void message(std::vector<std::byte> &tape, std::int32_t type, std::span<const std::byte> body) {
    std::vector<std::byte> contents;
    compact(contents, type);
    contents.insert(contents.end(), body.begin(), body.end());
    compact(tape, static_cast<std::int64_t>(contents.size()));
    tape.insert(tape.end(), contents.begin(), contents.end());
}

/** Retain only a real Java descriptor for a designed payload witness.
 * @param mode Existing explicit container mode.
 * @return Descriptor envelope bytes.
 */
std::vector<std::byte> header(p::TapeMode mode = p::TapeMode::LEGACY) {
    auto tape = fixture(mode == p::TapeMode::LEGACY ? "mixed-stream.qtp" : "mixed-raw.qtp");
    p::FrameReader reader({65536, 65545});
    const auto first = reader.feed(tape);
    REQUIRE(first.frame);
    REQUIRE(first.frame->type == 1);
    tape.resize(first.accepted);

    return tape;
}

/** Append complete owned source declarations in their specified order.
 * @param tape Owning destination.
 * @param records Exact raw descriptions.
 */
void describe(std::vector<std::byte> &tape, std::span<const p::WireRecordDescription> records) {
    std::vector<std::byte> body;

    for (const auto &record : records) {
        compact(body, record.senderId);
        text(body, record.name);
        compact(body, static_cast<std::int64_t>(record.fields.size()));

        for (const auto &field : record.fields) {
            text(body, field.name);
            compact(body, field.serialTypeId);
        }
    }

    message(tape, 2, body);
}

/** Append one zero-symbol integer-field record.
 * @param tape Owning destination.
 * @param sender Exact source identity.
 * @param values Source-order primitive values; -1 encodes null type-10 arrays.
 * @param mode Selected existing mode.
 */
void data(std::vector<std::byte> &tape, std::int32_t sender, std::initializer_list<std::int64_t> values,
          p::TapeMode mode = p::TapeMode::LEGACY) {
    std::vector<std::byte> body{std::byte{0xfc}, std::byte{0}};
    compact(body, sender);

    for (const auto value : values) {
        compact(body, value);
    }

    message(tape, mode == p::TapeMode::LEGACY ? 15 : 5, body);
}

/** Feed owned bytes to an existing reader, preserving every completed result.
 * @param reader Explicit owner.
 * @param bytes Borrowed finite bytes.
 * @param chunk Positive scheduling bound.
 * @return Independent owned completed prefix.
 */
std::vector<p::StreamDataRecord> feed(p::TapeReader &reader, std::span<const std::byte> bytes,
                                      std::size_t chunk = 65536) {
    std::vector<p::StreamDataRecord> records;

    while (!bytes.empty()) {
        auto result = reader.feed(bytes.first(std::min(chunk, bytes.size())));
        bytes = bytes.subspan(result.accepted);

        for (auto &record : result.records) {
            records.push_back(std::move(record));
        }

        if (result.status == p::TapeStatus::FAILED) {
            break;
        }

        REQUIRE(result.accepted > 0);
    }

    return records;
}

/** Construct an independently owned explicit ordinary schema.
 * @param records Caller declaration witness.
 * @return Validated immutable schema.
 */
c::LocalSchema local(std::vector<c::LocalRecord> records = {}) {
    auto result =
        c::LocalSchema::create(records, ti::Limits{}.tape.context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(result);

    return std::move(*result);
}

/** Split an output row while retaining every positional column.
 * @param text Complete row or typed-value cell.
 * @param separator Explicit column or value separator.
 * @return Independent owned cells.
 */
std::vector<std::string> cells(const std::string &text, char separator = '\t') {
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string value;

    while (std::getline(input, value, separator)) {
        result.push_back(value);
    }

    return result;
}

/** Accept a finite physical prefix, then report a short write even within one row group. */
class PrefixBuffer : public std::streambuf {
    std::size_t limit_; ///< Exact maximum accepted bytes.
    public:
    std::string bytes; ///< Physically accepted prefix for incomplete-snapshot assertions.

    /** Set the injected physical output boundary.
     * @param limit Maximum accepted bytes, independent of logical output budget.
     */
    explicit PrefixBuffer(std::size_t limit) : limit_(limit) {
    }

    protected:
    /** Accept only the remaining finite prefix.
     * @param source Borrowed encoded bytes.
     * @param count Requested physical write.
     * @return Actually accepted bytes; short count sets ostream failure.
     */
    std::streamsize xsputn(const char *source, std::streamsize count) override {
        const auto accepted = std::min(static_cast<std::size_t>(count), limit_ - bytes.size());
        bytes.append(source, accepted);

        return static_cast<std::streamsize>(accepted);
    }
};
} // namespace

TEST_CASE("supplied schema characterization skips unknown names and preserves explicit generic fields") {
    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        auto tape = header(mode);
        describe(tape, std::vector<p::WireRecordDescription>{{90, u"Custom", {{u"Count", 8}}}});
        data(tape, 90, {42}, mode);
        auto empty = local();
        p::TapeReader skipped(empty, ti::Limits{}.tape, mode);
        CHECK(feed(skipped, tape).empty());
        CHECK(skipped.finish() == p::TapeStatus::FINISHED);
        CHECK(skipped.processedRecords() == 1);
        auto exact = local({{0, u"Custom", false, {{8}}, {{u"Count", 0}}}});
        p::TapeReader retained(exact, ti::Limits{}.tape, mode);
        const auto records = feed(retained, tape);
        REQUIRE(records.size() == 1);
        REQUIRE(records[0].typedValues.size() == 1);
        CHECK(records[0].typedValues[0].raw == 42);
        CHECK(retained.finish() == p::TapeStatus::FINISHED);
        auto incompatible = local({{0, u"Custom", false, {{10}}, {{u"Count", 0}}}});
        p::TapeReader refused(incompatible, ti::Limits{}.tape, mode);
        CHECK(feed(refused, tape).empty());
        CHECK(refused.finish() == p::TapeStatus::FAILED);
        CHECK(refused.diagnostic().rejection == p::TapeRejection::TYPE);
    }
}

TEST_CASE("wire discovery keeps sparse source IDs and append-only fields with snapshot provenance") {
    p::StreamDataRecord oldAggregate{0, {}, {}};
    CHECK(oldAggregate.senderId == -1);

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        p::TapeReader reader(ti::Limits{}.tape, mode);
        CHECK(reader.schema().records().empty());
        auto first = header(mode);
        describe(first, std::vector<p::WireRecordDescription>{{90, u"R", {{u"A", 8}, {u"B", 10}}}});
        data(first, 90, {0, -1}, mode);
        auto original = feed(reader, first, 1);
        REQUIRE(original.size() == 1);
        CHECK(original[0].senderId == 90);
        auto oldSchema = reader.schema().boundedCopy(ti::Limits{}.tape.context.schema);
        REQUIRE(oldSchema);
        auto second = std::vector<std::byte>{};
        describe(second, std::vector<p::WireRecordDescription>{{7, u"R", {{u"B", 10}, {u"C", 40}, {u"A", 8}}},
                                                               {2147483647, u"Extra\U0001f600", {}}});
        data(second, 7, {-1, 12, 0}, mode);
        data(second, 90, {0, -1}, mode);
        data(second, 2147483647, {}, mode);
        auto current = feed(reader, second);
        first.assign(first.size(), std::byte{0});
        second.clear();
        REQUIRE(current.size() == 3);
        REQUIRE(reader.schema().records().size() == 2);
        const auto &record = reader.schema().records()[0];
        REQUIRE(record.fields.size() == 3);
        CHECK(record.lookups[0].name == u"A");
        CHECK(record.lookups[1].name == u"B");
        CHECK(record.lookups[2].name == u"C");
        CHECK(reader.descriptions()[0].senderId == 90);
        CHECK(reader.descriptions()[1].fields[0].name == u"B");
        CHECK(reader.context()->find(7)->binding.fields[0].targetFieldId == 1);
        CHECK(reader.context()->find(7)->binding.fields[1].targetFieldId == 2);
        CHECK(reader.context()->find(7)->binding.fields[2].targetFieldId == 0);
        CHECK(current[0].senderId == 7);
        CHECK(current[1].senderId == 90);
        CHECK(current[2].senderId == 2147483647);
        CHECK(current[2].localId == 1);
        CHECK(current[0].typedValues[2].raw == 12);
        CHECK(current[1].typedValues[2].raw == 0);
        CHECK_FALSE(current[0].typedValues[1].units);
        CHECK(original[0].typedValues.size() == 2);
        CHECK(original[0].context.matches(oldSchema->recordContext(0)));
        CHECK_FALSE(original[0].context.matches(reader.schema().recordContext(0)));
        CHECK(current[0].context.matches(reader.schema().recordContext(0)));
        auto beforeEmpty = reader.schema().recordContext(0);
        std::vector<std::byte> empty;
        describe(empty, {});
        feed(reader, empty);
        CHECK_FALSE(beforeEmpty.matches(reader.schema().recordContext(0)));
        CHECK(reader.finish() == p::TapeStatus::FINISHED);
    }
}

TEST_CASE("wire discovery rejects an entire late batch and retains old metadata and identity") {
    const std::vector<std::vector<p::WireRecordDescription>> cases{{{7, u"New", {{u"X", 8}}}, {8, u"R", {{u"A", 10}}}},
                                                                   {{7, u"New", {{u"X", 8}, {u"X", 8}}}},
                                                                   {{90, u"Again", {}}},
                                                                   {{7, u"New", {{u"X", 9}}}},
                                                                   {{7, std::u16string(1, char16_t(0xd800)), {}}}};

    for (const auto &batch : cases) {
        p::TapeReader reader(ti::Limits{}.tape);
        auto first = header();
        describe(first, std::vector<p::WireRecordDescription>{{90, u"R", {{u"A", 8}}}});
        feed(reader, first);
        const auto identity = reader.schema().recordContext(0);
        const auto *context = reader.context();
        const auto *description = reader.descriptions().data();
        const auto construction = reader.constructionUnits();
        std::vector<std::byte> bad;
        describe(bad, batch);
        feed(reader, bad);
        CHECK(reader.finish() == p::TapeStatus::FAILED);
        CHECK(identity.matches(reader.schema().recordContext(0)));
        CHECK(reader.schema().records().size() == 1);
        CHECK(reader.context() == context);
        CHECK(reader.descriptions().data() == description);
        CHECK(reader.descriptions().size() == 1);
        CHECK(reader.constructionUnits() > construction);
    }
}

TEST_CASE("wire construction and complete union schema are admitted before owning materialization") {
    auto tape = header();
    std::vector<std::byte> description;
    describe(description, std::vector<p::WireRecordDescription>{{7, u"R", {{u"A", 8}}}});
    p::FrameReader framer({4096, 4105});
    auto frame = framer.feed(description);
    REQUIRE(frame.frame);
    const auto rawCharge = frame.frame->payload.size();
    const auto completeCharge = rawCharge + 5 + 5 + 4; // S=5, context=4 for R(A:8).

    for (const auto allowance : {std::size_t{0}, rawCharge - 1, rawCharge, rawCharge + 5, completeCharge - 1,
                                 completeCharge, completeCharge + 1}) {
        ti::Limits bounds;
        bounds.tape.constructionUnits = allowance;
        p::TapeReader reader(bounds.tape);
        feed(reader, tape);
        feed(reader, description);
        const bool fits = allowance >= completeCharge;
        CHECK((reader.finish() == p::TapeStatus::FINISHED) == fits);
        CHECK(reader.schema().records().size() == (fits ? 1 : 0));
        CHECK(reader.constructionUnits() <= allowance);

        if (fits) {
            CHECK(reader.constructionUnits() == completeCharge);
        }
    }

    for (const auto member : {0, 1, 2, 3, 4}) {
        ti::Limits bounds;
        auto &schema = bounds.tape.context.schema;

        if (member == 0) {
            schema.records = 0;
        }

        if (member == 1) {
            schema.fields = 0;
        }

        if (member == 2) {
            schema.lookups = 0;
        }

        if (member == 3) {
            schema.nameUnits = 0;
        }

        if (member == 4) {
            schema.totalNameUnits = 1;
        }

        p::TapeReader reader(bounds.tape);
        feed(reader, tape);
        feed(reader, description);
        CHECK(reader.finish() == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
        CHECK(reader.constructionUnits() == rawCharge);
        CHECK(reader.schema().records().empty());
    }

    for (const auto delta : {std::size_t{0}, std::size_t{1}}) {
        ti::Limits bounds;
        bounds.tape.context.schema = {1 + delta, 1 + delta, 1 + delta, 1 + delta, 2 + delta};
        p::TapeReader reader(bounds.tape);
        feed(reader, tape);
        feed(reader, description);
        CHECK(reader.finish() == p::TapeStatus::FINISHED);
    }

    ti::Limits tight;
    tight.tape.context.schema.fields = 1;
    p::TapeReader late(tight.tape);
    feed(late, tape);
    feed(late, description);
    const auto old = late.schema().recordContext(0);
    const auto previousCharge = late.constructionUnits();
    std::vector<std::byte> extra;
    describe(extra, std::vector<p::WireRecordDescription>{{8, u"R", {{u"B", 8}}}});
    p::FrameReader extraFramer({4096, 4105});
    const auto extraFrame = extraFramer.feed(extra);
    REQUIRE(extraFrame.frame);
    feed(late, extra);
    CHECK(late.finish() == p::TapeStatus::FAILED);
    CHECK(old.matches(late.schema().recordContext(0)));
    CHECK(late.constructionUnits() == previousCharge + extraFrame.frame->payload.size());

    // The unchanged-shape append still charges a full old source copy, both local copies and new plans.
    for (const auto delta : {0, 1, 2}) {
        ti::Limits bounds;
        bounds.tape.constructionUnits = 2 * completeCharge + 6 - 1 + delta;
        p::TapeReader reader(bounds.tape);
        feed(reader, tape);
        feed(reader, description);
        std::vector<std::byte> append;
        describe(append, std::vector<p::WireRecordDescription>{{8, u"R", {{u"A", 8}}}});
        feed(reader, append);
        CHECK((reader.finish() == p::TapeStatus::FINISHED) == (delta >= 1));
    }
}

TEST_CASE("wire inventory 48 ordinary descriptions retains all eight exact types") {
    constexpr std::array types{2, 8, 10, 40, 56, 72, 88, 120};
    std::vector<p::WireRecordDescription> declarations;

    for (std::int32_t index = 0; index < 48; ++index) {
        auto name = std::u16string(u"Record") + static_cast<char16_t>(u'A' + index);
        declarations.push_back({1000 - index, std::move(name), {}});

        for (std::size_t field = 0; field < types.size(); ++field) {
            declarations.back().fields.push_back(
                {std::u16string(1, static_cast<char16_t>(u'A' + field)), types[field]});
        }
    }

    for (const auto chunk : {std::size_t{1}, std::size_t{4096}}) {
        auto tape = header();
        describe(tape, declarations);
        p::TapeReader reader(ti::Limits{}.tape);
        feed(reader, tape, chunk);
        CHECK(reader.finish() == p::TapeStatus::FINISHED);
        REQUIRE(reader.schema().records().size() == 48);

        for (std::size_t index = 0; index < 48; ++index) {
            const auto &record = reader.schema().records()[index];
            CHECK(record.id == static_cast<std::int32_t>(index));
            CHECK(record.name == declarations[index].name);
            REQUIRE(record.fields.size() == types.size());

            for (std::size_t field = 0; field < types.size(); ++field) {
                CHECK(record.fields[field].serialTypeId == types[field]);
            }
        }
    }
}

TEST_CASE("wire schema names use UTF16 units and decoded records outlive their reader") {
    for (const auto nameLimit : {std::size_t{1}, std::size_t{2}, std::size_t{3}}) {
        ti::Limits limits;
        limits.tape.context.schema.nameUnits = nameLimit;
        limits.tape.context.schema.totalNameUnits = 4;
        p::TapeReader reader(limits.tape);
        auto tape = header();
        describe(tape, std::vector<p::WireRecordDescription>{{7, u"\U0001f600", {{u"\U0001f600", 8}}}});
        feed(reader, tape);
        CHECK((reader.finish() == p::TapeStatus::FINISHED) == (nameLimit >= 2));
    }

    std::optional<c::LocalSchema> retainedSchema;
    std::vector<p::StreamDataRecord> records;
    {
        p::TapeReader reader(ti::Limits{}.tape);
        auto tape = header();
        describe(tape, std::vector<p::WireRecordDescription>{{7, u"Owned", {{u"Text", 10}}}});
        data(tape, 7, {1, 33}); // Type-10 one-unit string: ASCII ! is also a one-byte compact value.
        records = feed(reader, tape);
        REQUIRE(reader.finish() == p::TapeStatus::FINISHED);
        auto copy = reader.schema().boundedCopy(ti::Limits{}.tape.context.schema);
        REQUIRE(copy);
        retainedSchema.emplace(std::move(*copy));
        tape.assign(tape.size(), std::byte{0});
    }
    REQUIRE(records.size() == 1);
    REQUIRE(records[0].typedValues.size() == 1);
    CHECK(records[0].typedValues[0].units == std::optional<std::u16string>(u"!"));
    CHECK(records[0].context.matches(retainedSchema->recordContext(0)));
    CHECK(retainedSchema->records()[0].name == u"Owned");
}

TEST_CASE("production Java mixed fixtures have identical owned values with wire or supplied schema") {
    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        const auto tape = fixture(mode == p::TapeMode::LEGACY ? "mixed-stream.qtp" : "mixed-raw.qtp");
        auto schema = ti::oracleFieldTypesSchema(ti::Limits{}.tape.context.schema);
        REQUIRE(schema);
        p::TapeReader explicitReader(*schema, ti::Limits{}.tape, mode);
        const auto expected = feed(explicitReader, tape);
        REQUIRE(explicitReader.finish() == p::TapeStatus::FINISHED);
        REQUIRE(!expected.empty());

        for (const auto chunk : {std::size_t{1}, std::size_t{4096}}) {
            p::TapeReader reader(ti::Limits{}.tape, mode);
            const auto actual = feed(reader, tape, chunk);
            CHECK(reader.finish() == p::TapeStatus::FINISHED);
            REQUIRE(actual.size() == expected.size());

            for (std::size_t index = 0; index < actual.size(); ++index) {
                CHECK(actual[index].senderId == expected[index].senderId);
                CHECK(actual[index].symbol.units == expected[index].symbol.units);
                REQUIRE(actual[index].typedValues.size() == expected[index].typedValues.size());

                for (std::size_t field = 0; field < actual[index].typedValues.size(); ++field) {
                    const auto &name = reader.schema().records()[0].lookups[field].name;
                    const auto &keys = schema->records()[0].lookups;
                    const auto key = std::find_if(keys.begin(), keys.end(), [&](const auto &entry) {
                        return entry.name == name;
                    });
                    REQUIRE(key != keys.end());
                    const auto &value = expected[index].typedValues[key->fieldId];
                    CHECK(actual[index].typedValues[field].type == value.type);
                    CHECK(actual[index].typedValues[field].raw == value.raw);
                    CHECK(actual[index].typedValues[field].units == value.units);
                }
            }
        }
    }
}

TEST_CASE("wire output publishes successful generations only and exposes source order and presence") {
    auto tape = header();
    describe(tape, std::vector<p::WireRecordDescription>{{90, u"R", {{u"A", 8}, {u"B", 10}}},
                                                         {7, u"R", {{u"B", 10}, {u"C", 40}, {u"A", 8}}}});
    data(tape, 90, {0, -1});
    data(tape, 7, {-1, 0, 0});
    describe(tape, {});
    std::string source(reinterpret_cast<const char *>(tape.data()), tape.size());
    std::istringstream input(source);
    std::ostringstream output, diagnostic;
    REQUIRE(ti::inspectWire(input, output, diagnostic, {}) == 0);
    const auto good = output.str();
    CHECK(good.starts_with("format\tqdcxx-tape-inspect\t7\tordinary-v1\tlegacy\n"));
    CHECK(good.find("schema-field\t1\t0\t2\t0043\t40\n") != std::string::npos);
    CHECK(good.find("wire-field\t1\t7\t0\t0042\t10\t1\n") != std::string::npos);
    CHECK(good.find("wire-field\t1\t7\t1\t0043\t40\t2\n") != std::string::npos);
    CHECK(good.find("wire-record\t1\t0\t90\t0\t0052\t2\n") != std::string::npos);
    CHECK(good.find("wire-data\t2\t0\t1\t90\t0\t0052\t-\t8:0:-,10:0:null,40:0:-") != std::string::npos);
    CHECK(good.find("wire-data\t3\t1\t1\t7\t0\t0052\t-\t8:0:-,10:0:null,40:0:-") != std::string::npos);
    CHECK(good.find("schema-end\t2\n") != std::string::npos);
    // Reconstruct the original source field sequence independently from metadata and projected values.
    std::map<std::int32_t, std::vector<std::pair<std::string, std::size_t>>> sourceFields;
    std::map<std::int32_t, std::vector<std::string>> reconstructed;
    std::istringstream rows(good);
    std::string line;

    while (std::getline(rows, line)) {
        const auto row = cells(line);

        if (row[0] == "wire-field" && row[1] == "1") {
            REQUIRE(row.size() == 7);
            const auto sender = std::stoi(row[2]);
            CHECK(std::stoul(row[3]) == sourceFields[sender].size());
            sourceFields[sender].push_back({row[4], std::stoul(row[6])});
        }

        if (row[0] == "wire-data") {
            REQUIRE(row.size() == 9);
            CHECK(row[3] == "1");
            const auto sender = std::stoi(row[4]);
            const auto values = cells(row[8], ',');

            for (const auto &[name, target] : sourceFields[sender]) {
                REQUIRE(target < values.size());
                reconstructed[sender].push_back(name + "=" + values[target]);
            }
        }
    }

    CHECK(reconstructed[90] == std::vector<std::string>{"0041=8:0:-", "0042=10:0:null"});
    CHECK(reconstructed[7] == std::vector<std::string>{"0042=10:0:null", "0043=40:0:-", "0041=8:0:-"});
    const auto afterFirstField = good.find('\n', good.find("schema-field")) + 1;
    PrefixBuffer physical(afterFirstField);
    std::ostream shortOutput(&physical);
    std::istringstream shortInput(source);
    std::ostringstream shortError;
    CHECK(ti::inspectWire(shortInput, shortOutput, shortError, {}) == 3);
    CHECK(physical.bytes.ends_with("schema-field\t1\t0\t0\t0041\t8\n"));
    CHECK(physical.bytes.find("schema-end") == std::string::npos);
    CHECK(physical.bytes.find("complete\t") == std::string::npos);
    std::vector<std::byte> bad;
    describe(bad, std::vector<p::WireRecordDescription>{{8, u"New", {{u"X", 8}}}, {9, u"R", {{u"A", 10}}}});
    source.append(reinterpret_cast<const char *>(bad.data()), bad.size());
    std::istringstream rejected(source);
    std::ostringstream prefix, error;
    CHECK(ti::inspectWire(rejected, prefix, error, {}) == 4);
    CHECK(prefix.str().find("schema-end\t3\n") == std::string::npos);
    CHECK(prefix.str().find("schema-record\t3\t") == std::string::npos);
    CHECK(prefix.str().find("complete\t") == std::string::npos);

    for (const auto allowance :
         {good.find("schema-field"), good.find("schema-end"), good.size() - 1, good.size(), good.size() + 1}) {
        ti::Limits limits;
        limits.output = allowance;
        std::istringstream bounded(std::string(reinterpret_cast<const char *>(tape.data()), tape.size()));
        std::ostringstream rows, errors;
        CHECK(ti::inspectWire(bounded, rows, errors, limits) == (allowance >= good.size() ? 0 : 6));
        CHECK(rows.str().size() <= allowance);

        if (allowance < good.size()) {
            CHECK(rows.str().find("complete\t") == std::string::npos);
        }
    }
}

TEST_CASE("wire discovery allocation failures retain the previously published immutable snapshot") {
    bool succeeded = false;
    std::size_t failures = 0;

    for (std::ptrdiff_t checkpoint = 0; checkpoint < 200 && !succeeded; ++checkpoint) {
        p::TapeReader reader(ti::Limits{}.tape);
        auto initial = header();
        describe(initial,
                 std::vector<p::WireRecordDescription>{{90, u"RecordWithAnOwnedName", {{u"OriginalOwnedField", 8}}}});
        feed(reader, initial);
        const auto identity = reader.schema().recordContext(0);
        const auto *oldContext = reader.context();
        const auto *oldDescriptions = reader.descriptions().data();
        std::vector<std::byte> append;
        describe(append,
                 std::vector<p::WireRecordDescription>{
                     {7, u"RecordWithAnOwnedName", {{u"AdditionalOwnedField", 8}, {u"OriginalOwnedField", 8}}}});
        bool escaped = false;
        p::TapeFeedResult result;
        allocationFailAfter = checkpoint;
        try {
            result = reader.feed(append);
        } catch (const std::bad_alloc &) {
            escaped = true;
        }
        allocationFailAfter = -1;
        CHECK_FALSE(escaped);

        if (result.status == p::TapeStatus::FAILED) {
            ++failures;
            CHECK(reader.diagnostic().error == p::TapeError::ALLOCATION_FAILURE);
            CHECK(identity.matches(reader.schema().recordContext(0)));
            CHECK(reader.context() == oldContext);
            CHECK(reader.descriptions().data() == oldDescriptions);
            CHECK(reader.schema().records()[0].fields.size() == 1);
            CHECK(reader.descriptions().size() == 1);
        } else {
            succeeded = true;
            CHECK(reader.schema().records()[0].fields.size() == 2);
            CHECK(reader.descriptions().size() == 2);
            CHECK_FALSE(identity.matches(reader.schema().recordContext(0)));
            CHECK(reader.finish() == p::TapeStatus::FINISHED);
        }
    }

    CHECK(succeeded);
    CHECK(failures > 10);
}

TEST_CASE("wire inspection preserves IO failures and safe human generation metadata") {
    using org::ttldtor::qdcxx::test_support::FaultBuffer;
    using org::ttldtor::qdcxx::test_support::FaultOperation;

    for (const auto operation : {FaultOperation::READ, FaultOperation::WRITE, FaultOperation::FLUSH}) {
        FaultBuffer fault(operation);
        std::istream brokenInput(&fault);
        std::ostream brokenOutput(&fault);
        const auto tape = header();
        std::istringstream input(std::string(reinterpret_cast<const char *>(tape.data()), tape.size()));
        std::ostringstream output, errors;
        CHECK(ti::inspectWire(operation == FaultOperation::READ ? brokenInput : input,
                              operation == FaultOperation::READ ? output : brokenOutput, errors, {}) == 3);
    }

    auto tape = header();
    describe(tape, std::vector<p::WireRecordDescription>{{90, u"R\n\"", {{u"A\t", 8}}}});
    data(tape, 90, {42});
    std::istringstream input(std::string(reinterpret_cast<const char *>(tape.data()), tape.size()));
    std::ostringstream output, errors;
    CHECK(ti::inspectWire(input, output, errors, {}, display::OutputFormat::HUMAN) == 0);
    CHECK(output.str().find("\"R\\n\\\"\"") != std::string::npos);
    CHECK(output.str().find("\"A\\t\"") != std::string::npos);
    CHECK(output.str().find("schema-end 1\n") != std::string::npos);
    auto raw = header(p::TapeMode::RAW_MESSAGE_V1);
    describe(raw, std::vector<p::WireRecordDescription>{{90, u"R", {{u"A", 8}}}});
    data(raw, 90, {42}, p::TapeMode::RAW_MESSAGE_V1);
    std::istringstream rawInput(std::string(reinterpret_cast<const char *>(raw.data()), raw.size()));
    std::ostringstream rawOutput, rawError;
    CHECK(ti::inspectWire(rawInput, rawOutput, rawError, {}, display::OutputFormat::TSV, 2,
                          p::TapeMode::RAW_MESSAGE_V1) == 0);
    CHECK(rawOutput.str().find("wire-data\t2\t0\t1\t90\t0\t0052\t-\t8:42:-\t-\t0\t0\n") != std::string::npos);
}

/** Run focused checks with an explicit repository path.
 * @param argc Argument count.
 * @param argv Repository root followed by optional doctest arguments.
 * @return Nonzero for missing root or failed checks.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    root = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
