// Copyright (c) 2026 ttldtor.
// SPDX-License-Identifier: BSL-1.0
#define DOCTEST_CONFIG_IMPLEMENT
#include "indexed_test_support.hpp"
#include "sequence_test_seam.hpp"
#include "tape_sequence.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
namespace ti = org::ttldtor::qdcxx::tape_inspect;
std::filesystem::path standardFixtures;
std::filesystem::path generatedRoot;

/** Own only a newly created test directory and the exact files written through this object. */
struct Fixture {
    std::filesystem::path directory;
    std::vector<std::filesystem::path> files;

    Fixture() {
        std::filesystem::create_directories(generatedRoot);
        const auto ticks = std::chrono::steady_clock::now().time_since_epoch().count();

        directory = generatedRoot / ("owned-" + std::to_string(ticks));
        REQUIRE(std::filesystem::create_directory(directory));
    }

    ~Fixture() {
        std::error_code error;

        for (const auto &file : files) {
            std::filesystem::remove(file, error);
        }

        std::filesystem::remove(directory, error);
    }

    /** Create one owned binary file without replacing preexisting content.
     * @param name New relative filename in this fixture's unique directory.
     * @param bytes Exact finite file content.
     * @return Owning native path, valid until fixture destruction.
     */
    std::filesystem::path write(const std::filesystem::path &name, std::string_view bytes) {
        const auto path = directory / name;

        REQUIRE_FALSE(std::filesystem::exists(path));
        files.push_back(path);
        std::ofstream output(path, std::ios::binary);

        REQUIRE(output.is_open());
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        REQUIRE_FALSE(output.fail());

        return path;
    }
};

/** Convert owned designed wire bytes to a binary file value.
 * @param bytes Borrowed byte vector.
 * @return Owning exact bytes, with no text conversion.
 */
