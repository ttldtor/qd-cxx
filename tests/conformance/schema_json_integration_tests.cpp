// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "../../tools/tape-inspect/schema_input.hpp"
#include "event_oracle.hpp"
#include "fault_buffer.hpp"
#include "qd/encoding/compact.hpp"
#include "qd/encoding/utf_string.hpp"
#include "qd/schema/json_schema.hpp"
#include <doctest/doctest.h>
#include <sstream>

namespace {
namespace c = org::ttldtor::qdcxx::core;
namespace e = org::ttldtor::qdcxx::events;
namespace enc = org::ttldtor::qdcxx::encoding;
namespace p = org::ttldtor::qdcxx::protocol;
namespace s = org::ttldtor::qdcxx::schema;
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace display = org::ttldtor::qdcxx::inspect;
namespace o = event_oracle;
using org::ttldtor::qdcxx::test_support::FaultBuffer;
using org::ttldtor::qdcxx::test_support::FaultOperation;
std::filesystem::path root; ///< Repository root supplied by CTest, without ambient source lookup.

/** Load a reusable example and destroy/overwrite all input storage before returning.
 * @param name Native example filename.
 * @return Independently owned validated schema.
 */
c::LocalSchema load(std::string_view name = "mixed.json") {
    std::ifstream input(root / "spec/schema/examples/native" / name, std::ios::binary);
    REQUIRE(input.good());
    auto text = ti::readSchemaDocument(input, 65536);
    REQUIRE(text);
    auto schema = s::loadJsonSchema(*text);
    REQUIRE(schema);
    text->assign(text->size(), 'x');

    return std::move(*schema);
}

/** Decode independently of input/reader lifetime with one-byte or whole-file scheduling.
 * @param schema Explicit loaded context.
 * @param bytes Borrowed finite tape.
 * @param mode Explicit container policy.
 * @param chunk Positive maximum input chunk.
 * @return Owned generic records after actual EOF.
 */
std::vector<p::StreamDataRecord> decode(const c::LocalSchema &schema, std::span<const std::byte> bytes,
                                        p::TapeMode mode, std::size_t chunk) {
    ti::Limits limits;
    p::TapeReader reader(schema, limits.tape, mode);
    std::vector<p::StreamDataRecord> records;

    while (!bytes.empty()) {
        auto result = reader.feed(bytes.first(std::min(chunk, bytes.size())));
        REQUIRE(result.status != p::TapeStatus::FAILED);
        REQUIRE(result.accepted > 0);
        bytes = bytes.subspan(result.accepted);

        for (auto &record : result.records) {
            records.push_back(std::move(record));
        }
    }

    REQUIRE(reader.finish() == p::TapeStatus::FINISHED);

    return records;
}

/** Append a canonical compact integer to a designed test tape.
 * @param out Owned destination.
 * @param value Exact signed primitive.
 */
void compact(std::vector<std::byte> &out, std::int64_t value) {
    std::array<std::byte, 9> buffer{};
    const auto written = enc::writeCompactLong(value, buffer);
    REQUIRE(written.status == enc::Status::OK);
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(written.written));
}

/** Append a bounded UTFString name to a designed description.
 * @param out Owned destination.
 * @param value Exact field or record name.
 */
void text(std::vector<std::byte> &out, std::u16string_view value) {
    std::array<std::byte, 256> buffer{};
    const auto written = enc::writeUtfString(value, buffer, {256, 64});
    REQUIRE(written.status == enc::Status::OK);
    out.insert(out.end(), buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(written.written));
}

/** Append one complete designed envelope.
 * @param out Owned tape.
 * @param type Explicit message identity.
 * @param body Payload excluding type and length.
 */
void message(std::vector<std::byte> &out, std::int32_t type, std::span<const std::byte> body) {
    std::vector<std::byte> contents;
    compact(contents, type);
    contents.insert(contents.end(), body.begin(), body.end());
    compact(out, static_cast<std::int64_t>(contents.size()));
    out.insert(out.end(), contents.begin(), contents.end());
}

