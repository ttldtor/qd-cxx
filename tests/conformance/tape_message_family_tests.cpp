// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "../../tools/tape-inspect/inspect.hpp"
#include "indexed_test_support.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>

namespace {
using namespace indexed_test;
namespace ti = org::ttldtor::qdcxx::tape_inspect;
namespace view = org::ttldtor::qdcxx::inspect;
std::filesystem::path fixtureRoot; ///< Governed production dispatch captures supplied by CTest.

/** Finite independent payload, timing and metadata limits for the broader protocol tests. */
p::TapeLimits broadLimits() {
    auto result = limits();
    result.data.fieldString = {65536, 4096};
    result.data.fieldBytes = result.data.fieldUnits = result.data.ownedFieldUnits = 1048576;
    result.data.fieldPayloadEncodedBytes = result.data.fieldPayloadBytes = 65536;
    result.data.payloadEncodedBytes = result.data.payloadWorkBytes = result.data.ownedPayloadBytes = 1048576;

    return result;
}

/** Construct a designed binary full/delta descriptor with exact known or refused properties. */
std::vector<std::byte> protocol(std::initializer_list<std::pair<std::u16string_view, std::u16string_view>> properties,
                                std::initializer_list<std::pair<std::int32_t, std::u16string_view>> sends = {}) {
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    compact(body, static_cast<std::int64_t>(properties.size()));

    for (const auto &[key, value] : properties) {
        text(body, key);
        text(body, value);
    }

    compact(body, static_cast<std::int64_t>(sends.size()));

    for (const auto &[id, name] : sends) {
        compact(body, id);
        text(body, name);
        compact(body, 0);
    }

    compact(body, 0);

    return frame(1, body);
}

/** One complete scalar/byte/UTFString record, independent of its data family. */
std::vector<std::byte> mixed(std::int32_t family, bool removal = false) {
    std::vector<std::byte> body;

    if (removal) {
        body.push_back(std::byte{0xf9});
        compact(body, 2);
    }

    body.push_back(std::byte{0xfe});
    compact(body, 7);

    if (removal) {
        compact(body, 0);
    } else {
        compact(body, 42);
        compact(body, 3);
        body.insert(body.end(), {std::byte{0}, std::byte{0xff}, std::byte{0x41}});
        text(body, u"text");
    }

    return frame(family, body);
}

/** Append complete frames to an owned binary input for inspection. */
void append(std::string &output, const std::vector<std::byte> &bytes) {
    output.append(reinterpret_cast<const char *>(bytes.data()), bytes.size());
}

/** Build the explicit binary schema-discovery record used by every family. */
std::array<p::WireRecordDescription, 1> declaration() {
    return {{{7, u"Generic", {{u"N", 8}, {u"Blob", 9}, {u"Text", 137}}}}};
}
} // namespace

TEST_CASE("all four families share owning values flags removal and original envelope identity") {
    std::vector<p::StreamDataRecord> retained;

    for (const auto family : {5, 10, 15, 20}) {
        p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        accept(reader, descriptions(declaration()));
        const auto result = accept(reader, mixed(family));
        REQUIRE(result.frame);
        CHECK(result.frame->type == family);
        REQUIRE(result.records.size() == 1);
        const auto &record = result.records[0];
        REQUIRE(record.typedValues.size() == 3);
        CHECK(record.typedValues[0].raw == 42);
        CHECK(record.typedValues[1].bytes ==
              std::optional{std::vector{std::byte{0}, std::byte{0xff}, std::byte{0x41}}});
        CHECK(record.typedValues[2].units == std::optional{std::u16string(u"text")});
        CHECK(reader.schema().records()[0].hasTime == false);
        CHECK(reader.processedRecords() == 1);
        const auto remove = accept(reader, mixed(family, true));
        REQUIRE(remove.records.size() == 1);
        CHECK(remove.records[0].symbol.eventFlags == 2);
        CHECK(remove.records[0].removalTime == 0);
        CHECK_FALSE(remove.records[0].typedValues[1].bytes);
        CHECK_FALSE(remove.records[0].typedValues[2].units);
        retained.push_back(record);
        CHECK(reader.finish() == p::TapeStatus::FINISHED);
    }

    for (const auto &record : retained) {
        CHECK(record.context.valid());
        CHECK(record.typedValues[1].bytes->size() == 3);
        CHECK(*record.typedValues[2].units == u"text");
    }
}