std::string binary(const std::vector<std::byte> &bytes) {
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

/** Build a finite initial descriptor using existing canonical primitive helpers.
 * @param time Explicit token; nullopt deliberately omits the producer-default NONE property.
 * @return Owning type1 envelope.
 */
std::vector<std::byte> header(std::optional<std::u16string_view> time) {
    using indexed_test::compact;
    using indexed_test::text;
    std::vector<std::byte> body{std::byte{'D'}, std::byte{'X'}, std::byte{'P'}, std::byte{'3'}};

    compact(body, time ? 3 : 2);
    text(body, u"type");
    text(body, u"tape");
    text(body, u"opt");
    text(body, u"");

    if (time) {
        text(body, u"time");
        text(body, *time);
    }

    compact(body, 1);
    compact(body, 15);
    text(body, u"STREAM_DATA");
    compact(body, 0);
    compact(body, 0);

    return indexed_test::frame(1, body);
}

/** Create one independent source with a real data record and optional explicit timestamp assignment.
 * @param fixture Exact owned-file lifetime.
 * @param policy Explicit time profile.
 * @param value Ordinary field value, independent from metadata.
 * @param omitTime Omit descriptor time for the focused NONE-default and refusal checks.
 * @param heartbeat Include timestamp-bearing MESSAGE metadata, even when zero.
 * @param forceHeartbeat Deliberately inject the same metadata under another policy for refusal tests.
 * @param malformedTail Append an incomplete second record in the same complete data envelope.
 * @param emptyData Use a valid empty data envelope instead of record bytes.
 * @param dataType Selected finite data-family ID for empty-frame checkpoint tests.
 * @return Fully resolved owning occurrence, without discovery or inferred companion names.
 */
ti::TapeSegmentSpec source(Fixture &fixture, ti::SourceTimePolicy policy, std::int64_t value = 913,
                           bool omitTime = false, bool heartbeat = true, bool forceHeartbeat = false,
                           bool malformedTail = false, bool emptyData = false, std::int32_t dataType = 15) {
    using indexed_test::compact;
    using indexed_test::p::WireRecordDescription;
    const auto token = policy == ti::SourceTimePolicy::NONE      ? u"NONE"
                       : policy == ti::SourceTimePolicy::MESSAGE ? u"MESSAGE"
                       : policy == ti::SourceTimePolicy::FIELD   ? u"FIELD"
                       : policy == ti::SourceTimePolicy::LONG    ? u"LONG"
                                                                 : u"TEXT";
    auto bytes = header(omitTime ? std::optional<std::u16string_view>{} : token);
    WireRecordDescription description{91, u"Sample", {{u"Value", 8}}};

    if (policy == ti::SourceTimePolicy::FIELD) {
        description.fields.insert(description.fields.begin(), {{u"EventTime", 56}, {u"EventSequence", 72}});
    }

    const std::array descriptions{description};
    const auto names = indexed_test::descriptions(descriptions);

    bytes.insert(bytes.end(), names.begin(), names.end());

    if (heartbeat && (policy == ti::SourceTimePolicy::MESSAGE || forceHeartbeat)) {
        std::vector<std::byte> body;

        compact(body, 1);
        compact(body, 0);
        const auto mark = indexed_test::frame(0, body);

        bytes.insert(bytes.end(), mark.begin(), mark.end());
    }

    const auto dataPosition = bytes.size();
    std::vector<std::byte> body{std::byte{0xfe}};

    compact(body, 91);

    if (policy == ti::SourceTimePolicy::FIELD) {
        compact(body, 0);
        compact(body, 0);
    }

    compact(body, value);

    if (malformedTail) {
        body.push_back(std::byte{0xfe});
        compact(body, 91);
    }

    if (emptyData) {
        body.clear();
    }

    const auto record = indexed_test::frame(dataType, body);

    bytes.insert(bytes.end(), record.begin(), record.end());
    ti::TapeSegmentSpec segment;
    const auto ordinal = fixture.files.size();

    segment.path = fixture.write(std::filesystem::path(u8"источник-") += std::to_string(ordinal), binary(bytes));
    segment.limits.tape = indexed_test::limits();
    segment.limits.tape.data.ownedFieldTimeRecords = 10000;
    segment.limits.output = 65536;

    if (policy == ti::SourceTimePolicy::LONG || policy == ti::SourceTimePolicy::TEXT) {
        const auto time = policy == ti::SourceTimePolicy::LONG ? "0:" : "19700101-000000.000+0000:";
        const auto path = fixture.write("time-" + std::to_string(ordinal), time + std::to_string(dataPosition) + "\n");

        segment.companion = ti::TapeCompanionSpec{path};
    }

    return segment;
}

/** Supply generous explicit aggregate capacities; never infer a source-count multiplier in the API.
 * @param segment Fully resolved per-source values.
 * @param policy Explicit common timing profile.
 * @return Finite options for at most four selected occurrences.
 */
ti::TapeSequenceOptions optionsFor(const ti::TapeSegmentSpec &segment, ti::SourceTimePolicy policy) {
    ti::TapeSequenceOptions options;

    options.time = policy;
    options.aggregate.tape = segment.limits.tape;
    options.aggregate.dataContainer = segment.containerLimits;

    if (segment.companion) {
        options.aggregate.companionContainer = segment.companion->containerLimits;
        options.aggregate.scanner = segment.companion->scannerLimits;
    }

    options.aggregate.chunkBytes = segment.limits.chunkSize;
    options.aggregate.outputBytes = segment.limits.output;
    options.aggregate.eventUnits = segment.limits.eventUnits;
    options.aggregate.physicalProbes = segment.companion ? 8 : 4;
    options.aggregate.inflatedProbes = options.aggregate.physicalProbes;
    // The focused tests select at most four occurrences with identical configured capacities.
    // Each listed scalar is multiplied explicitly; production admission independently checks all fields.
    constexpr std::size_t COPIES = 4;
#define CAPACITY(field) options.aggregate.field *= COPIES
    CAPACITY(tape.frame.maximumBodyBytes);
    CAPACITY(tape.frame.maximumPendingBytes);
    CAPACITY(tape.inputBytes);
    CAPACITY(tape.frames);
    CAPACITY(tape.descriptor.properties);
    CAPACITY(tape.descriptor.messages);
    CAPACITY(tape.descriptor.text.bodyBytes);
    CAPACITY(tape.descriptor.text.codeUnits);
    CAPACITY(tape.descriptor.textUnits);
    CAPACITY(tape.context.schema.records);
    CAPACITY(tape.context.schema.fields);
    CAPACITY(tape.context.schema.lookups);
    CAPACITY(tape.context.schema.nameUnits);
    CAPACITY(tape.context.schema.totalNameUnits);
    CAPACITY(tape.context.descriptions);
    CAPACITY(tape.context.bindings.fields);
    CAPACITY(tape.context.bindings.nameUnits);
    CAPACITY(tape.context.bindings.totalNameUnits);
    CAPACITY(tape.constructionUnits);
    CAPACITY(tape.timingEvents);
    CAPACITY(tape.snapshotUnits);
    CAPACITY(tape.data.payloadBytes);
    CAPACITY(tape.data.processedRecords);
    CAPACITY(tape.data.wireFields);
    CAPACITY(tape.data.emittedRecords);
    CAPACITY(tape.data.emittedValues);
    CAPACITY(tape.data.ownedSymbolUnits);
    CAPACITY(tape.data.symbol.inputBytes);
    CAPACITY(tape.data.symbol.codeUnits);
    CAPACITY(tape.data.symbol.stateUnits);
    CAPACITY(tape.data.fieldString.encodedBytes);
    CAPACITY(tape.data.fieldString.codeUnits);
    CAPACITY(tape.data.fieldBytes);
    CAPACITY(tape.data.fieldUnits);
    CAPACITY(tape.data.ownedFieldUnits);
    CAPACITY(tape.data.fieldPayloadEncodedBytes);
    CAPACITY(tape.data.fieldPayloadBytes);
    CAPACITY(tape.data.payloadEncodedBytes);
    CAPACITY(tape.data.payloadWorkBytes);
    CAPACITY(tape.data.ownedPayloadBytes);
    CAPACITY(tape.data.ownedFieldTimeRecords);
    CAPACITY(chunkBytes);
    CAPACITY(outputBytes);
    CAPACITY(eventUnits);
#define CONTAINER(role)                                                                                                \
    CAPACITY(role.compressedBytes);                                                                                    \
    CAPACITY(role.inflatedBytes);                                                                                      \
    CAPACITY(role.expansionRatio);                                                                                     \
    CAPACITY(role.ratioSlack);                                                                                         \
    CAPACITY(role.gzipMembers);                                                                                        \
    CAPACITY(role.zipEntries);                                                                                         \
    CAPACITY(role.headerBytes);                                                                                        \
    CAPACITY(role.totalHeaderBytes);                                                                                   \
    CAPACITY(role.workBytes);                                                                                          \
    CAPACITY(role.inflateCalls);                                                                                       \
    CAPACITY(role.retainedBytes);                                                                                      \
    CAPACITY(role.inflaterBytes);                                                                                      \
    CAPACITY(role.chunkBytes)
    CONTAINER(dataContainer);
    CONTAINER(companionContainer);
    CAPACITY(scanner.inputBytes);
    CAPACITY(scanner.lineBytes);
    CAPACITY(scanner.lines);
    CAPACITY(scanner.marks);
    CAPACITY(scanner.workBytes);
    CAPACITY(scanner.retainedBytes);
#undef CONTAINER
#undef CAPACITY

    return options;
}

/** Split completed encoded rows without retaining views into a temporary string.
 * @param text Owning sink snapshot.
 * @param label Exact static row label.
 * @return Owning rows with that label.
 */
std::vector<std::string> rows(const std::string &text, std::string_view label) {
    std::istringstream input(text);
    std::vector<std::string> result;
    std::string line;

    while (std::getline(input, line)) {
        if (line.starts_with(std::string(label) + "\t")) {
            result.push_back(line);
        }
    }

    return result;
}

/** Observable synchronous sink with bounded record-stop and two distinct final-flush fault seams. */
class Sink : public std::stringbuf {
    public:
    std::stop_source *stop = nullptr;
    std::size_t failFlush = 0;
    std::size_t flushes = 0;
    bool failRecord = false;

    protected:
    std::streamsize xsputn(const char *bytes, std::streamsize count) override {
        const bool record = std::string_view(bytes, static_cast<std::size_t>(count)).starts_with("wire-data-v9\t");

        if (record && failRecord) {
            return 0;
        }

        const auto written = std::stringbuf::xsputn(bytes, count);

        if (record && stop) {
            stop->request_stop();
        }

        return written;
    }

    int sync() override {
        return ++flushes == failFlush ? -1 : std::stringbuf::sync();
    }
};
} // namespace