/** Generate a compact offline witness of generic records and extra-field retention.
 * @param mode Existing container policy; only its original descriptor is reused.
 * @return Three designed records, distinct from Java event compatibility evidence.
 */
std::vector<std::byte> mixedTape(p::TapeMode mode) {
    const auto fixture = mode == p::TapeMode::LEGACY ? "mixed-stream.qtp" : "mixed-raw.qtp";
    auto tape = o::bytes(root / "tests/fixtures/encoding/field-types/expected" / fixture);
    p::FrameReader frames({65536, 65545});
    auto descriptor = frames.feed(tape);
    REQUIRE(descriptor.frame);
    REQUIRE(descriptor.frame->type == 1);
    tape.resize(descriptor.accepted);
    const std::array<p::WireRecordDescription, 3> declarations{
        {{7, u"Custom", {{u"Count", 8}, {u"Text", 10}}},
         {8, u"Quote", {{u"ExtraText", 10}}},
         {9, u"Profile", {{u"Description", 10}, {u"ExtraShort", 40}}}}};
    std::vector<std::byte> body;

    for (const auto &record : declarations) {
        compact(body, record.senderId);
        text(body, record.name);
        compact(body, static_cast<std::int64_t>(record.fields.size()));

        for (const auto &field : record.fields) {
            text(body, field.name);
            compact(body, field.serialTypeId);
        }
    }

    message(tape, 2, body); // DESCRIBE_RECORDS.
    body.clear();
    // Each record uses a fresh empty literal symbol. Type-10 lengths are unit counts.
    body.insert(body.end(), {std::byte{0xfc}, std::byte{0}, std::byte{7}});
    compact(body, 42);
    compact(body, -1); // Custom.Text is null.
    body.insert(body.end(), {std::byte{0xfc}, std::byte{0}, std::byte{8}});
    compact(body, 0); // Quote.ExtraText is present and empty.
    body.insert(body.end(), {std::byte{0xfc}, std::byte{0}, std::byte{9}});
    compact(body, 1);
    body.push_back(std::byte{'X'});
    compact(body, -7); // Preserve a signed ShortString raw value without interpreting it.
    message(tape, mode == p::TapeMode::LEGACY ? 15 : 5, body);

    return tape;
}

/** Stream whose content keeps growing whenever the reader asks for more.
 * No seek or initial size can establish its eventual length.
 */
class GrowingBuffer : public std::streambuf {
    std::size_t requested_ = 0; ///< Count of bytes requested from the producer.

    protected:
    /** Produce every requested byte without establishing an EOF.
     * @param destination Borrowed writable read buffer.
     * @param count Requested bytes.
     * @return Complete requested size.
     */
    std::streamsize xsgetn(char *destination, std::streamsize count) override {
        requested_ += static_cast<std::size_t>(count);
        std::fill_n(destination, count, 'x');

        return count;
    }

    /** Produce an additional byte when probed at the admitted boundary.
     * @return A non-EOF byte without creating a backing infinite allocation.
     */
    int_type underflow() override {
        ++requested_;

        return traits_type::to_int_type('x');
    }

    public:
    /** Observe the total producer requests.
     * @return Read requests plus boundary probes.
     */
    std::size_t requested() const noexcept {
        return requested_;
    }
};
} // namespace