TEST_CASE("headerless authentication requires committed nonempty descriptions and physical EOF") {
    for (const auto &initial :
         {std::vector<std::byte>{}, std::vector<std::byte>{std::byte{0}}, frame(0, {}), frame(2, {})}) {
        p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);

        if (!initial.empty()) {
            accept(reader, initial);
        }

        CHECK(reader.finish() == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::MALFORMED);
    }

    p::TapeReader described(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    accept(described, descriptions(declaration()));
    const auto identity = described.schema().recordContext(0);
    accept(described, frame(2, {}));
    CHECK(described.schema().recordContext(0).matches(identity));
    CHECK(described.finish() == p::TapeStatus::FINISHED);
    p::TapeReader unknown(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    CHECK(unknown.feed(mixed(15)).status == p::TapeStatus::FAILED);
    CHECK(unknown.diagnostic().error == p::TapeError::MALFORMED);
}

TEST_CASE("protocol deltas preserve timing and schema identity with finite strict refusals") {
    p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    accept(reader, protocol({{u"type", u"tape"}, {u"version", u""}, {u"time", u"MESSAGE"}, {u"opt", u"hs"}},
                            {{10, u"TICKER_DATA"}}));
    std::vector<std::byte> clock;
    compact(clock, 1);
    compact(clock, -1);
    accept(reader, frame(0, clock));
    accept(reader, descriptions(declaration()));
    const auto identity = reader.schema().recordContext(0);
    accept(reader, protocol({{u"version", u"next"}, {u"time", u"NONE"}}, {{20, u"HISTORY_DATA"}}));
    CHECK(reader.descriptor()->version == u"next");
    CHECK(reader.descriptor()->explicitTickerSend);
    CHECK(reader.descriptor()->explicitHistorySend);
    CHECK(reader.schema().recordContext(0).matches(identity));
    const auto result = accept(reader, mixed(5));
    REQUIRE(result.timing);
    CHECK(result.timing->timestampMs == -1);
    CHECK(reader.finish() == p::TapeStatus::FINISHED);

    for (const auto &refused :
         {protocol({{u"type", u"tape"}, {u"unknown", u"value"}}),
          protocol({{u"type", u"tape"}, {u"version", u"a"}, {u"version", u"b"}}),
          protocol({{u"type", u"tape"}, {u"time", u"FIELD"}}), protocol({{u"type", u"tape"}, {u"opt", u"hs,hs"}}),
          protocol({{u"type", u"tape"}}, {{-1, u"STREAM_DATA"}})}) {
        p::TapeReader guarded(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        CHECK(guarded.feed(refused).status == p::TapeStatus::FAILED);
        CHECK(guarded.diagnostic().error == p::TapeError::UNSUPPORTED);
    }

    for (const auto kind : {4, 11, 12, 16, 17, 21, 22, 49, 50, 51, 52, 53, 54, 55, 56, 61, 70, 75, 139, 999}) {
        p::TapeReader guarded(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        CHECK(guarded.feed(frame(kind, {})).status == p::TapeStatus::FAILED);
        CHECK(guarded.diagnostic().rejection == p::TapeRejection::MESSAGE);
    }
}

TEST_CASE("retired discovered fields retain full types and failed replacements retain borrowed addresses") {
    p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    accept(reader, descriptions(declaration()));
    const std::array<p::WireRecordDescription, 1> retired{{{7, u"Other", {{u"X", 8}}}}};
    accept(reader, descriptions(retired));
    REQUIRE(reader.schema().records().size() == 2);
    const auto before = reader.schema().recordContext(0);
    const auto *old = reader.schema().records().data();
    const std::array<p::WireRecordDescription, 1> conflicting{{{7, u"Generic", {{u"Blob", 233}}}}};
    CHECK(reader.feed(descriptions(conflicting)).status == p::TapeStatus::FAILED);
    CHECK(reader.diagnostic().rejection == p::TapeRejection::TYPE);
    CHECK(reader.schema().recordContext(0).matches(before));
    CHECK(reader.schema().records().data() == old);
}

TEST_CASE("constructor and large UTFString prefix respect finite admission before proportional allocation") {
    auto zero = broadLimits();
    zero.snapshotUnits = 0;
    CHECK_THROWS_AS(p::TapeReader(zero, p::TapeDiscovery::BINARY_CATALOG_V1), std::invalid_argument);
    auto one = broadLimits();
    one.snapshotUnits = one.constructionUnits = 1;
    p::TapeReader minimal(one, p::TapeDiscovery::BINARY_CATALOG_V1);
    CHECK(minimal.schema().records().empty());
    CHECK(minimal.constructionUnits() == 1);
    auto huge = broadLimits();
    huge.descriptor.text.bodyBytes = std::numeric_limits<std::size_t>::max();
    huge.descriptor.text.codeUnits = 8;
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};
    compact(body, 1);
    compact(body, 2147483648LL);
    body.push_back(std::byte{'x'});
    p::TapeReader guarded(huge, p::TapeDiscovery::BINARY_CATALOG_V1);
    CHECK(guarded.feed(frame(1, body)).status == p::TapeStatus::FAILED);
    CHECK(guarded.diagnostic().error == p::TapeError::MALFORMED);
}

TEST_CASE("v8 generations follow publication and fixed fourteen cells preserve bytes and removal zero") {
    ti::Limits options;
    options.tape = broadLimits();
    std::string tape;
    append(tape, frame(2, {}));
    append(tape, descriptions(declaration()));
    append(tape, frame(2, {}));
    append(tape, mixed(15));
    append(tape, descriptions(declaration()));
    append(tape, mixed(20, true));
    std::istringstream input(tape);
    std::ostringstream output, error;
    CHECK(ti::inspectBinaryWire(input, output, error, options) == 0);
    CHECK(error.str().empty());
    std::istringstream lines(output.str());
    std::string line;
    std::size_t schemas = 0, records = 0;

    while (std::getline(lines, line)) {
        if (line.starts_with("schema\t")) {
            ++schemas;
        }

        if (line.starts_with("wire-data-v8\t")) {
            ++records;
            CHECK(std::count(line.begin(), line.end(), '\t') == 13);

            if (records == 1) {
                CHECK(line.find("9:0:-:00ff41") != std::string::npos);
                CHECK(line.find("137:0:0074006500780074:-") != std::string::npos);
            } else {
                CHECK(line.ends_with("\t2\t0"));
                CHECK(line.find("9:0:-:null") != std::string::npos);
            }
        }
    }

    CHECK(schemas == 2);
    CHECK(records == 2);
    CHECK(output.str().starts_with("format\tqdcxx-tape-inspect\t8\tbinary-catalog-v1\tbinary-catalog-v1\n"));
    CHECK(output.str().find("complete\t") != std::string::npos);
}

TEST_CASE("physical partial frames wait while complete malformed primitives fail with payload coordinates") {
    const auto bytes = mixed(15);

    for (std::size_t cut = 1; cut < bytes.size(); ++cut) {
        p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        accept(reader, descriptions(declaration()));
        CHECK(reader.feed(std::span(bytes).first(cut)).status == p::TapeStatus::NEED_INPUT);
        CHECK(reader.finish() == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().stage == "framing");
    }

    p::TapeReader complete(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    accept(complete, descriptions(declaration()));
    const std::array invalid{std::byte{0xfe}};
    CHECK(complete.feed(frame(15, invalid)).status == p::TapeStatus::FAILED);
    CHECK(complete.diagnostic().payloadStart.has_value());
    CHECK(complete.diagnostic().stage == "data");
}

TEST_CASE("descriptor peak includes old candidate and temporary owners while repeated work never refunds") {
    const auto initial = protocol({{u"type", u"tape"}});

    // Independent units: initial S1 + candidate H1 + key actual5 + value reservation6 =13 peak.
    // Work is initial S1 + payload17 + candidate1 + key reservation6 + value reservation6 =31.
    for (const auto maximum : {std::size_t{12}, std::size_t{13}, std::size_t{14}}) {
        auto bounds = broadLimits();
        bounds.snapshotUnits = maximum;
        p::TapeReader reader(bounds, p::TapeDiscovery::BINARY_CATALOG_V1);
        const auto result = reader.feed(initial);
        CHECK(result.status == (maximum < 13 ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));

        if (maximum >= 13) {
            CHECK(reader.constructionUnits() == 31);
        } else {
            CHECK_FALSE(reader.descriptor());
            CHECK(reader.diagnostic().error == p::TapeError::RESOURCE_LIMIT);
        }
    }

    for (const auto maximum : {std::size_t{30}, std::size_t{31}, std::size_t{32}}) {
        auto bounds = broadLimits();
        bounds.constructionUnits = maximum;
        p::TapeReader reader(bounds, p::TapeDiscovery::BINARY_CATALOG_V1);
        CHECK(reader.feed(initial).status == (maximum < 31 ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));
    }

    const auto version = protocol({{u"version", u"A"}});

    // Published S1+H3, candidate H3, key actual8, value reservation3 =18 peak.
    for (const auto maximum : {std::size_t{17}, std::size_t{18}, std::size_t{19}}) {
        auto bounds = broadLimits();
        bounds.snapshotUnits = maximum;
        p::TapeReader reader(bounds, p::TapeDiscovery::BINARY_CATALOG_V1);
        accept(reader, initial);
        accept(reader, version);
        const auto *prior = reader.descriptor()->version->data();
        const auto result = reader.feed(version);
        CHECK(result.status == (maximum < 18 ? p::TapeStatus::FAILED : p::TapeStatus::EVENT));

        if (maximum < 18) {
            CHECK(reader.descriptor()->version->data() == prior);
        } else {
            const auto before = reader.constructionUnits();
            accept(reader, protocol({}));
            CHECK(reader.constructionUnits() - before == 10); // Empty delta payload7 plus copied H3.
        }
    }
}

TEST_CASE("valid endpoint extensions are typed refusals while incomplete and extra tails are malformed") {
    const auto initial = protocol({{u"type", u"tape"}});
    p::FrameReader envelope({65536, 65545});
    auto parsed = envelope.feed(initial);
    REQUIRE(parsed.frame);
    const auto base = parsed.frame->payload;

    for (const bool nullUid : {false, true}) {
        auto body = base;
        compact(body, nullUid ? -1 : 0);

        if (!nullUid) {
            compact(body, -1);
            compact(body, 0);
        } // Empty UID, null UTF name, raw ID0.

        p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        CHECK(reader.feed(frame(1, body)).status == p::TapeStatus::FAILED);
        CHECK(reader.diagnostic().error == p::TapeError::UNSUPPORTED);
        CHECK(reader.diagnostic().rejection == p::TapeRejection::DESCRIPTOR);
        body.push_back(std::byte{0});
        p::TapeReader extra(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
        CHECK(extra.feed(frame(1, body)).status == p::TapeStatus::FAILED);
        CHECK(extra.diagnostic().error == p::TapeError::MALFORMED);
    }

    auto body = base;
    compact(body, 0); // Nonnull UID but missing UTF name/ID.
    p::TapeReader truncated(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
    CHECK(truncated.feed(frame(1, body)).status == p::TapeStatus::FAILED);
    CHECK(truncated.diagnostic().error == p::TapeError::MALFORMED);
}

TEST_CASE("lossless catalog formatter owns all nineteen tags and counts bytes before row expansion") {
    constexpr std::array ids{0, 1, 2, 3, 4, 8, 9, 10, 24, 40, 56, 72, 88, 104, 120, 137, 152, 233, 249};
    c::LocalRecord local{0, u"All", false, {}, {}};
    p::StreamDataRecord record;
    record.localId = 0;
    record.senderId = 7;

    for (const auto id : ids) {
        local.fields.push_back({id});
        const std::u16string name{u'F', static_cast<char16_t>(u'A' + local.lookups.size())};
        local.lookups.push_back({name, local.lookups.size()});
        record.typedValues.push_back({static_cast<c::SerialFieldType>(id), 0});
    }

    const std::array declarations{local};
    auto schema = c::LocalSchema::create(declarations, broadLimits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    REQUIRE(schema);
    record.context = schema->recordContext(0);
    record.typedValues[6].bytes = std::vector{std::byte{0}, std::byte{0xff}};
    record.typedValues[7].units = std::u16string{};
    record.typedValues[15].units = std::u16string{u'A', 0, u'\n'};
    record.typedValues[17].bytes = std::vector<std::byte>{};
    record.symbol.eventFlags = 95;
    record.removalTime = 0;
    view::Formatter formatter(view::OutputFormat::TSV, view::DisplayProfile::BINARY_CATALOG_V1);
    const auto row = formatter.catalogRecord(record, *schema, 1, 0, 65536, nullptr, 1);
    CHECK(std::count(row.begin(), row.end(), '\t') == 13);
    CHECK(row.find("9:0:-:00ff") != std::string::npos);
    CHECK(row.find("137:0:00410000000a:-") != std::string::npos);
    CHECK(row.find("233:0:-:empty") != std::string::npos);
    CHECK(row.find("249:0:-:null") != std::string::npos);
    CHECK(row.ends_with("\t95\t0\n"));
    CHECK_THROWS_AS(formatter.catalogRecord(record, *schema, 1, 0, row.size() - 1, nullptr, 1), view::OutputLimit);
    CHECK(formatter.catalogRecord(record, *schema, 1, 0, row.size(), nullptr, 1) == row);
    record.typedValues[6].raw = 1;
    CHECK_THROWS_AS(formatter.catalogRecord(record, *schema, 1, 0, 65536, nullptr, 1), std::invalid_argument);
}

TEST_CASE("actual FileWriter normal and headerless NONE MESSAGE captures retain every family once") {
    for (const bool headerless : {false, true}) {
        for (const auto time : {std::string("NONE"), std::string("MESSAGE")}) {
            for (const auto family : {5, 10, 15, 20}) {
                const auto path = fixtureRoot / (headerless ? "headerless" : "normal") /
                                  ("writer-" + time + "-" + std::to_string(family) + ".qtp");
                INFO(path.generic_string());
                std::ifstream file(path, std::ios::binary);
                REQUIRE(file.good());
                const std::string bytes{std::istreambuf_iterator<char>(file), {}};
                auto input = std::as_bytes(std::span(bytes.data(), bytes.size()));
                p::TapeReader reader(broadLimits(), p::TapeDiscovery::BINARY_CATALOG_V1);
                std::size_t records = 0;

                while (!input.empty()) {
                    const auto result = reader.feed(input);
                    REQUIRE(result.accepted > 0);
                    input = input.subspan(result.accepted);
                    REQUIRE(result.status != p::TapeStatus::FAILED);

                    for (const auto &record : result.records) {
                        REQUIRE(result.frame);
                        CHECK(result.frame->type == family);
                        REQUIRE(record.typedValues.size() == 1);
                        CHECK(record.typedValues[0].raw == 913);
                        CHECK(record.symbol.units == u"FLAGS");
                        CHECK(record.symbol.eventFlags == 0);
                        CHECK_FALSE(record.removalTime);
                        REQUIRE(result.timing);

                        if (time == "MESSAGE") {
                            CHECK(result.timing->timestampMs == 305419896000LL);
                            CHECK(result.timing->eventTimeSequence == 0x1234567800000000LL);
                        } else {
                            CHECK_FALSE(result.timing->timestampMs);
                        }

                        ++records;
                    }
                }

                CHECK(records == 1);
                CHECK(reader.finish() == p::TapeStatus::FINISHED);
            }
        }
    }
}

TEST_CASE("catalog formatter checks expected schema provenance shape and IDs before zero output admission") {
    const std::array<c::LocalRecord, 2> declarations{
        {{0, u"Authoritative", false, {{8}}, {{u"V", 0}}}, {1, u"Other", false, {{8}}, {{u"V", 0}}}}};
    auto expected =
        c::LocalSchema::create(declarations, broadLimits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    auto stale = c::LocalSchema::create(declarations, broadLimits().context.schema, c::LocalProfile::BINARY_FIELDS_V1);
    auto ordinary =
        c::LocalSchema::create(declarations, broadLimits().context.schema, c::LocalProfile::EXTENDED_FIELDS_V1);
    REQUIRE(expected);
    REQUIRE(stale);
    REQUIRE(ordinary);
    p::StreamDataRecord record;
    record.localId = 0;
    record.senderId = 7;
    record.context = expected->recordContext(0);
    record.typedValues = {{c::SerialFieldType::COMPACT_INT, 42}};
    view::Formatter formatter(view::OutputFormat::TSV, view::DisplayProfile::BINARY_CATALOG_V1);
    CHECK_THROWS_AS(formatter.record(record, u"Spoofed", 1, 0, 0, nullptr, 1), std::invalid_argument);
    CHECK_THROWS_AS(formatter.catalogRecord(record, *expected, 1, 0, 0, nullptr, 1), view::OutputLimit);
    CHECK_THROWS_AS(formatter.catalogRecord(record, *stale, 1, 0, 0, nullptr, 1), std::invalid_argument);
    CHECK_THROWS_AS(formatter.catalogRecord(record, *ordinary, 1, 0, 0, nullptr, 1), std::invalid_argument);
    const auto row = formatter.catalogRecord(record, *expected, 1, 0, 65536, nullptr, 1);
    CHECK(row.find("0041007500740068006f00720069007400610074006900760065") != std::string::npos);

    for (const auto id : {-1, 1, 999}) {
        auto invalid = record;
        invalid.localId = id;
        CHECK_THROWS_AS(formatter.catalogRecord(invalid, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    }

    auto invalid = record;
    invalid.typedValues.clear();
    CHECK_THROWS_AS(formatter.catalogRecord(invalid, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    invalid = record;
    invalid.typedValues[0].type = c::SerialFieldType::INT;
    CHECK_THROWS_AS(formatter.catalogRecord(invalid, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    invalid = record;
    invalid.typedValues[0].units = u"unexpected";
    CHECK_THROWS_AS(formatter.catalogRecord(invalid, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    invalid = record;
    invalid.values = {42};
    CHECK_THROWS_AS(formatter.catalogRecord(invalid, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    view::Formatter old(view::OutputFormat::TSV, view::DisplayProfile::WIRE_SCHEMA_V1);
    CHECK_THROWS_AS(old.catalogRecord(record, *expected, 1, 0, 0, nullptr, 1), std::invalid_argument);
    const p::TapeTiming zero{0, 0};
    const auto timed = formatter.catalogRecord(record, *expected, 1, 0, 65536, &zero, 1);
    CHECK(timed.ends_with("\t0\t0\t0\t0\t-\n"));
}

TEST_CASE("catalog HUMAN names every accepted control and family without changing old frame presentation") {
    view::Formatter human(view::OutputFormat::HUMAN, view::DisplayProfile::BINARY_CATALOG_V1);
    view::Formatter tsv(view::OutputFormat::TSV, view::DisplayProfile::BINARY_CATALOG_V1);

    for (const auto &[id, name] :
         {std::pair{0, "HEARTBEAT"}, std::pair{1, "DESCRIBE_PROTOCOL"}, std::pair{2, "DESCRIBE_RECORDS"},
          std::pair{3, "DESCRIBE_RESERVED"}, std::pair{5, "RAW_DATA"}, std::pair{10, "TICKER_DATA"},
          std::pair{15, "STREAM_DATA"}, std::pair{20, "HISTORY_DATA"}}) {
        const p::Frame event{p::FrameKind::MESSAGE, id, {}, 0, 2};
        const auto text = human.tapeFrame(event, 0, 4096, p::TapeMode::BINARY_CATALOG_V1);
        CHECK(text.find(std::string(": ") + name + " (") != std::string::npos);
        CHECK(text.find("UNSUPPORTED") == std::string::npos);
        CHECK(tsv.tapeFrame(event, 0, 4096, p::TapeMode::BINARY_CATALOG_V1) == tsv.frame(event, 0, 4096));

        if (id == 0 || id == 5 || id == 10 || id == 20 || id == 3) {
            CHECK(human.tapeFrame(event, 0, 4096, p::TapeMode::LEGACY) == human.frame(event, 0, 4096));
        }
    }

    const p::Frame empty{p::FrameKind::EMPTY_HEARTBEAT, std::nullopt, {}, 0, 1};
    CHECK(human.tapeFrame(empty, 0, 4096, p::TapeMode::BINARY_CATALOG_V1) == human.frame(empty, 0, 4096));
}

/** Run the fixed native regression executable with an explicit governed fixture root. */
int main(int argc, char **argv) {
    if (argc < 2) {
        return 2;
    }

    fixtureRoot = argv[1];
    doctest::Context context;
    context.applyCommandLine(argc - 1, argv + 1);

    return context.run();
}