TEST_CASE("finite owner accepts all five independent explicit policies and assigned zero") {
    for (const auto policy : {ti::SourceTimePolicy::NONE, ti::SourceTimePolicy::MESSAGE, ti::SourceTimePolicy::FIELD,
                              ti::SourceTimePolicy::LONG, ti::SourceTimePolicy::TEXT}) {
        Fixture fixture;
        const auto first = source(fixture, policy, 913, policy == ti::SourceTimePolicy::NONE);
        const auto second = source(fixture, policy, 914, policy == ti::SourceTimePolicy::NONE);
        const std::array selected{first, second, first};
        const auto options = optionsFor(first, policy);
        std::ostringstream output;
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;
        const auto result = ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test);

        INFO(diagnostic.str());
        REQUIRE(result.primary.exitCode == 0);
        CHECK(result.status == ti::SequenceStatus::COMPLETE);
        CHECK(result.counters.sources == 3);
        CHECK(result.counters.emitted == 3);
        CHECK(rows(output.str(), "wire-data-v9").size() == 3);
        CHECK(rows(output.str(), "source-policy").size() == 3);
        CHECK(rows(output.str(), "source-end").size() == 3);
        CHECK(rows(output.str(), "complete-v9").size() == 1);
        CHECK(test.dataCloses == 3);
        CHECK(test.timeCloses == (first.companion ? 3 : 0));
        CHECK(test.liveAdapters == 0);
    }
}