TEST_CASE("loaded mixed schemas prepare explicit event IDs with independent owned contexts") {
    auto schema = load();
    auto other = load();
    REQUIRE(schema.records().size() == 3);
    CHECK(schema.records()[0].name == u"Custom");
    CHECK(schema.records()[0].lookups[2].name == u"Text.\U0001f600");
    auto quote = e::PreparedQuote::prepare(schema, 1, o::mappingLimits());
    auto profile = e::PreparedProfile::prepare(schema, 2, o::mappingLimits());
    auto otherQuote = e::PreparedQuote::prepare(other, 1, o::mappingLimits());
    auto otherProfile = e::PreparedProfile::prepare(other, 2, o::mappingLimits());
    REQUIRE(quote);
    REQUIRE(profile);
    REQUIRE(otherQuote);
    REQUIRE(otherProfile);
    CHECK_FALSE(quote->context().matches(other.recordContext(1)));
    CHECK_FALSE(profile->context().matches(other.recordContext(2)));

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        auto records = decode(schema, mixedTape(mode), mode, 1);
        REQUIRE(records.size() == 3);
        auto projectedQuote = quote->map(u"S", {records[1].context, records[1].typedValues}, 8);
        auto projectedProfile = profile->map(u"S", {records[2].context, records[2].typedValues}, {8, 8, 16});
        REQUIRE(projectedQuote);
        REQUIRE(projectedProfile);
        CHECK(projectedQuote->bidTimeMs == 0);
        CHECK(projectedQuote->timeNanoPart == 0);
        CHECK(projectedProfile->description == u"X");
        CHECK_FALSE(projectedProfile->statusReason);
        CHECK(records[1].typedValues[1].units == u"");
        CHECK(records[2].typedValues[2].raw == -7);
        const auto wrongQuote = quote->map(u"S", {other.recordContext(1), records[1].typedValues}, 8);
        const auto wrongProfile = profile->map(u"S", {other.recordContext(2), records[2].typedValues}, {8, 8, 16});
        REQUIRE_FALSE(wrongQuote);
        REQUIRE_FALSE(wrongProfile);
        CHECK(wrongQuote.error() == e::MappingError::WRONG_CONTEXT);
        CHECK(wrongProfile.error() == e::MappingError::WRONG_CONTEXT);
        CHECK(otherQuote->map(u"S", {other.recordContext(1), records[1].typedValues}, 8));
        CHECK(otherProfile->map(u"S", {other.recordContext(2), records[2].typedValues}, {8, 8, 16}));
    }
}

TEST_CASE("loaded reordered fields preserve selected Java event values through original tapes") {
    const auto schema = load();
    const auto directory = root / "tests/fixtures/events/mapping/expected/default";
    const auto oracle = o::read(directory / "observations.tsv");
    const auto quote = e::PreparedQuote::prepare(schema, 1, o::mappingLimits());
    const auto profile = e::PreparedProfile::prepare(schema, 2, o::mappingLimits());
    REQUIRE(quote);
    REQUIRE(profile);

    for (const std::string name : {"Quote", "Profile"}) {
        for (const std::string suffix : {"stream", "raw"}) {
            const auto mode = suffix == "raw" ? p::TapeMode::RAW_MESSAGE_V1 : p::TapeMode::LEGACY;
            const auto input = o::bytes(directory / (name + "-" + suffix + ".qtp"));
            std::vector<const o::Observation *> expected;

            for (const auto &observation : oracle.observations) {
                if (observation.phase == name + "-" + suffix + "-read") {
                    expected.push_back(&observation);
                }
            }

            REQUIRE(expected.size() == 3);

            for (const auto chunk : {std::size_t{1}, input.size()}) {
                const auto records = decode(schema, input, mode, chunk);
                REQUIRE(records.size() == expected.size());

                for (std::size_t i = 0; i < records.size(); ++i) {
                    const auto &record = records[i];
                    const auto &observed = *expected[i];
                    CHECK(record.symbol.units == o::units(observed.event[0]));
                    REQUIRE(record.typedValues.size() == 6);

                    if (name == "Quote") {
                        REQUIRE(record.localId == 1);
                        auto mapped = quote->map(record.symbol.units, {record.context, record.typedValues}, 1024);
                        REQUIRE(mapped);
                        const auto properties = o::properties(*mapped);

                        for (const auto column : {3U, 4U, 7U, 8U}) {
                            CHECK(properties[column] == observed.event[column]);
                        }

                        CHECK(record.typedValues[0] == observed.values[7]);
                        CHECK(record.typedValues[2] == observed.values[2]);
                        CHECK_FALSE(record.typedValues[1].units);
                        CHECK(mapped->bidTimeMs == 0); // Optional field absent from this local declaration.
                    } else {
                        REQUIRE(record.localId == 2);
                        auto mapped =
                            profile->map(record.symbol.units, {record.context, record.typedValues}, {1024, 1024, 4096});
                        REQUIRE(mapped);
                        const auto properties = o::properties(*mapped);

                        for (const auto column : {1U, 5U, 14U, 15U, 16U, 17U, 18U}) {
                            CHECK(properties[column] == observed.event[column]);
                        }

                        CHECK(record.typedValues[0] == observed.values[15]);
                        CHECK(record.typedValues[3] == observed.values[14]);
                        CHECK(record.typedValues[2].raw == 0);
                        CHECK(mapped->haltStartTimeMs == 0);
                    }
                }
            }
        }
    }
}