TEST_CASE("missing explicit non-NONE declarations and timestamp assignments never inherit") {
    for (const auto policy : {ti::SourceTimePolicy::MESSAGE, ti::SourceTimePolicy::FIELD, ti::SourceTimePolicy::LONG,
                              ti::SourceTimePolicy::TEXT}) {
        Fixture fixture;
        const auto first = source(fixture, policy);
        const auto missing = source(fixture, policy, 914, true);
        const std::array selected{first, missing, first};
        const auto options = optionsFor(first, policy);
        std::ostringstream output;
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;
        const auto result = ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test);

        CHECK(result.status == ti::SequenceStatus::FAILED);
        CHECK(result.primary.exitCode == 4);
        CHECK(result.primary.sourceOrdinal == 1);
        CHECK(test.dataOpens == 2);
        CHECK(rows(output.str(), "wire-data-v9").size() == 1);
        CHECK(rows(output.str(), "complete-v9").empty());
    }

    Fixture fixture;
    const auto first = source(fixture, ti::SourceTimePolicy::MESSAGE);
    const auto missing = source(fixture, ti::SourceTimePolicy::MESSAGE, 914, false, false);
    const std::array selected{first, missing, first};
    std::ostringstream output;
    std::ostringstream diagnostic;
    const auto result =
        ti::inspectSequence(selected, output, diagnostic, optionsFor(first, ti::SourceTimePolicy::MESSAGE));

    CHECK(result.primary.exitCode == 4);
    CHECK(result.primary.staticReason == "missing-time-assignment");
    CHECK(rows(output.str(), "wire-data-v9").size() == 1);

    const auto overlay = source(fixture, ti::SourceTimePolicy::NONE, 915, true, true, true);
    const std::array overlaySelection{overlay};
    std::ostringstream overlayOutput;
    std::ostringstream overlayDiagnostic;
    const auto refused = ti::inspectSequence(overlaySelection, overlayOutput, overlayDiagnostic,
                                             optionsFor(overlay, ti::SourceTimePolicy::NONE));

    CHECK(refused.primary.exitCode == 4);
    CHECK(rows(overlayOutput.str(), "wire-data-v9").empty());
}

TEST_CASE("physical pair cleanup and both final flushes determine aggregate completion") {
    Fixture fixture;
    const auto first = source(fixture, ti::SourceTimePolicy::LONG);
    const std::array selected{first, first};
    const auto options = optionsFor(first, ti::SourceTimePolicy::LONG);

    SUBCASE("partial open performs no read") {
        std::ostringstream output;
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;

        test.failTimeOpen = 0;
        const auto result = ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test);

        CHECK(result.primary.exitCode == 3);
        CHECK(test.dataCloses == 1);
        CHECK(test.timeCloses == 0);
        CHECK(test.reads == 0);
    }

    SUBCASE("both close failures are retained after adapter destruction") {
        std::ostringstream output;
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;

        test.failTimeClose = 0;
        test.failDataClose = 0;
        const auto result = ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test);

        CHECK(result.primary.exitCode == 3);
        CHECK(result.primary.layer == "time");
        CHECK(result.timeClose.has_value());
        CHECK(result.dataClose.has_value());
        CHECK(test.dataOpens == 1);
        CHECK(test.dataCloses == 1);
        CHECK(test.timeCloses == 1);
        CHECK(test.liveAdapters == 0);
        CHECK((test.cleanupOrder == std::array<unsigned, 4>{1, 2, 3, 4}));
        CHECK(rows(output.str(), "source-end").empty());
    }

    for (const auto failedFlush : {std::size_t{1}, std::size_t{2}}) {
        Sink sink;

        sink.failFlush = failedFlush;
        std::ostream output(&sink);
        std::ostringstream diagnostic;
        const auto result = ti::inspectSequence(selected, output, diagnostic, options);

        CHECK(result.status == ti::SequenceStatus::FAILED);
        CHECK(result.primary.exitCode == 3);
        CHECK(result.flush.has_value());
        CHECK(rows(sink.str(), "source-end").size() == 2);
        CHECK(rows(sink.str(), "complete-v9").size() == (failedFlush == 2 ? 1 : 0));
    }
}