TEST_CASE("external generic output keeps custom records extra fields and null empty distinctions") {
    const auto schema = load();

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        const auto bytes = mixedTape(mode);
        const std::string tape(reinterpret_cast<const char *>(bytes.data()), bytes.size());

        for (const auto format : {display::OutputFormat::TSV, display::OutputFormat::HUMAN}) {
            ti::Limits limits;
            limits.chunkSize = 1;
            std::istringstream input(tape);
            std::ostringstream output, error;
            REQUIRE(ti::inspect(input, output, error, schema, limits, format,
                                display::DisplayProfile::EXTERNAL_SCHEMA_V1, 2, mode) == 0);
            CHECK(error.str().empty());
            const auto rendered = output.str();
            CHECK(rendered.find("external-schema-v1") != rendered.npos);

            if (format == display::OutputFormat::TSV) {
                CHECK(rendered.starts_with("format\tqdcxx-tape-inspect\t6\t"));
                CHECK(rendered.find("\t0\t0043007500730074006f006d\t-\t8:42:-,10:0:null,40:0:-,2:0:-,56:0:-,72:0:-,88:"
                                    "0:-,120:0:-\n") != rendered.npos);
                CHECK(rendered.find(
                          "\t1\t00510075006f00740065\t-\t120:0:-,10:0:empty,120:0:-,120:0:-,120:0:-,72:0:-\n") !=
                      rendered.npos);
                CHECK(rendered.find(
                          "\t2\t00500072006f00660069006c0065\t-\t10:0:null,120:0:-,40:-7:-,10:0:0058,8:0:-,88:0:-\n") !=
                      rendered.npos);
                CHECK(rendered.find("quote-record\t") == rendered.npos);
                CHECK(rendered.find("profile-record\t") == rendered.npos);
            } else {
                CHECK(rendered.find("name=\"Custom\"") != rendered.npos);
                CHECK(rendered.find("name=\"Quote\"") != rendered.npos);
                CHECK(rendered.find("name=\"Profile\"") != rendered.npos);
                CHECK(rendered.find("field[1]=(type:10 raw:0 units:null)") != rendered.npos);
                CHECK(rendered.find("field[1]=(type:10 raw:0 units:\"\")") != rendered.npos);
                CHECK(rendered.find("field[2]=(type:40 raw:-7)") != rendered.npos);
            }

            for (const auto delta : {0U, 1U}) {
                limits.output = rendered.size() - delta;
                std::istringstream retry(tape);
                std::ostringstream bounded, diagnostic;
                CHECK(ti::inspect(retry, bounded, diagnostic, schema, limits, format,
                                  display::DisplayProfile::EXTERNAL_SCHEMA_V1, 2, mode) == (delta == 0 ? 0 : 6));
                CHECK(bounded.str().size() <= limits.output);
            }

            for (const auto operation : {FaultOperation::WRITE, FaultOperation::FLUSH}) {
                FaultBuffer failed(operation);
                std::ostream destination(&failed);
                std::istringstream retry(tape);
                std::ostringstream diagnostic;
                CHECK(ti::inspect(retry, destination, diagnostic, schema, {}, format,
                                  display::DisplayProfile::EXTERNAL_SCHEMA_V1, 2, mode) == 3);
            }
        }
    }
}