TEST_CASE("sink stop and failure do not open the next source") {
    Fixture fixture;
    const auto first = source(fixture, ti::SourceTimePolicy::LONG);
    const std::array selected{first, first};
    const auto options = optionsFor(first, ti::SourceTimePolicy::LONG);

    for (const bool stopAtRecord : {true, false}) {
        Sink sink;
        std::stop_source stop;

        sink.stop = stopAtRecord ? &stop : nullptr;
        sink.failRecord = !stopAtRecord;
        std::ostream output(&sink);
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;
        const auto result =
            ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test, stop.get_token());

        CHECK(result.primary.exitCode == (stopAtRecord ? 7 : 3));
        CHECK(test.dataOpens == 1);
        CHECK(test.timeOpens == 1);
        CHECK(test.dataCloses == 1);
        CHECK(test.timeCloses == 1);
        CHECK(test.afterPrimaryReads == 0);
        CHECK(rows(sink.str(), "source-end").empty());
        CHECK(rows(sink.str(), "complete-v9").empty());
    }
}

TEST_CASE("already detected parser failure precedes fallible completed-prefix rendering") {
    Fixture fixture;
    const auto malformed = source(fixture, ti::SourceTimePolicy::NONE, 913, true, false, false, true);
    const std::array selected{malformed, malformed};
    const auto options = optionsFor(malformed, ti::SourceTimePolicy::NONE);

    for (const bool stopAtRecord : {false, true}) {
        Sink sink;
        std::stop_source stop;

        sink.stop = stopAtRecord ? &stop : nullptr;
        sink.failRecord = !stopAtRecord;
        std::ostream output(&sink);
        std::ostringstream diagnostic;
        ti::detail::SequenceFileTest test;
        const auto result =
            ti::detail::inspectSequenceForTest(selected, output, diagnostic, options, test, stop.get_token());

        INFO(diagnostic.str());
        CHECK(result.status == ti::SequenceStatus::FAILED);
        CHECK(result.primary.exitCode == 5);
        CHECK(result.primary.layer == "parser");
        CHECK(result.primary.frameStart.has_value());
        CHECK(result.primary.payloadStart.has_value());
        REQUIRE(result.primary.physicalFetched.has_value());
        REQUIRE(result.primary.physicalConsumed.has_value());
        REQUIRE(result.primary.logicalProduced.has_value());
        CHECK(*result.primary.physicalFetched == std::filesystem::file_size(malformed.path));
        CHECK(result.primary.physicalConsumed == result.primary.physicalFetched);
        CHECK(result.primary.logicalProduced == result.primary.physicalFetched);
        CHECK(test.dataOpens == 1);
        CHECK(test.dataCloses == 1);
        CHECK(test.afterPrimaryReads == 0);
        CHECK(rows(sink.str(), "wire-data-v9").size() == (stopAtRecord ? 1 : 0));
        CHECK(rows(sink.str(), "complete-v9").empty());
    }

    std::ostringstream baselineOutput;
    std::ostringstream baselineDiagnostic;
    const auto baseline = ti::inspectSequence(selected, baselineOutput, baselineDiagnostic, options);

    REQUIRE(baseline.primary.exitCode == 5);
    const auto baselineText = baselineOutput.str();
    const auto sourceStart = baselineText.find("source-begin\t");
    const auto recordStart = baselineText.find("wire-data-v9\t");
    const auto recordEnd = baselineText.find('\n', recordStart);

    REQUIRE(sourceStart != std::string::npos);
    REQUIRE(recordStart != std::string::npos);
    REQUIRE(recordEnd != std::string::npos);
    auto limited = selected;

    limited[0].limits.output = recordEnd - sourceStart;
    limited[1].limits.output = limited[0].limits.output;
    std::ostringstream limitedOutput;
    std::ostringstream limitedDiagnostic;
    const auto refused = ti::inspectSequence(limited, limitedOutput, limitedDiagnostic, options);

    CHECK(refused.primary.exitCode == 5);
    CHECK(refused.primary.layer == "parser");
    CHECK(rows(limitedOutput.str(), "wire-data-v9").empty());
}

TEST_CASE("parser EOF failure retains known source counters after adapter release") {
    Fixture fixture;
    const auto malformed = source(fixture, ti::SourceTimePolicy::NONE, 913, true, false);
    const auto size = std::filesystem::file_size(malformed.path);

    REQUIRE(size > 0);
    std::filesystem::resize_file(malformed.path, size - 1);
    const std::array selected{malformed};
    std::ostringstream output;
    std::ostringstream diagnostic;
    ti::detail::SequenceFileTest test;
    const auto result = ti::detail::inspectSequenceForTest(selected, output, diagnostic,
                                                           optionsFor(malformed, ti::SourceTimePolicy::NONE), test);

    CHECK(result.primary.exitCode == 5);
    CHECK(result.primary.layer == "parser");
    REQUIRE(result.primary.physicalFetched.has_value());
    REQUIRE(result.primary.physicalConsumed.has_value());
    REQUIRE(result.primary.logicalProduced.has_value());
    CHECK(*result.primary.physicalFetched == size - 1);
    CHECK(result.primary.physicalConsumed == result.primary.physicalFetched);
    CHECK(result.primary.logicalProduced == result.primary.physicalFetched);
    CHECK(test.dataCloses == 1);
    CHECK(test.liveAdapters == 0);
}

TEST_CASE("source timing is checked before publishing empty or nonempty data-family frames") {
    for (const auto policy : {ti::SourceTimePolicy::MESSAGE, ti::SourceTimePolicy::LONG, ti::SourceTimePolicy::TEXT}) {
        for (const bool empty : {false, true}) {
            for (const auto type : {5, 10, 15, 20}) {
                Fixture fixture;
                auto segment = source(fixture, policy, 913, false, false, false, false, empty, type);

                if (segment.companion) {
                    std::ofstream time(segment.companion->path, std::ios::binary | std::ios::trunc);

                    REQUIRE(time.is_open());
                    time.close();
                    REQUIRE_FALSE(time.fail());
                }

                const std::array selected{segment};
                std::ostringstream output;
                std::ostringstream diagnostic;
                const auto result = ti::inspectSequence(selected, output, diagnostic, optionsFor(segment, policy));

                INFO(diagnostic.str());
                CHECK(result.primary.exitCode == 4);
                CHECK(rows(output.str(), "wire-data-v9").empty());

                for (const auto &row : rows(output.str(), "frame")) {
                    std::istringstream input(row);
                    std::string cell;

                    for (unsigned column = 0; column <= 4; ++column) {
                        REQUIRE(static_cast<bool>(std::getline(input, cell, '\t')));
                    }

                    CHECK(cell != std::to_string(type));
                }
            }
        }
    }
}

TEST_CASE("governed real standard-event captures remain independently replayable") {
    for (const auto pin : {"baseline", "current"}) {
        ti::TapeSegmentSpec segment;

        segment.path = standardFixtures / pin / "none.qtp";
        segment.limits.tape.snapshotUnits = 16777216;
        segment.limits.output = 1048576;
        const auto options = optionsFor(segment, ti::SourceTimePolicy::NONE);
        std::ostringstream once;
        std::ostringstream diagnostic;
        const std::array one{segment};
        const auto baseline = ti::inspectSequence(one, once, diagnostic, options);

        INFO(diagnostic.str());
        REQUIRE(baseline.primary.exitCode == 0);
        REQUIRE(baseline.counters.emitted > 0);
        const std::array repeated{segment, segment};
        std::ostringstream twice;
        std::ostringstream repeatedDiagnostic;
        const auto replay = ti::inspectSequence(repeated, twice, repeatedDiagnostic, options);

        REQUIRE(replay.primary.exitCode == 0);
        CHECK(replay.counters.emitted == baseline.counters.emitted * 2);
        CHECK(replay.counters.accepted == baseline.counters.accepted * 2);
        CHECK(rows(twice.str(), "complete-v9").size() == 1);
    }
}

/** Run public owner regressions with explicit immutable fixtures and an isolated generated-file parent.
 * @param argc Argument count.
 * @param argv Public container-fixture root, generated-file parent, then optional doctest arguments.
 * @return Doctest status, or usage2 before any file work when explicit roots are absent.
 */
int main(int argc, char **argv) {
    if (argc < 3) {
        return 2;
    }

    standardFixtures = argv[1];
    generatedRoot = argv[2];
    doctest::Context context;

    context.applyCommandLine(argc - 2, argv + 2);

    return context.run();
}