TEST_CASE("external field-types example preserves the original generic oracle tape rows") {
    const auto schema = load("field-types.json");
    auto builtin = ti::oracleFieldTypesSchema(ti::Limits{}.tape.context.schema);
    REQUIRE(builtin);

    for (const auto mode : {p::TapeMode::LEGACY, p::TapeMode::RAW_MESSAGE_V1}) {
        const auto fixture = mode == p::TapeMode::LEGACY ? "mixed-stream.qtp" : "mixed-raw.qtp";
        const auto bytes = o::bytes(root / "tests/fixtures/encoding/field-types/expected" / fixture);
        const std::string tape(reinterpret_cast<const char *>(bytes.data()), bytes.size());

        for (const auto format : {display::OutputFormat::TSV, display::OutputFormat::HUMAN}) {
            std::istringstream externalInput(tape), builtinInput(tape);
            std::ostringstream externalOutput, builtinOutput, externalError, builtinError;
            ti::Limits limits;
            limits.chunkSize = 1;
            REQUIRE(ti::inspect(externalInput, externalOutput, externalError, schema, limits, format,
                                display::DisplayProfile::EXTERNAL_SCHEMA_V1, 2, mode) == 0);
            REQUIRE(ti::inspect(builtinInput, builtinOutput, builtinError, *builtin, limits, format,
                                display::DisplayProfile::ORACLE_FIELD_TYPES_V1, 2, mode) == 0);
            CHECK(externalError.str().empty());
            CHECK(builtinError.str().empty());
            const auto external = externalOutput.str();
            const auto baseline = builtinOutput.str();
            REQUIRE(external.find('\n') != external.npos);
            REQUIRE(baseline.find('\n') != baseline.npos);
            CHECK(external.substr(external.find('\n') + 1) == baseline.substr(baseline.find('\n') + 1));
        }
    }
}

TEST_CASE("schema acquisition bounds exact zero exceeded failing and growing streams") {
    for (const std::string text : std::vector<std::string>{"", "abc", std::string(8192, 'x')}) {
        std::istringstream input(text);
        const auto exact = ti::readSchemaDocument(input, text.size());
        REQUIRE(exact);
        CHECK(*exact == text);

        if (!text.empty()) {
            std::istringstream shortInput(text);
            const auto exceeded = ti::readSchemaDocument(shortInput, text.size() - 1);
            REQUIRE_FALSE(exceeded);
            CHECK(exceeded.error() == ti::SchemaInputError::RESOURCE_LIMIT);
        }
    }

    std::istringstream nonempty("x");
    CHECK(ti::readSchemaDocument(nonempty, 0).error() == ti::SchemaInputError::RESOURCE_LIMIT);

    for (const bool exceptions : {false, true}) {
        FaultBuffer failed(FaultOperation::READ);
        std::istream input(&failed);

        if (exceptions) {
            input.exceptions(std::ios::badbit);
        }

        const auto result = ti::readSchemaDocument(input, 8);
        REQUIRE_FALSE(result);
        CHECK(result.error() == ti::SchemaInputError::IO_ERROR);
    }

    GrowingBuffer growing;
    std::istream input(&growing);
    const auto result = ti::readSchemaDocument(input, 4097);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ti::SchemaInputError::RESOURCE_LIMIT);
    CHECK(growing.requested() == 4098);
}

/** Run integration cases with an explicit repository root.
 * @param argc Argument count.
 * @param argv Repository root followed by optional doctest arguments.
 * @return Nonzero on a missing root or failed checks.
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
